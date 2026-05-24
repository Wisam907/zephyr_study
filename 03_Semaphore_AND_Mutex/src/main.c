#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "syscalls/device.h"

LOG_MODULE_REGISTER(lesson3_mutex, LOG_LEVEL_INF);

// 定义互斥锁
K_MUTEX_DEFINE(buf_mutex);

/* ==========================================================
 *  共享资源（故意不加锁，用于观察竞争现象）
 * ========================================================== */
#define SHARED_BUF_SIZE 256
static char shared_buf[SHARED_BUF_SIZE];
static volatile int shared_pos = 0;

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* ==========================================================
 *  缓慢写入函数
 *
 *  ★ 关键改变：去掉了局部变量 pos
 *    直接用 shared_buf[shared_pos++] 逐字符推进
 *
 *  这样两个线程交替执行时：
 *    A 写字符到 pos 0, shared_pos → 1, sleep
 *    B 写字符到 pos 1, shared_pos → 2, sleep
 *    A 写字符到 pos 2, shared_pos → 3, sleep
 *    B 写字符到 pos 3, shared_pos → 4, sleep
 *    ...
 *  → 两个线程的字符交替排列，形成清晰的交织
 * ========================================================== */
static void slow_write(const char *tag, const char *msg)
{
    for (int i = 0; tag[i] != '\0'; i++) {
        if (shared_pos < SHARED_BUF_SIZE - 1) {
            shared_buf[shared_pos++] = tag[i];
        }
        k_msleep(2);
    }
    if (shared_pos < SHARED_BUF_SIZE - 1) {
        shared_buf[shared_pos++] = ' ';
    }
    for (int i = 0; msg[i] != '\0'; i++) {
        if (shared_pos < SHARED_BUF_SIZE - 1) {
            shared_buf[shared_pos++] = msg[i];
        }
        k_msleep(2);
    }
    if (shared_pos < SHARED_BUF_SIZE) {
        shared_buf[shared_pos] = '\0';
    }
}

/* ==========================================================
 *  线程 A
 * ========================================================== */
void thread_a(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        // 加锁
        k_mutex_lock(&buf_mutex, K_FOREVER);

        shared_pos = 0;
        memset(shared_buf, 0, SHARED_BUF_SIZE);

        slow_write("[Thread-A]", "Hello from A.");

        /* 打印当前缓冲区内容（此时 B 也写了一半进去） */
        LOG_INF("Buffer: %s", shared_buf);

        k_mutex_unlock(&buf_mutex);
        k_msleep(100);

    }
}

/* ==========================================================
 *  线程 B
 * ========================================================== */
void thread_b(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        // 加锁
        k_mutex_lock(&buf_mutex, K_FOREVER);

        shared_pos = 0;
        memset(shared_buf, 0, SHARED_BUF_SIZE);
        slow_write("[Thread-B]", "Hello from B.");

        LOG_INF("Buffer: %s", shared_buf);

        k_mutex_unlock(&buf_mutex);

        k_msleep(100);

    }
}

/* ==========================================================
 *  LED 线程（独立运行）
 * ========================================================== */
void thread_led(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(led.port)) {
        return;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(1000);
    }
}

K_THREAD_DEFINE(tid_a,   2048, thread_a,   NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(tid_b,   2048, thread_b,   NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(tid_led, 1024, thread_led, NULL, NULL, NULL, 3, 0, 0);
