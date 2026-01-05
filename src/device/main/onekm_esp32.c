/*
 * OneKM ESP32-S3 固件主程序
 *
 * 功能：
 * 1. UART 接收：接收Linux服务器的USB HID报告
 * 2. USB 转发：直接向Windows发送HID报文
 * 3. 无状态管理，无键码转换 - ESP32只做转发
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "soc/uart_periph.h"
#include "esp_rom_gpio.h"
#include "hal/gpio_hal.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

#define TAG "onekm"

// UART 配置 - 使用 UART0 (GPIO43/44) 避免下载器冲突
#define UART_NUM UART_NUM_0
#define UART_TX_PIN GPIO_NUM_43
#define UART_RX_PIN GPIO_NUM_44
static const int UART_BAUD_RATE = 230400;  // 波特率（可修改为230400等）
#define UART_BUF_SIZE 128

// 按钮配置
#define APP_BUTTON GPIO_NUM_0

// 共享数据结构
typedef struct {
    int16_t x;              // X 位移（累积值）
    int16_t y;              // Y 位移（累积值）
    int8_t vertical_wheel;  // 垂直滚轮（累积值）
    int8_t horizontal_wheel; // 水平滚轮（累积值）
    uint8_t buttons;        // 按键位掩码 (bit0=左, bit1=右, bit2=中)
    bool changed;           // 状态变化标志
} mouse_state_t;

// 键盘直接转发状态
typedef struct {
    uint8_t modifiers;     // 修饰键
    uint8_t reserved;      // 保留
    uint8_t keys[6];       // 按键码
    bool changed;          // 状态变化标志
} keyboard_state_t;

// 全局共享变量
static mouse_state_t mouse_state = {0};
static keyboard_state_t keyboard_state = {0};
static SemaphoreHandle_t state_mutex;      // 保护共享状态
static SemaphoreHandle_t hid_update_sem;   // 触发 HID 发送

// 控制状态（LOCAL/REMOTE）
static volatile bool is_remote_mode = false;

/************* USB HID 描述符 ***************/

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

// HID 报告描述符：键盘 + 鼠标
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2))
};

// 字符串描述符
const char* hid_string_descriptor[5] = {
    (char[]){0x09, 0x04},  // 语言：英语
    "OneKM",               // 制造商
    "OneKM Device",        // 产品
    "123456",              // 序列号
    "OneKM HID Interface", // HID 接口
};

// 配置描述符
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/************* TinyUSB 回调函数 ***************/

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

/************* 协议定义（与Linux服务器一致） ***************/
typedef struct {
    uint8_t type;          // 消息类型
    union {
        struct {
            int16_t dx;    // 鼠标X位移
            int16_t dy;    // 鼠标Y位移
        } mouse_move;
        struct {
            uint8_t button; // 鼠标按键（1=左，2=右，3=中）
            uint8_t state;  // 状态（0=释放，1=按下）
            uint8_t padding[2]; // 填充
        } mouse_button;
        struct {
            uint8_t modifiers;     // 修饰键位掩码
            uint8_t reserved;      // 保留（必须为0）
            uint8_t keys[6];       // 最多6个同时按下的按键
        } keyboard;
        struct {
            uint8_t state;  // 控制状态（0=本地，1=远程）
            uint8_t padding[3]; // 填充
        } control;
        struct {
            int16_t vertical;   // 垂直滚轮
            int16_t horizontal; // 水平滚轮
        } mouse_wheel;
    } data;
} __attribute__((packed)) input_message_t;

enum MessageType {
    MSG_MOUSE_MOVE = 0x01,
    MSG_MOUSE_BUTTON = 0x02,
    MSG_KEYBOARD_REPORT = 0x03,
    MSG_SWITCH = 0x04,
    MSG_MOUSE_WHEEL = 0x05
};

