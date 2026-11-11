/**
 * @file voip_demo.c
 * @brief EG800Z QuecDuino 单路 SIP/PCMA 通话示例。
 * @note 只有控制任务调用阻塞接口；AT 和按键任务传递参数副本。
 */
#include "qosa_sys.h"
#include "qosa_rtc.h"
#include "qosa_at_cmd.h"
#include "qosa_at_param.h"
#include "qosa_urc.h"
#include "qosa_datacall.h"
#include "qosa_sockets.h"
#include "qcm_voip.h"
#include "voip_board.h"
#include "voip_demo_config.h"
#include "voip_ringtone.h"

typedef struct {
    unsigned op;                 /**< 0..4 为通话控制，10 为内部配置操作。 */
    unsigned sequence;           /**< 请求编号，0 为启动配置。 */
    qosa_at_dev_type_e port;      /**< AT 来源端口；自动操作使用 USB AT 端口。 */
    bool wait_response;          /**< 等待 AT 接收响应发出后再执行。 */
    qcm_voip_config_t config;     /**< 配置副本，处理完成后清零。 */
    char peer[32];               /**< 本次拨号或默认对端账号。 */
} voip_request_t;

static qosa_msgq_t requests;
static qosa_sem_t response_sent;
static qosa_task_t worker_task, io_task;
static bool busy, configured, started, auto_register, auto_dial_pending;
static int last_result, board_result;
static unsigned next_sequence, last_sequence, last_op, key_presses, rejected, urc_drops;
static uint32_t next_retry;
static char default_peer[32];
static qosa_at_dev_type_e notify_port = QOSA_DEV_NONE;
static qosa_datacall_conn_t connection = QOSA_DATACALL_CONN_INVALID;

/**
 * @brief 检查账号字符，避免向 SIP URI 或状态文本注入分隔符。
 * @param[in] text 以零结束的账号，最大 31 字节。
 * @return 合法返回 true，否则返回 false。
 */
static bool valid_peer(const char *text)
{
    unsigned n;
    if (!text || !text[0]) return false;
    for (n = 0; n < 32 && text[n]; ++n) {
        unsigned char c = (unsigned char)text[n];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || c == '_' || c == '-' || c == '.' || c == '+')) return false;
    }
    return n < 32;
}

/**
 * @brief 发送异步结果或状态通知，并累计发送失败次数。
 * @param[in] port AT 来源端口或默认 URC 端口。
 * @param[in] text 不含首尾换行的通知文本。
 * @return 无。
 */
static void report(qosa_at_dev_type_e port, char *text)
{
    if (qcm_urc_send_report(port, text, qosa_strlen(text), QOSA_URC_OUT_METHOD_UNICAST,
                            QOSA_RI_CATEG_OTHERS, QOSA_URC_SEND_DEFAULT) != QOSA_URC_SEND_RES_OK) {
        qosa_enter_critical(); ++urc_drops; qosa_exit_critical();
    }
}

/**
 * @brief 将请求副本交给唯一控制任务，串行保护 SIP 和音频资源。
 * @param[in,out] req 请求参数，成功后写入分配的编号。
 * @return 0 表示入队成功，非零表示忙或队列不可用。
 */
static int submit(voip_request_t *req)
{
    int ret;
    qosa_enter_critical();
    if (!requests || busy) { ++rejected; qosa_exit_critical(); return -1; }
    busy = true;
    req->sequence = ++next_sequence;
    if (!req->sequence) req->sequence = ++next_sequence;
    qosa_exit_critical();
    ret = qosa_msgq_release(requests, sizeof(*req), (unsigned char *)req, QOSA_NO_WAIT);
    if (ret) {
        qosa_enter_critical();
        busy = false; ++rejected; qosa_exit_critical();
    }
    return ret;
}

/**
 * @brief 使用 SIM0/PDP1 的 IPv4 地址注册 SIP 账号。
 * @return 0 表示注册成功，非零为板级、网络或 SIP 错误。
 * @note 仅控制任务调用；PDP1 与标准 AT 共享，停止 VoIP 不关闭 PDP。
 */
