/**
 * @file voip_sim.c
 * @brief 无卡开机后插卡的检测与受限恢复，使用官方 SDK 公开接口。
 * @note Pin79 是 EVB 原理图中的 SIM_DET。仅读取电平，不接管底层检测中断。
 */
#include "qosa_sys.h"
#include "qosa_sim.h"
#include "qosa_dev1.h"
#include "qosa_lpm.h"
#include "qcm_voip_platform.h"
#include "voip_demo_config.h"
#include "voip_sim.h"

enum {
    SIM_REC_IDLE, SIM_REC_SETTLE, SIM_REC_CFUN0, SIM_REC_CFUN1,
    SIM_REC_DONE, SIM_REC_ERROR
};
static unsigned phase;             /**< 当前恢复阶段，不是累计次数。 */
static int recovery_rc;            /**< 最近一次恢复错误；0 表示尚未出错。 */
static bool pending, callback_done;
static int callback_rc;
static unsigned requested_func;
static uint32_t request_at;
static bool restore_due;
static uint32_t restore_at;
static unsigned restore_left;      /**< 仅内部限制 CFUN1 恢复次数，不向 AT 输出。 */
static bool polled, settling, removed, attempted;
static uint32_t poll_at, settle_at, removed_at;

/** @brief 保存异步 CFUN 结果；只使用静态存储，超时后的迟到回调也不会访问栈变量。 */
static void sim_cfun_done(void *ctx, void *arg)
{
    const qosa_dev_general_cnf_t *cnf = arg;
    (void)ctx;
    qosa_enter_critical();
    if (pending && !callback_done) {
        callback_rc = cnf ? (int)cnf->err_code : -42;
        callback_done = true;
    }
    qosa_exit_critical();
}

/** @brief 更新供 AT 读取的当前阶段和错误。 */
static void sim_result(unsigned state, int rc)
{
    qosa_enter_critical(); phase = state; recovery_rc = rc; qosa_exit_critical();
}

/** @brief 提交一个 CFUN 请求；同一时刻至多一个，返回 0 仅表示受理。 */
static int sim_set_cfun(unsigned func, uint32_t now)
{
    int rc;
    requested_func = func;
    request_at = now;
    qosa_enter_critical();
    pending = true; callback_done = false;
    phase = func ? SIM_REC_CFUN1 : SIM_REC_CFUN0;
    qosa_exit_critical();
    rc = qosa_dev_set_cfun(0, (qosa_uint8_t)func, sim_cfun_done, NULL);
    if (rc) {
        qosa_enter_critical(); pending = false; qosa_exit_critical();
        sim_result(SIM_REC_ERROR, rc);
    }
    return rc;
}

/** @brief 有限重试恢复 CFUN1；即使用户停止自动注册，也完成已开始的恢复。 */
static void sim_restore(uint32_t now)
{
    if (!restore_left) { restore_due = false; return; }
    --restore_left;
    restore_due = false;
    if (sim_set_cfun(1, now) && restore_left) {
        restore_due = true; restore_at = now;
    }
}

bool voip_sim_recovering(void)
{
    /* 仅控制任务使用；回调不修改 pending/restore_due。 */
    return pending || restore_due;
}

