/**
 * @file voip_ringtone.c
 * @brief 原文件 telefone_ring.mp3 的来电 PCM 响铃；与接听、挂断共用唯一控制任务。
 * @note 原录音首个完整两秒周期，16 kHz 单声道；资源压扩保存，逐帧还原为 PCM。
 */
#include "voip_ringtone.h"
#include "voip_demo_config.h"
#include "voip_board.h"
#include "qosa_sys.h"
#include "qosa_rtc.h"
#include "qcm_voip_platform.h"
#include "qcm_voip_tone_volume.h"
#include "qcm_audio.h"

/** 每帧 320 个 16 位样本，对应 20 ms。 */
#define RING_SAMPLES 320U
/** 原文件完整一轮为 2 秒，包含约 1.3 秒铃声和 0.7 秒停顿。 */
#define RING_CYCLE_SAMPLES 32000U
/** 原录音经单声道/16 kHz 转换后的 mu-law 资源；末尾解码为零的样本省略存储。 */
static const uint8_t ring_audio[] = {
#include "assets/telefone_ring_ulaw.inc"
};
static qosa_aud_handle_t ring_handle; /**< 控制任务独占，关闭失败保留以便重试。 */
static int16_t ring_frame[RING_SAMPLES]; /**< 可写帧，底层可原地调整音量。 */
static unsigned ring_offset, ring_position, ring_call;
static uint32_t ring_due, ring_progress, ring_retry;
static bool ring_seen, ring_blocked, ring_playing;

/** @brief 解压一份原铃声样本并调整数字幅度，静音段直接补零。
 * @param[in] unsigned position 两秒周期内的样本下标，范围 0～31999。
 * @return 16 kHz 单声道 S16 PCM；无需 MP3 解码器或浮点运算。
 * @note mu-law 仅为本地资源压缩，与 SIP 使用的 8 kHz PCMA 无关。
 */
static int16_t ring_sample(unsigned position)
{
    unsigned code;
    int32_t sample;
    if (position >= sizeof(ring_audio)) return 0;
    code = (unsigned)(~ring_audio[position]) & 255U;
    sample = (int32_t)(((code & 15U) << 3) + 132U) << ((code >> 4) & 7U);
    sample = (code & 128U) ? 132 - sample : sample - 132;
    return qcm_voip_tone_sample((int16_t)(sample * VOIP_DEMO_RING_GAIN / 100),
                                VOIP_DEMO_RING_VOLUME);
}

/** @brief 先关闭本地 PA 标志再关闭响铃流；关闭失败保留句柄并阻止接听。
 * @param[in] bool inhibit true 表示本次来电不再重新起铃，防止异步接听期间重开。
 * @return 0 已释放；-4 音频关闭失败，调用者不得继续打开通话音频。
 */
int voip_ringtone_stop(bool inhibit)
{
    if (inhibit) ring_blocked = true;
    if (ring_playing) {
        voip_board_audio_output(false);
        ring_playing = false;
    }
    if (ring_handle) {
        if (qcm_aud_stream_close(ring_handle, true)) return -4;
        ring_handle = NULL;
    }
    ring_offset = sizeof(ring_frame);
    return 0;
}

/** @brief 打开单路原始 PCM 响铃并预留三帧播放余量。
 * @return 0 成功，其余为板级、音量或流打开错误。
 * @note 注册前已完成 codec 初始化；不新增任务、不改变通话编解码格式。
 */
static int ring_open(void)
{
    qosa_aud_stream_cfg_t cfg = {0};
    int ret = voip_board_audio_init();
    if (ret) return ret;
    /* 档位已作用于 PCM 样本；底层保持原幅度，避免与通话全局音量叠加。 */
    ret = qosa_aud_set_volume(QOSA_AUD_VOLUME_LEVEL_6);
    if (ret) return ret;
    cfg.samprate = 16000;
    cfg.channels = 1;
    cfg.format = QOSA_AUD_FMT_PCM;
    /* PCM uses the local route in the official SDK. */
    cfg.sync_mode = false;
    /* 官方 SDK 的旧音频归档不保证回调 ctx 透传。保持 callback/ctx 为零，
     * 由同一控制任务检查 write 返回值和 200 ms 写入进度，避免迟到回调污染新来电。 */
    ret = qcm_aud_stream_open(&cfg, &ring_handle);
    if (ret) return ret;
    ring_position = 0;
    ring_offset = sizeof(ring_frame);
    ring_progress = qcm_voip_uptime_ms();
    ring_retry = ring_progress;
    ring_due = ring_progress - 60U;
    ring_playing = true;
    voip_board_audio_output(true);
    return 0;
}

/** @brief 轮询来电并按节拍提交 PCM；只由控制任务在请求之间调用。
 * @param[in] const qcm_voip_status_t *status 当前通话状态快照。
 * @param[in] bool allowed SIP 已启动且板级初始化成功时为 true。
 * @return 无；音频失败时停止本次响铃，不修改 SIP 呼叫状态。
 * @note 每轮最多四次非阻塞写入；正常请求到达即可先停铃，再执行接听或挂断。
 */
void voip_ringtone_poll(const qcm_voip_status_t *status, bool allowed)
{
    bool incoming;
    uint32_t now = qcm_voip_uptime_ms();
    unsigned attempt;
    int ret = 0;
    if (!status) return;
    incoming = allowed && VOIP_DEMO_RING_ENABLE && status->state == QCM_VOIP_INCOMING;
    if (!incoming || ((ring_seen || ring_handle) && ring_call != status->call_id)) {
        ring_seen = false;
        ring_blocked = false;
        if (ring_handle && (int32_t)(now - ring_retry) >= 0) {
            ret = voip_ringtone_stop(false);
            ring_retry = now + 250U;
        }
        if (ring_handle || !incoming) return;
    }
    if (!ring_seen) {
        ring_seen = true;
        ring_call = status->call_id;
    }
    if (ring_blocked) {
        if (ring_handle && (int32_t)(now - ring_retry) >= 0) {
            ret = voip_ringtone_stop(false);
            ring_retry = now + 250U;
        }
        return;
    }
    if (!ring_handle && (ret = ring_open())) goto error;
    for (attempt = 0; attempt < 4U; ++attempt) {
        unsigned i;
        int written;
        if (ring_offset == sizeof(ring_frame)) {
            if ((int32_t)(now - ring_due) < 0) break;
            for (i = 0; i < RING_SAMPLES; ++i) {
                ring_frame[i] = ring_sample(ring_position);
                ring_position = (ring_position + 1U) % RING_CYCLE_SAMPLES;
            }
            ring_offset = 0;
        }
        written = qcm_aud_stream_write(ring_handle, (uint8_t *)ring_frame + ring_offset,
                                       sizeof(ring_frame) - ring_offset);
        if (written < 0 || (unsigned)written > sizeof(ring_frame) - ring_offset || (written & 1)) {
            ret = -3; goto error;
        }
        if (!written) {
            if ((uint32_t)(now - ring_progress) >= 200U) { ret = -6; goto error; }
            break;
        }
        ring_offset += (unsigned)written;
        ring_progress = now;
        if (ring_offset == sizeof(ring_frame)) ring_due += 20U;
    }
    return;
error:
    ret = voip_ringtone_stop(true);
}