static int register_account(void)
{
    qosa_datacall_ip_info_t info = {0};
    char ip[16];
    int ret;
    if (!configured || started) return -1;
    if (board_result) return board_result;
    if ((ret = voip_board_audio_init())) return ret;
    if (!qosa_datacall_wait_attached(0, 60)) return -10;
    if (connection == QOSA_DATACALL_CONN_INVALID)
        connection = qosa_datacall_conn_new(0, 1, QOSA_DATACALL_CONN_TCPIP);
    if (connection == QOSA_DATACALL_CONN_INVALID) return -11;
    if (!qosa_datacall_get_status(connection) && (ret = qosa_datacall_start(connection, 60))) return ret;
    if ((ret = qosa_datacall_get_ip_info(connection, &info))) return ret;
    if (!inet_ntop(AF_INET, &info.ipv4_ip.apptcpip_ipv4_addr, ip, sizeof(ip)) ||
        !qosa_strcmp(ip, "0.0.0.0")) return -12;
    qosa_enter_critical(); started = true; qosa_exit_critical();
    ret = qcm_voip_start(ip);
    if (ret) {
        int stop_result = qcm_voip_stop();
        if (!stop_result) { qosa_enter_critical(); started = false; qosa_exit_critical(); }
    }
    return ret;
}

/**
 * @brief 执行一条串行请求。
 * @param[in] req 已验证并复制的请求。
 * @return 0 表示操作完成或 SIP 请求已提交；最终通话状态见状态通知。
 */
static int execute(const voip_request_t *req)
{
    int ret;
    // 接听请求可能异步创建媒体；必须先释放响铃，且等待状态变化期间不再起铃。
    if (req->op == 0 || req->op == 2 || req->op == 3 || req->op == 4 || req->op == 10) {
        if (voip_ringtone_stop(req->op == 3 || req->op == 4)) return -4;
    }
    switch (req->op) {
    case 0:
        qosa_enter_critical(); auto_register = false; auto_dial_pending = false; qosa_exit_critical();
        ret = qcm_voip_stop();
        if (!ret) { qosa_enter_critical(); started = false; qosa_exit_critical(); }
        return ret;
    case 1:
        ret = register_account();
        qosa_enter_critical();
        next_retry = qosa_get_running_time() + VOIP_DEMO_RETRY_MS;
        auto_register = ret != 0 && VOIP_DEMO_RETRY_MS != 0;
        auto_dial_pending = ret == 0 && VOIP_DEMO_AUTO_DIAL != 0;
        qosa_exit_critical();
        return ret;
    case 2:
        qosa_enter_critical(); auto_dial_pending = false; qosa_exit_critical();
        return qcm_voip_dial(req->peer[0] ? req->peer : default_peer);
    case 3: return qcm_voip_answer();
    case 4: return qcm_voip_hangup();
    case 10:
        if (started || !valid_peer(req->peer) || !req->config.password[0]) return -1;
        ret = qcm_voip_set_audio(VOIP_DEMO_CALL_VOLUME, VOIP_DEMO_MIC_GAIN_PERCENT);
        if (!ret) ret = qcm_voip_set_ringback_volume(VOIP_DEMO_RINGBACK_VOLUME);
        if (!ret) ret = qcm_voip_configure(&req->config);
        if (!ret) {
            qosa_enter_critical();
            configured = true;
            auto_register = false;
            qosa_memcpy(default_peer, req->peer, sizeof(default_peer));
            qosa_exit_critical();
        }
        return ret;
    default: return -1;
    }
}

/**
 * @brief 保存最近结果并发送带请求编号的 URC。
 * @param[in] req 已处理请求。
 * @param[in] result 执行结果。
 * @return 无。
 */
static void complete(const voip_request_t *req, int result)
{
    char text[96];
    int n;
    qosa_enter_critical();
    last_op = req->op; last_result = result; last_sequence = req->sequence;
    notify_port = req->port;
    qosa_exit_critical();
    n = qosa_snprintf(text, sizeof(text), "+QVOIPRESULT: %u,%u,%d", req->sequence, req->op, result);
    if (n > 0 && (unsigned)n < sizeof(text)) report(req->port, text);
    qosa_enter_critical();
    busy = false; qosa_exit_critical();
}

/**
 * @brief 从编译宏装载启动配置，不使用占位口令尝试注册。
 * @return 无。
 */