void voip_sim_poll(bool allow)
{
    uint32_t now = qcm_voip_uptime_ms();
    qosa_lpm_wakeup_pin_level_e level = QOSA_LPM_LEVEL_LOW;
    qosa_sim_hot_swap_cfg_t cfg = {0};
    qosa_sim_status_e sim = QOSA_SIM_STATUS_UNKNOWN;
    qosa_uint8_t slot = 0xff, cfun = 0xff;
    bool done;
    int rc;

    qosa_enter_critical(); done = callback_done; rc = callback_rc; qosa_exit_critical();
    if (pending) {
        if (!done) {
            /* 不重用仍在等待回调的请求。迟到回调到达后继续恢复 CFUN1。 */
            if ((uint32_t)(now - request_at) >= 30000U) sim_result(SIM_REC_ERROR, -42);
            return;
        }
        qosa_enter_critical(); pending = false; callback_done = false; qosa_exit_critical();
        if (!requested_func) {
            if (rc) sim_result(SIM_REC_ERROR, rc);
            sim_restore(now);
        } else if (rc) {
            sim_result(SIM_REC_ERROR, rc);
            if (restore_left) { restore_due = true; restore_at = now; }
        } else {
            sim_result(recovery_rc ? SIM_REC_ERROR : SIM_REC_DONE, recovery_rc);
        }
        return;
    }
    if (restore_due) {
        if ((uint32_t)(now - restore_at) >= 5000U) sim_restore(now);
        return;
    }
    if (polled && (uint32_t)(now - poll_at) < 1000U) return;
    polled = true; poll_at = now;

    /* 只读物理电平，不将专用 SIM_DET 切换为 GPIO，也不改写 SDK。 */
    if (qosa_lpm_wakeup_pin_get_level(79, &level)) { settling = removed = false; return; }
    if ((unsigned)level != VOIP_DEMO_SIM_INSERT_LEVEL) {
        settling = false;
        if (!removed) { removed = true; removed_at = now; }
        if ((uint32_t)(now - removed_at) >= 2000U) {
            attempted = false; sim_result(SIM_REC_IDLE, 0);
        }
        return;
    }
    removed = false;
    if (!allow || !VOIP_DEMO_SIM_HOT_SWAP || !VOIP_DEMO_SIM_RECOVER || attempted) {
        settling = false;
        if (phase == SIM_REC_SETTLE) sim_result(SIM_REC_IDLE, 0);
        return;
    }
    if (qosa_sim_get_sim_hot_swap(0, &cfg) || !cfg.enable ||
        (unsigned)cfg.insert_level != VOIP_DEMO_SIM_INSERT_LEVEL ||
        qosa_sim_get_slot_id(0, &slot) || slot != 0 ||
        qosa_sim_read_status(0, &sim) || sim != QOSA_SIM_STATUS_NOT_INSERTED) {
        settling = false;
        if (phase == SIM_REC_SETTLE) sim_result(SIM_REC_IDLE, 0);
        return;
    }
    if (!settling) { settling = true; settle_at = now; sim_result(SIM_REC_SETTLE, 0); return; }
    if ((uint32_t)(now - settle_at) < 5000U) return;
    settling = false;
    rc = qosa_dev_get_cfun(0, &cfun);
    if (rc) { sim_result(SIM_REC_ERROR, rc); return; }
    /* 不替用户解除手动设置的最小功能/飞行模式；仅恢复原本 CFUN1 的无卡识别。 */
    if (cfun != QOSA_DEV_FUNC_1) return;
    attempted = true;
    restore_left = 3;
    sim_result(SIM_REC_CFUN0, 0);
    (void)sim_set_cfun(0, now);
}

int voip_sim_diag_text(char *text, unsigned size)
{
    qosa_lpm_wakeup_pin_level_e level = QOSA_LPM_LEVEL_LOW;
    qosa_sim_hot_swap_cfg_t cfg = {0};
    qosa_uint8_t slot = 0xff, cfun = 0xff;
    int detrc = qosa_lpm_wakeup_pin_get_level(79, &level);
    int cfgrc = qosa_sim_get_sim_hot_swap(0, &cfg);
    int slotrc = qosa_sim_get_slot_id(0, &slot);
    int cfunrc = qosa_dev_get_cfun(0, &cfun);
    unsigned state;
    int rc;
    qosa_enter_critical(); state = phase; rc = recovery_rc; qosa_exit_critical();
    return qosa_snprintf(text, size,
        "+QVOIPSIM: det=%d,detrc=%d,enable=%d,level=%d,cfgrc=%d,slot=%d,slotrc=%d,cfun=%d,cfunrc=%d,recover=%u,recrc=%d",
        detrc ? -1 : (int)level, detrc, cfgrc ? -1 : (int)cfg.enable,
        cfgrc ? -1 : (int)cfg.insert_level, cfgrc, slotrc ? -1 : (int)slot,
        slotrc, cfunrc ? -1 : (int)cfun, cfunrc, state, rc);
}
