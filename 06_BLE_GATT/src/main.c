#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_led, LOG_LEVEL_INF);

/* ==================== 硬件：板载 LED ==================== */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* ==================== BLE UUID 定义 ==================== */
static struct bt_uuid_128 my_service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x567812345678));

static struct bt_uuid_128 my_char_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x567812345679));

/* ==================== LED 状态 ==================== */
static uint8_t led_status = 0;

/* ==================== GATT 读写回调 ==================== */
static ssize_t read_led(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    LOG_INF("Client reading LED status: %d", led_status);

    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &led_status, sizeof(led_status));
}

static ssize_t write_led(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         const void *buf, uint16_t len, uint16_t offset,
                         uint8_t flags)
{
    if (offset + len > sizeof(led_status)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(&led_status + offset, buf, len);
    LOG_INF("Client wrote LED status: %d", led_status);

    gpio_pin_set_dt(&led, led_status ? 1 : 0);

    return len;
}

/* ==================== GATT 服务注册 ==================== */
BT_GATT_SERVICE_DEFINE(my_led_svc,
    BT_GATT_PRIMARY_SERVICE(&my_service_uuid),
    BT_GATT_CHARACTERISTIC(&my_char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           read_led, write_led, &led_status),
);

/* ==================== 广播数据 ==================== */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
        BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678,
                           0x1234, 0x567812345678)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ==================== 广播参数 ==================== */
static const struct bt_le_adv_param adv_param = {
    .id                 = BT_ID_DEFAULT,
    .sid                = 0,
    .secondary_max_skip = 0,
    .options            = BT_LE_ADV_OPT_CONN,
    .interval_min       = BT_GAP_ADV_FAST_INT_MIN_2,
    .interval_max       = BT_GAP_ADV_FAST_INT_MAX_2,
    .peer               = NULL,
};

/* ================================================================
 *  ★ 延迟工作项：用于断连后延迟重启广播
 * ================================================================ */
static struct k_work_delayable adv_work;

static void adv_work_handler(struct k_work *work)
{
    int err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad),
                              sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising restart failed (err %d)", err);
    } else {
        LOG_INF("Advertising restarted, waiting for connections...");
    }
}

/* ==================== 连接/断开回调 ==================== */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
    } else {
        LOG_INF("Connected");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);

    /* ★ 不在回调中直接重启，而是调度一个 100ms 延迟的工作项 */
    k_work_schedule(&adv_work, K_MSEC(100));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* ==================== 主函数 ==================== */
int main(void)
{
    int err;

    /* 0. 初始化延迟工作项 */
    k_work_init_delayable(&adv_work, adv_work_handler);

    /* 1. 初始化 LED GPIO */
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO device not ready");
        return -1;
    }

    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("LED pin config failed (err %d)", err);
        return -1;
    }
    LOG_INF("LED GPIO initialized (OFF)");

    /* 2. 初始化蓝牙协议栈 */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return -1;
    }
    LOG_INF("Bluetooth initialized");

    /* 3. 启动 BLE 广播 */
    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising start failed (err %d)", err);
        return -1;
    }
    LOG_INF("Advertising started, waiting for connections...");

    return 0;
}
