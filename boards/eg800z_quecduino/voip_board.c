/**
 * @file voip_board.c
 * @brief EG800Z QuecDuino V1.0 的 LED、S2/S3、功放和 ES8311 适配。
 * @note 引脚编号为模组物理 Pin。依据 EVB 原理图第 1～3 页及本工程引脚映射。
 */
#include "voip_board.h"
#include "voip_demo_config.h"
#include "qosa_dev_eigen.h"
#include "qosa_sys.h"
#include "qosa_gpio.h"
#include "qosa_pinctrl.h"
#include "qosa_iic.h"
#include "qosa_audio.h"
#include "qosa_rtc.h"

/** @struct voip_key_t
 * @brief 单个按键的 GPIO 映射和独立消抖状态，仅由 IO 任务访问。
 */
typedef struct {
    qosa_pin_cfg_t pin; /**< 模组物理 Pin 对应的平台 GPIO。 */
    int previous; /**< 上次采样电平。 */
    int stable; /**< 已确认电平，0 表示按下。 */
    unsigned debounce; /**< 连续同电平采样次数，最多 3 次。 */
} voip_key_t;
static voip_key_t g_keys[2] = {
    {.previous = 1, .stable = 1},
    {.previous = 1, .stable = 1}
};
static qosa_pin_cfg_t g_led_reg, g_led_call, g_pa;
static bool g_io_ready, g_codec_ready, g_local_playback;
static unsigned char g_codec_address;
static int g_io_error;
static qosa_dai_t g_dai_before;
static int g_dai_before_rc;

// 保留当前 SDK audio_demo.c 的 ES8311 16 kHz、MSB、16 位初始化序列。
static const unsigned char g_codec_regs[][2] = {
    {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x20}, {0x16, 0x20}, {0x04, 0x20},
    {0x05, 0x00}, {0x0B, 0x00}, {0x0C, 0x00}, {0x0F, 0x44}, {0x10, 0x1F},
    {0x11, 0x7F}, {0x00, 0x80}, {0x00, 0x80}, {0x01, 0x3F}, {0x01, 0xBF},
    {0x02, 0x18}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x07, 0x00},
    {0x08, 0xFF}, {0x06, 0x03}, {0x01, 0xBF}, {0x06, 0x03}, {0x13, 0x10},
    {0x1B, 0x0A}, {0x1C, 0x6A}, {0x09, 0x0D}, {0x0A, 0x0D}, {0x09, 0x0D},
    {0x0A, 0x0D}, {0x32, 0xBF}, {0x09, 0x0D}, {0x0A, 0x0D}, {0x17, 0xBF},
    {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A}, {0x14, 0x1A}, {0x0D, 0x01},
    {0x15, 0x20}, {0x37, 0x48}, {0x45, 0x00}, {0x0D, 0x01}, {0x45, 0x00},
    {0x37, 0x08}, {0x14, 0x1A}, {0x12, 0x00}, {0x0E, 0x00}, {0x32, 0xBF}
};

/** @brief 按物理 Pin 查找 GPIO 映射并设置方向。
 * @param[in] unsigned pin 模组物理引脚。
 * @param[out] qosa_pin_cfg_t *cfg 返回目标平台映射。
 * @param[in] bool input true 表示按键输入，使用上拉。
 * @return 0 成功，其余为底层错误码。
 */
static int voip_setup_pin(unsigned pin, qosa_pin_cfg_t *cfg, bool input)
{
    int ret = qosa_get_pin_default_cfg(pin, cfg);
    if (ret) return ret;
    ret = qosa_pin_set_func(pin, cfg->gpio_func);
    if (ret) return ret;
    return qosa_gpio_init(cfg->gpio_num, input ? QOSA_GPIO_DIRECTION_INPUT : QOSA_GPIO_DIRECTION_OUTPUT,
                         input ? QOSA_GPIO_PULL_UP : QOSA_GPIO_PULL_NONE, QOSA_GPIO_LEVEL_LOW);
}

/** @brief 初始化 D3、D4、S2/S3 和功放控制，功放默认关闭。
 * @return 0 成功，其余为引脚初始化错误。
 */
int voip_board_init(void)
{
    int ret;
    if (g_io_ready) return 0;
    if ((ret = voip_setup_pin(55, &g_led_reg, false))) return ret;
    if ((ret = voip_setup_pin(56, &g_led_call, false))) return ret;
    if ((ret = voip_setup_pin(50, &g_keys[0].pin, true))) return ret;
    if ((ret = voip_setup_pin(51, &g_keys[1].pin, true))) return ret;
    if ((ret = voip_setup_pin(54, &g_pa, false))) return ret;
    g_io_ready = true;
    return 0;
}