static void load_defaults(void)
{
    voip_request_t req = {0};
    int ret = -40;
    req.op = 10;
    req.port = QOSA_DEV_SIOLIB_USB_AT_PORT;
    if (sizeof(VOIP_DEMO_SERVER) <= sizeof(req.config.server) &&
        sizeof(VOIP_DEMO_ACCOUNT) <= sizeof(req.config.account) &&
        sizeof(VOIP_DEMO_PASSWORD) <= sizeof(req.config.password) &&
        sizeof(VOIP_DEMO_PEER) <= sizeof(req.peer) && VOIP_DEMO_SERVER[0] &&
        VOIP_DEMO_PASSWORD[0] && qosa_strcmp(VOIP_DEMO_SERVER, "YOUR_SIP_SERVER") &&
        qosa_strcmp(VOIP_DEMO_PASSWORD, "YOUR_SIP_PASSWORD") &&
        qosa_strcmp(VOIP_DEMO_PASSWORD, "REPLACE_ME")) {
        qosa_strlcpy(req.config.server, VOIP_DEMO_SERVER, sizeof(req.config.server));
        qosa_strlcpy(req.config.account, VOIP_DEMO_ACCOUNT, sizeof(req.config.account));
        qosa_strlcpy(req.config.password, VOIP_DEMO_PASSWORD, sizeof(req.config.password));
        qosa_strlcpy(req.peer, VOIP_DEMO_PEER, sizeof(req.peer));
        req.config.port = VOIP_DEMO_PORT;
        ret = execute(&req);
    }
    qosa_enter_critical();
    auto_register = !ret && VOIP_DEMO_AUTOSTART != 0;
    next_retry = qosa_get_running_time();
    qosa_exit_critical();
    complete(&req, ret);
    qosa_memset(&req, 0, sizeof(req));
}

/** @brief 格式化不含敏感字段的来电/注册计数。
 * @param[in] const qcm_voip_net_diag_t *diag 诊断快照。
 * @param[out] char *text 输出缓冲。
 * @param[in] unsigned size 输出容量，单位字节。
 * @return snprintf 的长度；调用者校验截断。
 */
static int net_diag_text(const qcm_voip_net_diag_t *diag, char *text, unsigned size)
{
    return qosa_snprintf(text, size,
        "+QVOIPNET: invite=%lu,incoming=%lu,busy=%lu,error=%lu,drop=%lu,cid=%ld,rc=%ld,regfail=%lu,regcode=%ld",
        (unsigned long)diag->invites_rx, (unsigned long)diag->incoming,
        (unsigned long)diag->busy_reject, (unsigned long)diag->invite_errors,
        (unsigned long)diag->queue_drops, (long)diag->invite_cid, (long)diag->invite_result,
        (unsigned long)diag->reg_failures, (long)diag->reg_code);
}

/**
 * @brief 控制任务入口，执行耗时操作并清理配置副本。
 * @param[in] arg 未使用。
 * @return 无，常驻任务。
 */
static void worker(void *arg)
{
    voip_request_t req;
    int ret;
    (void)arg;
    load_defaults();
    for (;;) {
        qcm_voip_status_t status;
        // 队列到达立即处理；空闲时每 20 ms 补响铃帧，不占用 AT/按键的 busy 标志。
        if (!qosa_msgq_wait(requests, (unsigned char *)&req, sizeof(req), 20)) {
            ret = req.wait_response ? qosa_sem_wait(response_sent, QOSA_WAIT_FOREVER) : 0;
            if (!ret) ret = execute(&req);
            complete(&req, ret);
            qosa_memset(&req, 0, sizeof(req));
        }
        qcm_voip_get_status(&status);
        voip_ringtone_poll(&status, started && !board_result);
    }
}

/**
 * @brief 将双键事件转成串行控制请求，同时按下时优先处理 S3。
 * @param[in] int keys VOIP_BOARD_KEY_CALL/CONTROL 新按下位图。
 * @param[in] const qcm_voip_status_t *status 本轮 SIP 状态快照。
 * @note S2 仅空闲时呼叫固定账号；S3 空闲不拨号，来电接听，其余活动态挂断。
 */
