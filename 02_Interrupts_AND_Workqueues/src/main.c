#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

// 0. 日志模块注册，需要注意必须在prj.conf中启用CONFIG_LOG=y才会有正确的日志打印
LOG_MODULE_REGISTER(lession2, LOG_LEVEL_INF);

// 1. 获取设备树定义
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// 2. 定义工作项（Work Item）
struct k_work my_work;

// 定义可延迟工作项（Delayable Work Item）
struct k_work_delayable my_dwork;

// 3. 工作队列处理函数（下半部）
void my_work_handler(struct k_work *work) {
        // 这里是线程上下文，允许执行耗时操作
        LOG_INF("Workqueue executed: Processing heavy task(Toggle LED)...");
        gpio_pin_toggle_dt(&led0);
}
// 延迟任务工作队列处理函数（下半部）
void my_dwork_handler(struct k_work *work) {
        // 这里是线程上下文，允许执行耗时操作
        LOG_INF("Delay Workqueue executed: Toggle LED...");
        gpio_pin_toggle_dt(&led1);
}

// 4. 中断回调函数（上半部）
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
        // 严禁在这里执行LOG_INF 或 k_msleep
        // 快速提交任务给工作队列
        k_work_submit(&my_work);
}
// 中断回调函数，延迟处理任务（上半部）
void button_pressed_delay(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
        // 严禁在这里执行LOG_INF 或 k_msleep
        // 取消之前尚未执行的延迟工作（如有），实现每次按键重置2s定时
        k_work_cancel_delayable(&my_dwork);
        // 重新调度延迟工作项：2s后执行
        k_work_schedule(&my_dwork, K_SECONDS(2));
}

static struct gpio_callback button_cb_data;
static struct gpio_callback button_delay_cb_data;

int main(void)
{
        // 5. 初始化硬件
        gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
        gpio_pin_configure_dt(&button0, GPIO_INPUT | GPIO_PULL_UP);
        gpio_pin_configure_dt(&button1, GPIO_INPUT | GPIO_PULL_UP);

        // 6. 配置中断
        // 设置中断触发方式：下降沿（按键按下）
        gpio_pin_interrupt_configure_dt(&button0, GPIO_INT_EDGE_TO_ACTIVE);
        gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);

        // 初始化回调数据结构，绑定处理函数
        gpio_init_callback(&button_cb_data, button_pressed, BIT(button0.pin));
        gpio_init_callback(&button_delay_cb_data, button_pressed_delay, BIT(button1.pin));

        // 将回调添加到 GPIO驱动中
        gpio_add_callback(button0.port, &button_cb_data);
        gpio_add_callback(button1.port, &button_delay_cb_data);

        // 7.初始化工作项
        k_work_init(&my_work, my_work_handler);
        k_work_init_delayable(&my_dwork, my_dwork_handler);

        LOG_INF("Lession 2: Interrupt & Workqueue ready.");
        while (1) {
                k_sleep(K_FOREVER);
        }

        return 0;
}
