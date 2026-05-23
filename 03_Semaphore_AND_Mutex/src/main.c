#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "syscalls/device.h"

// 0. 注册日志模块
LOG_MODULE_REGISTER(lession3, LOG_LEVEL_INF);

// 1. 定义信号量：初始值为0，最大值为1
K_SEM_DEFINE(my_sem, 0, 1);

// 2. 获取硬件定义
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// 3. 生产者线程
void thread_producer(void *unused1, void *unused2, void *unused3) {
        while (1) {
                k_msleep(2000); // 模拟传感器采样耗时
                LOG_INF("Producer: Data sampled, signaling consumer...");

                // 释放信号量，通知消费者
                k_sem_give(&my_sem);
        }
}

// 4. 消费者线程
void thread_consumer(void *unused1, void *unused2, void *unused3) {
        if (!device_is_ready(led.port)) {
                return;
        }

        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

        while (1) {
                // 等待信号量，永久阻塞直到获取
                if (k_sem_take(&my_sem, K_FOREVER) == 0) {
                        LOG_INF("Consumer: Received signal, toggling LED.");
                        gpio_pin_toggle_dt(&led);
                }
        }
}

// 5. 定义并启动线程
// 生产者优先级设为7，消费者优先级设为7（抢占式调度）
K_THREAD_DEFINE(producer_id, 1024, thread_producer, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(consumer_id, 1024, thread_consumer, NULL, NULL, NULL, 7, 0, 0);