static void handle_keys(int keys, const qcm_voip_status_t *status)
{
    voip_request_t req = {0};
    bool occupied, ready;
    int ret;
    bool control = (keys & VOIP_BOARD_KEY_CONTROL) != 0;
    qosa_enter_critical();
    key_presses += !!(keys & VOIP_BOARD_KEY_CALL) + !!(keys & VOIP_BOARD_KEY_CONTROL);
    occupied = busy; ready = configured;
    qosa_exit_critical();
    if (occupied) {
        return;
    }
    if (control) {
        if (status->state == QCM_VOIP_INCOMING || status->state == QCM_VOIP_WAITING) req.op = 3;
        else if (status->state != QCM_VOIP_IDLE && status->state != QCM_VOIP_ENDED) req.op = 4;
        else {
            return;
        }
    } else if (keys & VOIP_BOARD_KEY_CALL) {
        if (status->state != QCM_VOIP_IDLE && status->state != QCM_VOIP_ENDED) {
            return;
        }
        if (!ready || !valid_peer(VOIP_DEMO_KEY_PEER)) {
            return;
        }
        req.op = status->registered ? 2 : 1;
        if (req.op == 2) qosa_strlcpy(req.peer, VOIP_DEMO_KEY_PEER, sizeof(req.peer));
    } else return;
    req.port = QOSA_DEV_SIOLIB_USB_AT_PORT;
    ret = submit(&req);
    if (!ret && req.op == 2) {
        qosa_enter_critical(); auto_dial_pending = false; qosa_exit_critical();
    }
}

/** @brief 格式化呼叫诊断；只含编号、阶段和错误，不含账号密码。
 * @param[in] const qcm_voip_call_diag_t *diag 已获取的快照。
 * @param[out] char *text 输出缓冲。
 * @param[in] unsigned size 缓冲容量，单位字节。
 * @return 完整输出长度；负值或不小于 size 表示失败或截断。
 */
static int call_diag_text(const qcm_voip_call_diag_t *diag, char *text, unsigned size)
{
    return qosa_snprintf(text, size,
        "+QVOIPCALL: try=%lu,role=%lu,cid=%ld,stage=%u,fail=%u,rc=%ld,event=%ld,sip=%ld,rec=%ld,play=%ld,senderr=%lu,playerr=%lu",
        (unsigned long)diag->attempt, (unsigned long)diag->role, (long)diag->cid,
        (unsigned)diag->stage, (unsigned)diag->fail_stage, (long)diag->fail_rc,
        (long)diag->sip_event, (long)diag->sip_code, (long)diag->record_open,
        (long)diag->play_open, (unsigned long)diag->send_errors, (unsigned long)diag->play_errors);
}

/**
 * @brief 轮询按键、LED 和自动注册/拨号/接听，不执行阻塞控制。
 * @param[in] arg 未使用。
 * @return 无，常驻任务。
 */
static void io_worker(void *arg)
{
    qcm_voip_status_t s;
    voip_request_t req;
    unsigned old_state = ~0U;
    int old_reg = -1;
    bool answered = false;
    (void)arg;
    for (;;) {
        bool running, retry, dial, occupied;
        qosa_at_dev_type_e port;
        uint32_t retry_at;
        int keys;
        qcm_voip_get_status(&s);
        qosa_enter_critical();
        running = started; retry = auto_register; dial = auto_dial_pending;
        retry_at = next_retry; occupied = busy; port = notify_port;
        qosa_exit_critical();
        voip_board_update(&s, running);
        if (old_state != (unsigned)s.state || old_reg != (int)s.registered) {
            char text[80];
            int n = qosa_snprintf(text, sizeof(text), "+QVOIPSTATE: %u,%u,%u",
                                 (unsigned)s.registered, (unsigned)s.state, (unsigned)s.call_id);
            if (n > 0 && (unsigned)n < sizeof(text)) report(port, text);
            old_state = s.state; old_reg = s.registered;
        }
        if (s.state != QCM_VOIP_INCOMING && s.state != QCM_VOIP_WAITING) answered = false;
        qosa_memset(&req, 0, sizeof(req));
        req.port = QOSA_DEV_SIOLIB_USB_AT_PORT;
        keys = voip_board_key();
        if (keys) {
            handle_keys(keys, &s);
        } else if (!occupied && retry && !running && (int32_t)(qosa_get_running_time() - retry_at) >= 0) {
            req.op = 1; submit(&req);
        } else if (!occupied && VOIP_DEMO_AUTO_ANSWER && !answered &&
                   (s.state == QCM_VOIP_INCOMING || s.state == QCM_VOIP_WAITING)) {
            req.op = 3; if (!submit(&req)) answered = true;
        } else if (!occupied && dial && s.registered &&
                   (s.state == QCM_VOIP_IDLE || s.state == QCM_VOIP_ENDED)) {
            req.op = 2;
            if (!submit(&req)) { qosa_enter_critical(); auto_dial_pending = false; qosa_exit_critical(); }
        }
        qosa_task_sleep_ms(20);
    }
}

