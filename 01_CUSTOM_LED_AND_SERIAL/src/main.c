#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

// 注册本文件的日志模块
LOG_MODULE_REGISTER(lession1, LOG_LEVEL_INF);

// 1. 通过别名获取设备树中的节点标识符
#define LED_EXT_NODE DT_ALIAS(led_ext)

// 2. 将节点信息转化为GPIO结构体（包含port、pin、flags）
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_EXT_NODE, gpios);

int main(void)
{
        int ret;

        // 3. 检查硬件设备是否就绪
        if (!device_is_ready(led.port)) {
                LOG_ERR("GPIO device not ready!");
                return -1;
        }

        // 4. 配置引脚为输出
        ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
        if (ret < 0) {
                LOG_ERR("Failed to configure GPIO!");
                return -1;
        }

        LOG_INF("Hello Zephyr! This program is controlling LED winking.");

        while (1) {
                LOG_INF("Port: %s Pin: %d", led.port->name, led.pin);
                gpio_pin_toggle_dt(&led);       // 翻转LED
                LOG_INF("LED Toggled.");
                k_msleep(1000); // 延时1000ms
        }

        return 0;
}