/************* UART 接收任务 ***************/
static void uart_receive_task(void *pvParameters)
{
    uint8_t data[UART_BUF_SIZE];
    input_message_t msg;
    size_t bytes_received = 0;

    ESP_LOGI(TAG, "UART receive task started");

    while (1) {
        // 读取 UART 数据
        int len = uart_read_bytes(UART_NUM, data, sizeof(data), 10 / portTICK_PERIOD_MS);

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                // 累加字节到消息缓冲区
                ((uint8_t*)&msg)[bytes_received++] = data[i];

                // 如果接收到了完整的消息
                if (bytes_received == sizeof(input_message_t)) {
                    // 处理消息
                    switch (msg.type) {
                        case MSG_MOUSE_MOVE:
                            xSemaphoreTake(state_mutex, portMAX_DELAY);
                            // 累积鼠标移动（不移除int16_t转换，直接累积）
                            mouse_state.x += msg.data.mouse_move.dx;
                            mouse_state.y += msg.data.mouse_move.dy;
                            mouse_state.changed = true;
                            xSemaphoreGive(state_mutex);
                            xSemaphoreGive(hid_update_sem);
                            ESP_LOGD(TAG, "Mouse move: dx=%d, dy=%d, accumulated: x=%d, y=%d",
                                     msg.data.mouse_move.dx, msg.data.mouse_move.dy,
                                     mouse_state.x, mouse_state.y);
                            break;

                        case MSG_MOUSE_BUTTON:
                            xSemaphoreTake(state_mutex, portMAX_DELAY);
                            if (msg.data.mouse_button.state) {
                                mouse_state.buttons |= (1 << (msg.data.mouse_button.button - 1));
                            } else {
                                mouse_state.buttons &= ~(1 << (msg.data.mouse_button.button - 1));
                            }
                            mouse_state.changed = true;
                            xSemaphoreGive(state_mutex);
                            xSemaphoreGive(hid_update_sem);
                            ESP_LOGD(TAG, "Mouse button: button=%d, state=%d",
                                     msg.data.mouse_button.button, msg.data.mouse_button.state);
                            break;

                        case MSG_MOUSE_WHEEL:
                            xSemaphoreTake(state_mutex, portMAX_DELAY);
                            // Accumulate wheel movement
                            mouse_state.vertical_wheel += msg.data.mouse_wheel.vertical;
                            mouse_state.horizontal_wheel += msg.data.mouse_wheel.horizontal;
                            mouse_state.changed = true;
                            xSemaphoreGive(state_mutex);
                            xSemaphoreGive(hid_update_sem);
                            ESP_LOGI(TAG, "[RECV] MOUSE_WHEEL vertical=%d, horizontal=%d, accumulated: v=%d, h=%d",
                                     msg.data.mouse_wheel.vertical, msg.data.mouse_wheel.horizontal,
                                     mouse_state.vertical_wheel, mouse_state.horizontal_wheel);
                            break;

                        case MSG_KEYBOARD_REPORT:
                            // 直接复制键盘报告
                            xSemaphoreTake(state_mutex, portMAX_DELAY);
                            memcpy(&keyboard_state, &msg.data.keyboard, sizeof(keyboard_state_t) - sizeof(bool));
                            keyboard_state.changed = true;
                            xSemaphoreGive(state_mutex);
                            xSemaphoreGive(hid_update_sem);
                            ESP_LOGD(TAG, "Keyboard report: mod=0x%02X, keys=%d,%d,%d,%d,%d,%d",
                                     msg.data.keyboard.modifiers,
                                     msg.data.keyboard.keys[0], msg.data.keyboard.keys[1],
                                     msg.data.keyboard.keys[2], msg.data.keyboard.keys[3],
                                     msg.data.keyboard.keys[4], msg.data.keyboard.keys[5]);
                            break;

                        case MSG_SWITCH:
                            is_remote_mode = (msg.data.control.state == 1);
                            ESP_LOGI(TAG, "Mode switched: %s", is_remote_mode ? "REMOTE" : "LOCAL");

                            // 重置鼠标状态（清除累积的移动数据）
                            xSemaphoreTake(state_mutex, portMAX_DELAY);
                            mouse_state.x = 0;
                            mouse_state.y = 0;
                            mouse_state.vertical_wheel = 0;
                            mouse_state.horizontal_wheel = 0;
                            mouse_state.changed = false;
                            xSemaphoreGive(state_mutex);

                            // LED 指示
                            if (is_remote_mode) {
                                gpio_set_level(GPIO_NUM_48, 1);
                            } else {
                                gpio_set_level(GPIO_NUM_48, 0);
                            }
                            break;

                        default:
                            ESP_LOGW(TAG, "Unknown message type: %d", msg.type);
                            break;
                    }

                    bytes_received = 0; // 重置接收缓冲区
                }
            }
        }
    }
}