/**
 * @brief 有界复制 AT 字符串参数。
 * @param[in] cmd AT 命令；index 为已确认存在的参数下标。
 * @param[in] index 参数下标。
 * @param[out] out 目标缓冲区。
 * @param[in] size 目标容量，含结尾零字节。
 * @return 非空且完整复制返回 true。
 */
static bool copy_param(qosa_at_cmd_t *cmd, unsigned index, char *out, size_t size)
{
    qosa_bool_t valid = QOSA_TRUE;
    const char *text = qosa_at_param_string(cmd->params[index], &valid);
    size_t len;
    if (!valid || !text) return false;
    len = qosa_strnlen(text, size);
    if (!len || len >= size) return false;
    qosa_memcpy(out, text, len + 1);
    return true;
}

/**
 * @brief 应答已受理的 AT 请求，然后允许控制任务执行。
 * @param[in] cmd AT 来源命令。
 * @param[in,out] req 参数副本。
 * @return 无；发送一次 OK 或 ERROR。
 */
static void accept_command(qosa_at_cmd_t *cmd, voip_request_t *req)
{
    char text[64];
    req->port = cmd->dev_port;
    req->wait_response = true;
    if (submit(req)) { qosa_at_resp_error(cmd->dev_port); return; }
    /* 两个 unsigned 十进制数最多 20 字节，64 字节缓冲区足够。 */
    qosa_snprintf(text, sizeof(text), "+QVOIPACCEPT: %u,%u", req->sequence, req->op);
    qosa_at_resp_text(cmd->dev_port, text);
    qosa_sem_release(response_sent);
}

/**
 * @brief 配置服务器、端口、账号、口令和默认对端；不回显口令。
 * @param[in] cmd 标准 AT 分发对象。
 * @return 无；配置完成结果由 QVOIPRESULT 通知。
 */
void voip_demo_config_cmd(qosa_at_cmd_t *cmd)
{
    voip_request_t req = {0};
    qosa_bool_t valid = QOSA_TRUE;
    if (cmd->type == QOSA_AT_CMD_TEST) {
        qosa_at_resp_text(cmd->dev_port, "+QVOIPCFG: \"server_ipv4\",port,\"account\",\"password\",\"default_peer\"");
        return;
    }
    if (cmd->type == QOSA_AT_CMD_READ) {
        char text[80], peer[32];
        bool ready;
        qosa_enter_critical(); ready = configured; qosa_memcpy(peer, default_peer, sizeof(peer)); qosa_exit_critical();
        qosa_snprintf(text, sizeof(text), "+QVOIPCFG: %u,\"%s\"", (unsigned)ready, peer);
        qosa_at_resp_text(cmd->dev_port, text);
        return;
    }
    if (cmd->type != QOSA_AT_CMD_SET || cmd->param_count != 5) goto error;
    req.op = 10;
    if (!copy_param(cmd, 0, req.config.server, sizeof(req.config.server)) ||
        !copy_param(cmd, 2, req.config.account, sizeof(req.config.account)) ||
        !copy_param(cmd, 3, req.config.password, sizeof(req.config.password)) ||
        !copy_param(cmd, 4, req.peer, sizeof(req.peer)) ||
        !valid_peer(req.config.account) || !valid_peer(req.peer)) goto error;
    req.config.port = qosa_at_param_uint_in_range(cmd->params[1], 1, 65535, &valid);
    if (!valid) goto error;
    accept_command(cmd, &req);
    qosa_memset(&req, 0, sizeof(req));
    return;
error:
    qosa_memset(&req, 0, sizeof(req));
    qosa_at_resp_error(cmd->dev_port);
}

/**
 * @brief 控制通话或查询计数器；查询不占用阻塞控制任务。
 * @param[in] cmd 标准 AT 分发对象。
 * @return 无；控制结果由 QVOIPRESULT 通知。
 */
