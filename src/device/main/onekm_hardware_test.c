/*
 * OneKM 硬件可行性测试程序
 *
 * 测试键盘和鼠标混合输入功能
 * - 键盘：依次按下 A, B, C, Enter
 * - 鼠标：移动并点击
 * - 循环执行，便于验证硬件可行性
 */

#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#define APP_BUTTON (GPIO_NUM_0)  // BOOT 按钮
static const char *TAG = "onekm_test";

/************* TinyUSB 描述符 ****************/

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

// HID 报告描述符：键盘 + 鼠标
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

// 字符串描述符
const char* hid_string_descriptor[5] = {
    (char[]){0x09, 0x04},  // 语言：英语
    "OneKM",               // 制造商
    "OneKM Test Device",   // 产品
    "123456",              // 序列号
    "OneKM HID Test",      // HID 接口
};

// 配置描述符
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/********* TinyUSB HID 回调函数 ***************/

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

/********* 测试程序 ***************/

// 发送键盘按键（按下 + 释放）
static void send_key(uint8_t keycode)
{
    uint8_t keys[6] = {0};
    keys[0] = keycode;

    ESP_LOGI(TAG, "Keyboard: press key %d", keycode);
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keys);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Keyboard: release key");
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
    vTaskDelay(pdMS_TO_TICKS(50));
}

// 发送鼠标移动
static void send_mouse_move(int8_t dx, int8_t dy)
{
    ESP_LOGI(TAG, "Mouse: move (%d, %d)", dx, dy);
    tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, dx, dy, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
}

// 发送鼠标按键
static void send_mouse_button(uint8_t buttons)
{
    ESP_LOGI(TAG, "Mouse: buttons 0x%02X", buttons);
    tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, buttons, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 释放按键
    tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0, 0, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
}

// 混合输入测试序列
static void mixed_input_test(void)
{
    ESP_LOGI(TAG, "=== Starting Mixed Input Test ===");

    // 1. 键盘输入：输入 "abc" + Enter
    ESP_LOGI(TAG, "Step 1: Keyboard typing 'abc' + Enter");
    send_key(HID_KEY_A);
    send_key(HID_KEY_B);
    send_key(HID_KEY_C);
    send_key(HID_KEY_ENTER);

    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. 鼠标移动：画一个小正方形
    ESP_LOGI(TAG, "Step 2: Mouse drawing square");
    for (int i = 0; i < 10; i++) {
        send_mouse_move(5, 0);
    }
    for (int i = 0; i < 10; i++) {
        send_mouse_move(0, 5);
    }
    for (int i = 0; i < 10; i++) {
        send_mouse_move(-5, 0);
    }
    for (int i = 0; i < 10; i++) {
        send_mouse_move(0, -5);
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // 3. 鼠标点击：左键点击
    ESP_LOGI(TAG, "Step 3: Mouse left click");
    send_mouse_button(0x01);  // 左键

    vTaskDelay(pdMS_TO_TICKS(500));

    // 4. 鼠标点击：右键点击
    ESP_LOGI(TAG, "Step 4: Mouse right click");
    send_mouse_button(0x02);  // 右键

    vTaskDelay(pdMS_TO_TICKS(500));

    // 5. 键盘组合键：Ctrl+A
    ESP_LOGI(TAG, "Step 5: Keyboard Ctrl+A");
    uint8_t ctrl_a[6] = {HID_KEY_A};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, HID_KEY_MODIFIER_LEFT_CTRL, ctrl_a);
    vTaskDelay(pdMS_TO_TICKS(50));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

    vTaskDelay(pdMS_TO_TICKS(500));

    // 6. 鼠标快速移动
    ESP_LOGI(TAG, "Step 6: Mouse rapid movement");
    for (int i = 0; i < 20; i++) {
        send_mouse_move(10, 5);
    }

    ESP_LOGI(TAG, "=== Test Sequence Complete ===");
    ESP_LOGI(TAG, "Waiting 5 seconds before next cycle...");
    vTaskDelay(pdMS_TO_TICKS(5000));
}

// 键盘单独测试（循环输入字母）
static void keyboard_only_test(void)
{
    static uint8_t key_index = 0;
    uint8_t keys[] = {HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F};

    ESP_LOGI(TAG, "Keyboard test: sending key %d", keys[key_index]);
    send_key(keys[key_index]);

    key_index = (key_index + 1) % 6;
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// 鼠标单独测试（画圆）
static void mouse_only_test(void)
{
    ESP_LOGI(TAG, "Mouse test: drawing circle");

    // 简单的圆形轨迹
    for (int angle = 0; angle < 360; angle += 30) {
        double rad = angle * 3.14159 / 180.0;
        int8_t dx = (int8_t)(cos(rad) * 8);
        int8_t dy = (int8_t)(sin(rad) * 8);
        send_mouse_move(dx, dy);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
}

/********* 主程序 ***************/

void app_main(void)
{
    // 初始化 BOOT 按钮
    const gpio_config_t boot_button_config = {
        .pin_bit_mask = BIT64(APP_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_button_config));

    ESP_LOGI(TAG, "=== OneKM Hardware Test ===");
    ESP_LOGI(TAG, "Press BOOT button to pause/resume");
    ESP_LOGI(TAG, "Testing keyboard + mouse mixed input");

    // 初始化 USB
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
    ESP_LOGI(TAG, "Waiting for USB connection to Windows PC...");

    // 等待 USB 连接
    while (!tud_mounted()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "USB connected! Starting test...");

    // 测试模式选择
    enum {
        MODE_FULL_TEST = 0,  // 完整混合测试
        MODE_KEYBOARD_ONLY,  // 仅键盘
        MODE_MOUSE_ONLY,     // 仅鼠标
        MODE_MAX
    };
    int test_mode = MODE_FULL_TEST;

    while (1) {
        // 检查 BOOT 按钮（暂停/恢复）
        if (gpio_get_level(APP_BUTTON) == 0) {
            ESP_LOGI(TAG, "Button pressed - pausing 2 seconds...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            test_mode = (test_mode + 1) % MODE_MAX;
            ESP_LOGI(TAG, "Switched to test mode: %d", test_mode);
        }

        // 执行测试
        if (tud_mounted()) {
            switch (test_mode) {
                case MODE_FULL_TEST:
                    mixed_input_test();
                    break;
                case MODE_KEYBOARD_ONLY:
                    keyboard_only_test();
                    break;
                case MODE_MOUSE_ONLY:
                    mouse_only_test();
                    break;
            }
        } else {
            ESP_LOGI(TAG, "USB disconnected, waiting for reconnection...");
            while (!tud_mounted()) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            ESP_LOGI(TAG, "USB reconnected!");
        }
    }
}