/** @brief 在指定地址读取 ES8311 的两个只读芯片 ID 寄存器。
 * @param[in] unsigned char address 7 位 I2C 地址。
 * @return 0 表示 ID 为 0x8311；-20 表示不匹配；其余为 I2C 错误码。
 */
static int voip_codec_probe(unsigned char address)
{
    unsigned char high = 0, low = 0;
    int ret = qosa_i2c_read(QOSA_I2C_1, address, 0xFD, &high, 1);
    if (ret) return ret;
    ret = qosa_i2c_read(QOSA_I2C_1, address, 0xFE, &low, 1);
    if (ret) return ret;
    return high == 0x83 && low == 0x11 ? 0 : -20;
}

/** @brief 识别 ES8311 并设置 16 kHz 外部 PCM 通路。
 * @return 0 成功；-20 芯片不匹配；其余为底层错误码。
 * @note 自动模式兼容探测 0x19 和 0x18；本 EVB 原理图 CE 经 R45 下拉，对应 0x18。
 */
int voip_board_audio_init(void)
{
    unsigned i;
    unsigned char address;
    int ret;
    qosa_aud_i2s_cfg_t timing = {0, 0, 1, 1};
    qosa_dai_t dai = {3, 0, 0, 4, 0, QOSA_AUDIO_HAL_16K_SAMPLES, 1, 16};
    qosa_dai_t actual = {0};
    if (g_codec_ready) return 0;
    if (!g_io_ready) return -1;
    // 保留经板测验证的启动延时；公开接口同时切换 Normal/AON IO 电压。
    qosa_uint32_t uptime = qosa_get_running_time();
    if (uptime < 5000) qosa_task_sleep_ms(5000 - uptime);
    if ((ret = qosa_aud_output_ctrl(QOSA_AUDIO_OUTPUT_EXTERNAL, &timing))) return ret;
    if ((ret = qosa_gpio_set_voltage(VOL_3_30V))) return ret;
    if ((ret = qosa_pin_set_func(67, 2))) return ret;
    if ((ret = qosa_pin_set_func(66, 2))) return ret;
    if ((ret = qosa_i2c_init(QOSA_I2C_1, QOSA_IIC_STANDARD_MODE))) return ret;
    qosa_task_sleep_ms(1000);
#if VOIP_DEMO_CODEC_ADDR == 0
    address = 0x19;
    ret = voip_codec_probe(address);
    if (ret) {
        address = 0x18;
        ret = voip_codec_probe(address);
    }
#else
    address = VOIP_DEMO_CODEC_ADDR;
    ret = voip_codec_probe(address);
#endif /* VOIP_DEMO_CODEC_ADDR */
    if (ret) return ret;
    for (i = 0; i < sizeof(g_codec_regs) / sizeof(g_codec_regs[0]); ++i) {
        unsigned char value = g_codec_regs[i][1];
        unsigned attempt;
        for (attempt = 0; attempt < 3; ++attempt) {
            ret = qosa_i2c_write(QOSA_I2C_1, address, g_codec_regs[i][0], &value, 1);
            qosa_task_sleep_ms(1);
            if (!ret) break;
        }
        if (ret) return ret;
    }
    if ((ret = qosa_aud_output_ctrl(QOSA_AUDIO_OUTPUT_EXTERNAL, &timing))) return ret;
    // fsync=0 对应 EC718 的 MSB_MODE；与 ES8311 0x09/0x0A=0x0D 保持一致。
    // 先读取 NV 同步驱动缓存，否则默认 RAM 值相等时 setter 会跳过保存旧 NV 的修正。
    g_dai_before_rc = qosa_audio_get_stream_iface(&g_dai_before);
    if (g_dai_before_rc && g_dai_before_rc != QOSA_AUD_SYSTEM_ERR) return g_dai_before_rc;
    if ((ret = qosa_audio_set_stream_iface(&dai))) return ret;
    ret = qosa_audio_get_stream_iface(&actual);
    // 无 NV 文件时 EC718 getter 返回 SYSTEM_ERR，但同时返回有效的 RAM 默认配置。
    if (ret && ret != QOSA_AUD_SYSTEM_ERR) return ret;
    if (actual.io != dai.io || actual.mode != dai.mode || actual.fsync != dai.fsync ||
        actual.clock != dai.clock || actual.format != dai.format || actual.sample != dai.sample ||
        actual.num_slots != dai.num_slots || actual.slot_mapping != dai.slot_mapping) return -21;
    if ((ret = qosa_aud_set_volume((qosa_aud_vol_level_e)VOIP_DEMO_CALL_VOLUME))) return ret;
    qosa_enter_critical();
    g_codec_address = address;
    g_codec_ready = true;
    qosa_exit_critical();
    return 0;
}