/************* HID 发送任务 ***************/
static void hid_send_task(void *pvParameters)
{
    ESP_LOGI(TAG, "HID send task started");

    while (1) {
        // 等待信号量（由 UART 任务触发）
        if (xSemaphoreTake(hid_update_sem, portMAX_DELAY) == pdTRUE) {
            // 获取互斥锁，读取状态
            xSemaphoreTake(state_mutex, portMAX_DELAY);

            bool kb_changed = keyboard_state.changed;
            bool mouse_changed = mouse_state.changed;

            keyboard_state_t kb_local = keyboard_state;
            mouse_state_t mouse_local = mouse_state;

            // 清除变化标志
            keyboard_state.changed = false;
            mouse_state.changed = false;

            xSemaphoreGive(state_mutex);

            // 发送键盘事件
            if (kb_changed) {
                tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD,
                    kb_local.modifiers, kb_local.keys);
                ESP_LOGV(TAG, "Sent keyboard report");
            }

            // 发送鼠标事件
            if (mouse_changed) {
                // 将int16_t转换为int8_t（TinyUSB API需要int8_t）
                int8_t dx = (int8_t)(mouse_local.x > 127 ? 127 : (mouse_local.x < -128 ? -128 : mouse_local.x));
                int8_t dy = (int8_t)(mouse_local.y > 127 ? 127 : (mouse_local.y < -128 ? -128 : mouse_local.y));
                // 滚轮直接使用int8_t值（无需转换）
                int8_t vertical_wheel = mouse_local.vertical_wheel;
                int8_t horizontal_wheel = mouse_local.horizontal_wheel;

                tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE,
                    mouse_local.buttons, dx, dy, vertical_wheel, horizontal_wheel);
                ESP_LOGI(TAG, "[SEND] HID_MOUSE_REPORT buttons=0x%x dx=%d dy=%d wheel_v=%d wheel_h=%d",
                         mouse_local.buttons, dx, dy, vertical_wheel, horizontal_wheel);

                // 减去已发送的值（保留未发送的部分）
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                mouse_state.x -= dx;
                mouse_state.y -= dy;
                mouse_state.vertical_wheel = 0;
                mouse_state.horizontal_wheel = 0;
                // 如果已经发送完所有累积值，清除changed标志
                if ((mouse_state.x == 0 && mouse_state.y == 0) || !is_remote_mode) {
                    mouse_state.changed = false;
                }
                xSemaphoreGive(state_mutex);
            }
        }
    }
}

/************* 主程序 ***************/
void app_main(void)
{
    ESP_LOGI(TAG, "=== OneKM ESP32-S3 Firmware ===");
    ESP_LOGI(TAG, "Mode: Pure forwarding (no state management)");

    // 1. 初始化 GPIO
    // BOOT 按钮
    const gpio_config_t boot_button_config = {
        .pin_bit_mask = BIT64(APP_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_button_config));

    // LED 指示灯（GPIO48，部分开发板可用）
    const gpio_config_t led_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_48),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = false,
        .pull_down_en = false,
    };
    gpio_config(&led_config);
    gpio_set_level(GPIO_NUM_48, 0);

    // 2. 初始化 UART0 (GPIO43/44)
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 安装 UART 驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

    // 手动设置引脚映射（绕过默认的 USB CDC 映射）
    esp_rom_gpio_connect_out_signal(UART_TX_PIN, UART_PERIPH_SIGNAL(0, SOC_UART_TX_PIN_IDX), false, false);
    esp_rom_gpio_connect_in_signal(UART_RX_PIN, UART_PERIPH_SIGNAL(0, SOC_UART_RX_PIN_IDX), false);

    // 配置 GPIO
    gpio_set_direction(UART_TX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(UART_RX_PIN, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "UART0 initialized: baud=%d, TX=GPIO%d, RX=GPIO%d", UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    // 3. 创建信号量和互斥锁
    state_mutex = xSemaphoreCreateMutex();
    hid_update_sem = xSemaphoreCreateBinary();

    if (state_mutex == NULL || hid_update_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore/mutex");
        return;
    }

    // 4. 初始化 USB
    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = hid_configuration_descriptor;
#endif

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    // 5. 创建任务
    // UART 接收任务（Core 0）
    xTaskCreatePinnedToCore(
        uart_receive_task,      // 任务函数
        "uart_receive",         // 任务名
        4096,                   // 堆栈大小
        NULL,                   // 参数
        5,                      // 优先级
        NULL,                   // 任务句柄
        0                       // Core 0
    );

    // HID 发送任务（Core 1）
    xTaskCreatePinnedToCore(
        hid_send_task,          // 任务函数
        "hid_send",             // 任务名
        4096,                   // 堆栈大小
        NULL,                   // 参数
        5,                      // 优先级
        NULL,                   // 任务句柄
        1                       // Core 1
    );

    ESP_LOGI(TAG, "All tasks created");
    ESP_LOGI(TAG, "Waiting for USB connection...");

    // 6. 主循环：监控状态
    while (1) {
        if (tud_mounted()) {
            // USB 已连接，LED 闪烁指示
            static bool led_state = false;
            if (is_remote_mode) {
                // REMOTE 模式：LED 常亮
                gpio_set_level(GPIO_NUM_48, 1);
            } else {
                // LOCAL 模式：LED 慢闪
                led_state = !led_state;
                gpio_set_level(GPIO_NUM_48, led_state ? 1 : 0);
            }
        } else {
            // USB 未连接：LED 快闪
            static int counter = 0;
            if (counter++ % 10 == 0) {
                gpio_set_level(GPIO_NUM_48, !gpio_get_level(GPIO_NUM_48));
            }
        }

        // 检查 BOOT 按钮（手动切换模式）
        if (gpio_get_level(APP_BUTTON) == 0) {
            is_remote_mode = !is_remote_mode;
            ESP_LOGI(TAG, "Manual mode switch: %s", is_remote_mode ? "REMOTE" : "LOCAL");
            vTaskDelay(pdMS_TO_TICKS(500)); // 防抖
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
