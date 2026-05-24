#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

// 0. 注册日志模块
LOG_MODULE_REGISTER(lession5_ble, LOG_LEVEL_INF);

#define BLE_DEVICE_NAME         CONFIG_BT_DEVICE_NAME
#define BLE_DEVICE_NAME_LEN     (sizeof(BLE_DEVICE_NAME) - 1)

/* 
 * ==========================================
 * 自定义 Manufacturer Data
 * 格式：[Company ID 2Bytes] + [自定义Payload]
 * Company ID 用0xFFFF表示测试/实验用途
 * Payload 配置为姓名缩写+生日
 * ==========================================
 */
#define MANUFACTURER_COMPANY_ID         0xFFFF

// 自定义数据：姓名缩写"wzh" + 生日"0909"
static const uint8_t manufacturer_data[] = {
        // Company ID （小端序）
        BT_BYTES_LIST_LE16(MANUFACTURER_COMPANY_ID),

        // 自定义Payload
        // 姓名缩写
        'W', 'z', 'h',
        // 生日
        0x09, 0x09,
        // 自定义标记吗（可选，方便在App中识别）
        0xDE, 0xAD,
};

// 1. 定义广播数据
static const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA(BT_DATA_NAME_COMPLETE, 
                BLE_DEVICE_NAME, 
                BLE_DEVICE_NAME_LEN),

        // 新增：厂商自定义数据
        BT_DATA(BT_DATA_MANUFACTURER_DATA, 
                manufacturer_data,
                sizeof(manufacturer_data))
};

// 2. 定义扫描响应数据（可选，别人扫描你时返回的额外信息）
static const struct bt_data sd[] = {
        BT_DATA_BYTES(BT_DATA_UUID128_ALL, 
                BT_UUID_128_ENCODE(
                        0x12345678,     // time_low
                        0x1234,         // time_mid
                        0x5678,         // time_hi_and_version
                        0x9abc,         // clock_seq
                        0xdef0123456789abcULL   // node
                )),
};

int main(void)
{
        int err = 0;

        LOG_INF("Starting Lession5: BLE Advertising...");
       
        // 3. 初始化蓝牙控制器和协议栈
        err = bt_enable(NULL);
        if (err) {
                LOG_ERR("Bluetooth init failed(err %d)", err);
                return -1;
        }

        LOG_INF("Bluetooth initialized.");

        // 4. 开始广播
        // 使用默认广播参数：可连接、可发现
        err = bt_le_adv_start(
                BT_LE_ADV_PARAM(BT_LE_ADV_OPT_SCANNABLE,
                    BT_GAP_ADV_FAST_INT_MIN_2,   /* 100ms */
                    BT_GAP_ADV_FAST_INT_MAX_2,   /* 150ms */
                    NULL), 
                ad, ARRAY_SIZE(ad), 
                sd, ARRAY_SIZE(sd));
        if (err) {
                LOG_ERR("Advertising failed to start (err %d)", err);
                return -1;
        }

        LOG_INF("Advertising successfully started!");

        return 0;
}