void voip_demo_control_cmd(qosa_at_cmd_t *cmd)
{
    voip_request_t req = {0};
    qosa_bool_t valid = QOSA_TRUE;
    if (cmd->type == QOSA_AT_CMD_TEST) {
        qosa_at_resp_text(cmd->dev_port, "+QVOIP: 0=stop,1=register,2=dial[,\"peer\"],3=answer,4=hangup");
        return;
    }
    if (cmd->type == QOSA_AT_CMD_READ) {
        qcm_voip_status_t s;
        char text[400];
        bool b, run;
        int result, board, n;
        unsigned op, keys, seq, drops, rejects;
        qcm_voip_get_status(&s);
        qosa_enter_critical();
        b = busy; run = started; result = last_result; board = board_result;
        op = last_op; keys = key_presses; seq = last_sequence; drops = urc_drops; rejects = rejected;
        qosa_exit_critical();
        n = qosa_snprintf(text, sizeof(text),
            "+QVOIP: init=%u,start=%u,reg=%u,state=%u,busy=%u,seq=%u,op=%u,result=%d,board=%d,codec=%u,io=%d,tx=%lu,rx=%lu,capture=%lu,errors=%lu,keys=%u,reject=%u,urcdrop=%u",
            (unsigned)s.initialized, (unsigned)run, (unsigned)s.registered, (unsigned)s.state,
            (unsigned)b, seq, op, result, board, voip_board_codec_address(), voip_board_io_error(),
            (unsigned long)s.tx_packets, (unsigned long)s.rx_packets,
            (unsigned long)s.capture_frames, (unsigned long)s.capture_errors, keys, rejects, drops);
        if (n <= 0 || (unsigned)n >= sizeof(text)) { qosa_at_resp_error(cmd->dev_port); return; }
        {
            qcm_voip_call_diag_t diag = {0};
            qosa_at_resp_ntext(cmd->dev_port, text, (unsigned)n);
            qcm_voip_get_call_diag(&diag);
            n = call_diag_text(&diag, text, sizeof(text));
            if (n <= 0 || (unsigned)n >= sizeof(text)) { qosa_at_resp_error(cmd->dev_port); return; }
            qosa_at_resp_ntext(cmd->dev_port, text, (unsigned)n);
            qcm_voip_net_diag_t net = {0};
            qcm_voip_get_net_diag(&net);
            n = net_diag_text(&net, text, sizeof(text));
            if (n <= 0 || (unsigned)n >= sizeof(text)) { qosa_at_resp_error(cmd->dev_port); return; }
            qosa_at_resp_text(cmd->dev_port, text);
        }
        return;
    }
    if (cmd->type != QOSA_AT_CMD_SET || cmd->param_count < 1 || cmd->param_count > 2) goto error;
    req.op = qosa_at_param_uint_in_range(cmd->params[0], 0, 4, &valid);
    if (!valid) goto error;
    if (cmd->param_count == 2 && (req.op != 2 || !copy_param(cmd, 1, req.peer, sizeof(req.peer)) ||
                                !valid_peer(req.peer))) goto error;
    accept_command(cmd, &req);
    return;
error:
    qosa_at_resp_error(cmd->dev_port);
}

/**
 * @brief 初始化板级资源、命令队列与常驻任务。
 * @return 无；资源错误由 QVOIP 查询的 board 字段返回。
 */
void voip_demo_start(void)
{
    if (requests) return;
    notify_port = QOSA_DEV_SIOLIB_USB_AT_PORT;
    board_result = voip_board_init();
    if (qosa_sem_create(&response_sent, 0)) { board_result = -31; return; }
    if (qosa_msgq_create(&requests, sizeof(voip_request_t), 2)) {
        requests = NULL; qosa_sem_delete(response_sent); response_sent = NULL; board_result = -32; return;
    }
    busy = true;
    if (qosa_task_create(&worker_task, 6144, QOSA_PRIORITY_NORMAL, "voip_demo", worker, NULL)) {
        qosa_msgq_delete(requests); requests = NULL;
        qosa_sem_delete(response_sent); response_sent = NULL; busy = false; board_result = -33; return;
    }
    if (qosa_task_create(&io_task, 3072, QOSA_PRIORITY_NORMAL, "voip_io", io_worker, NULL)) board_result = -30;
}