/** @brief 记录 GPIO 输出错误，使状态查询能区分初始化和运行错误。
 * @param[in] qosa_pin_cfg_t *pin 已初始化的 GPIO 映射。
 * @param[in] bool level 输出电平。
 */
static void voip_pin_write(qosa_pin_cfg_t *pin, bool level)
{
    int ret = qosa_gpio_set_level(pin->gpio_num, level ? QOSA_GPIO_LEVEL_HIGH : QOSA_GPIO_LEVEL_LOW);
    if (ret) {
        qosa_enter_critical(); g_io_error = ret; qosa_exit_critical();
    }
}

/** @brief 根据 SIP 快照更新注册灯、通话灯和功放。
 * @param[in] const qcm_voip_status_t *status SIP 状态快照。
 * @param[in] bool started true 表示已经开始注册或 SIP 服务在运行。
 */
void voip_board_update(const qcm_voip_status_t *status, bool started)
{
    unsigned now = qosa_get_running_time();
    bool audio_ready, local_playback;
    bool incoming, connected, dialing;
    if (!g_io_ready || !status) return;
    incoming = status->state == QCM_VOIP_INCOMING || status->state == QCM_VOIP_WAITING;
    connected = status->state == QCM_VOIP_CONNECTED;
    dialing = status->state == QCM_VOIP_DIALING || status->state == QCM_VOIP_ALERTING;
    qosa_enter_critical();
    audio_ready = g_codec_ready;
    local_playback = g_local_playback;
    qosa_exit_critical();
    voip_pin_write(&g_led_reg, status->registered || (started && (now / 500U) % 2U));
    voip_pin_write(&g_led_call, connected || ((incoming || dialing) && (now / 150U) % 2U));
    // J13 短接 1-2 时由 Pin54 控制；短接 2-3 会硬件常开。
    voip_pin_write(&g_pa, audio_ready && (connected || dialing || local_playback));
}

/** @brief 对单个低有效按键进行消抖，长按期间只触发一次。
 * @param[in,out] voip_key_t *key 当前键的 GPIO 和消抖状态。
 * @return 1 表示确认了新按下；0 表示未按下、尚不稳定或读取失败。
 * @note GPIO 读取失败会重新计数，不能将错误读数当成按下。
 */
static int voip_key_poll(voip_key_t *key)
{
    qosa_gpio_level_e level;
    int ret = qosa_gpio_get_level(key->pin.gpio_num, &level);
    if (ret) {
        qosa_enter_critical(); g_io_error = ret; qosa_exit_critical();
        key->debounce = 0;
        return 0;
    }
    if ((int)level != key->previous) { key->previous = level; key->debounce = 0; }
    if (key->debounce < 3U) ++key->debounce;
    if (key->debounce == 3U && key->stable != (int)level) {
        key->stable = level;
        return key->stable == 0;
    }
    return 0;
}

/** @brief 每 20 ms 轮询两键，独立确认连续三次稳定采样。
 * @return VOIP_BOARD_KEY_CALL/CONTROL 位图；可同时置位，无新按下返回 0。
 */
int voip_board_key(void)
{
    int keys = 0;
    if (!g_io_ready) return 0;
    if (voip_key_poll(&g_keys[0])) keys |= VOIP_BOARD_KEY_CALL;
    if (voip_key_poll(&g_keys[1])) keys |= VOIP_BOARD_KEY_CONTROL;
    return keys;
}

/** @brief 为来电铃声控制功放；通话功放由状态更新函数维护。
 * @param[in] bool enable true 打开，false 关闭。
 */
void voip_board_audio_output(bool enable)
{
    qosa_enter_critical(); g_local_playback = enable; qosa_exit_critical();
    if (g_io_ready) voip_pin_write(&g_pa, enable);
}

/** @brief 查询经芯片 ID 校验的 I2C 地址。
 * @return 0 尚未初始化；其余为实际 7 位地址。
 */
unsigned voip_board_codec_address(void)
{
    return g_codec_address;
}

/** @brief 查询最近一次 GPIO 运行错误。
 * @return 0 未记录错误，其余为底层错误码。
 */
int voip_board_io_error(void)
{
    int value;
    qosa_enter_critical(); value = g_io_error; qosa_exit_critical();
    return value;
}

