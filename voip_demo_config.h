/**
 * @file voip_demo_config.h
 * @brief EG800Z QuecDuino SIP 通话演示配置；直接在本文件填写账号、服务器和音量。
 */
#ifndef VOIP_DEMO_CONFIG_H
#define VOIP_DEMO_CONFIG_H

/** 服务器 IPv4 地址；将 YOUR_SIP_SERVER 替换为实际地址，提交前恢复占位值。 */
#ifndef VOIP_DEMO_SERVER
#define VOIP_DEMO_SERVER "YOUR_SIP_SERVER"
#endif /* VOIP_DEMO_SERVER */
/** SIP UDP 端口，范围 1～65535。 */
#ifndef VOIP_DEMO_PORT
#define VOIP_DEMO_PORT 9999
#endif /* VOIP_DEMO_PORT */
/** 本端 SIP 用户名，最长 31 字节；第一块板默认使用 1001。 */
#ifndef VOIP_DEMO_ACCOUNT
#define VOIP_DEMO_ACCOUNT "1001"
#endif /* VOIP_DEMO_ACCOUNT */
/** SIP 密码，最长 31 字节；将 YOUR_SIP_PASSWORD 替换为实际密码，提交前恢复占位值。 */
#ifndef VOIP_DEMO_PASSWORD
#define VOIP_DEMO_PASSWORD "YOUR_SIP_PASSWORD"
#endif /* VOIP_DEMO_PASSWORD */
/** 上电时 AT 默认被叫；成功的 AT+QVOIPCFG 会同时更新 AT 与 S2 的运行时对端。 */
#ifndef VOIP_DEMO_PEER
#define VOIP_DEMO_PEER "1002"
#endif /* VOIP_DEMO_PEER */
/** S2 上电默认呼叫账号，可与 PEER 不同；示例对端账号为 1002。
 * 成功执行 AT+QVOIPCFG 后，S2 跟随命令的最后一个参数，不再呼叫旧编译号码。
 * AT 配置不持久化；重启后恢复此宏。失败的配置和单次指定号码拨号不会覆盖当前目标。
 */
#ifndef VOIP_DEMO_KEY_PEER
#define VOIP_DEMO_KEY_PEER "1002"
#endif /* VOIP_DEMO_KEY_PEER */
/**
 * @brief 上电自动注册开关。
 * 0：等待手动启动，按 S2 或发送 AT+QVOIP=1 发起注册。
 * 1：SIP 配置有效时，上电后自动建立数据连接并注册 SIP。
 */
#ifndef VOIP_DEMO_AUTOSTART
#define VOIP_DEMO_AUTOSTART 1
#endif /* VOIP_DEMO_AUTOSTART */
/**
 * @brief 注册后自动拨号开关。
 * 0：注册成功后，通过 S2 或 AT+QVOIP=2 手动拨号。
 * 1：注册成功后自动拨打 VOIP_DEMO_PEER 一次，手动启动 VoIP 时也生效。
 */
#ifndef VOIP_DEMO_AUTO_DIAL
#define VOIP_DEMO_AUTO_DIAL 0
#endif /* VOIP_DEMO_AUTO_DIAL */
/**
 * @brief 来电自动接听开关。
 * 0：收到来电后，按 S3 或发送 AT+QVOIP=3 接听。
 * 1：收到来电后自动接听。
 */
#ifndef VOIP_DEMO_AUTO_ANSWER
#define VOIP_DEMO_AUTO_ANSWER 0
#endif /* VOIP_DEMO_AUTO_ANSWER */
/* SIM1 热插拔：1 开启检测，支持无卡开机后插卡；0 关闭，须插卡后再上电。
 * 使用平台专用 SIM_DET（模组 Pin79），SDK 会保存此配置；停止 SIP 不关闭检测。 */
#ifndef VOIP_DEMO_SIM_HOT_SWAP
#define VOIP_DEMO_SIM_HOT_SWAP 1
#endif
/* 插卡时 SIM_DET 的电平：0 低，1 高。当前 QuecDuino EVB 实测拔卡为 0、插卡为 1。
 * Pin79 读数已在 2026-11-12 板测确认；其他板型须按实际卡座验证，不能沿用平台默认值。 */
#ifndef VOIP_DEMO_SIM_INSERT_LEVEL
#define VOIP_DEMO_SIM_INSERT_LEVEL 1
#endif
#if (VOIP_DEMO_SIM_HOT_SWAP != 0 && VOIP_DEMO_SIM_HOT_SWAP != 1) || \
    (VOIP_DEMO_SIM_INSERT_LEVEL != 0 && VOIP_DEMO_SIM_INSERT_LEVEL != 1)
#error "SIM hot swap and insert level must be 0 or 1"
#endif

/* 1：允许“检测到卡但仍报未插卡”时自动重新识卡；0：只依赖平台热插拔。
 * 仅在 SIP 未启动、允许自动注册、Pin79 有卡持续 5 秒且 CFUN=1 时执行 CFUN0→1。
 * 每次稳定插卡最多执行一次，不影响已注册/通话中的链路，不解除手动飞行模式。 */
#ifndef VOIP_DEMO_SIM_RECOVER
#define VOIP_DEMO_SIM_RECOVER 1
#endif
#if VOIP_DEMO_SIM_RECOVER != 0 && VOIP_DEMO_SIM_RECOVER != 1
#error "VOIP_DEMO_SIM_RECOVER must be 0 or 1"
#endif

/** 初始注册失败、运行中拔卡/PDP失效后的重试间隔，单位毫秒；0 关闭自动重试。
 * 拔卡时会清除旧 SIP 在线状态；重新插卡后恢复当前运行时账号和对端，不自动重拨通话。
 * AT+QVOIP=0 主动停止后不重连；AUTOSTART=0 时须先由 S2/AT 手动启动。
 */
#ifndef VOIP_DEMO_RETRY_MS
#define VOIP_DEMO_RETRY_MS 15000U
#endif /* VOIP_DEMO_RETRY_MS */
/** ES8311 的 7 位地址；0 自动识别 0x19/0x18，也可明确指定其中一个。 */
#ifndef VOIP_DEMO_CODEC_ADDR
#define VOIP_DEMO_CODEC_ADDR 0
#endif /* VOIP_DEMO_CODEC_ADDR */
/** 通话播放等级；平台 6 为原幅度，9 为 4 倍幅度，超幅由驱动限幅。 */
#ifndef VOIP_DEMO_CALL_VOLUME
#define VOIP_DEMO_CALL_VOLUME 9
#endif /* VOIP_DEMO_CALL_VOLUME */
/** 上行 MIC 在滤波后、PCMA 编码前的幅度百分比；200 为 2 倍。 */
#ifndef VOIP_DEMO_MIC_GAIN_PERCENT
#define VOIP_DEMO_MIC_GAIN_PERCENT 200
#endif /* VOIP_DEMO_MIC_GAIN_PERCENT */
/** 主叫 180/183 回铃 PCM 档位：0 静音，1 为 1/32 幅度，6 为原幅度。
 * 1～6 依次为 1/32、1/16、1/8、1/4、1/2、1 倍；7～11 为 2～6 倍。
 * 开启 ANSWERED_TONE_GUARD 时，主叫 SIP 200 后识别到的提示音也使用此档位。
 * 正常人声仍使用 CALL_VOLUME；改变音量后需重新编译、烧录。
 */
#ifndef VOIP_DEMO_RINGBACK_VOLUME
#define VOIP_DEMO_RINGBACK_VOLUME 1
#endif /* VOIP_DEMO_RINGBACK_VOLUME */
#if VOIP_DEMO_RINGBACK_VOLUME < 0 || VOIP_DEMO_RINGBACK_VOLUME > 11
#error "VOIP_DEMO_RINGBACK_VOLUME must be in 0..11"
#endif /* VOIP_DEMO_RINGBACK_VOLUME */
/** 提前应答服务器兼容：1 开启，0 关闭；默认 1。
 * 仅主叫 SIP 200 后检测常见 425/450 Hz 单音及 440+480 Hz 双音“嘟嘟”，
 * 匹配时按 RINGBACK_VOLUME 衰减；非提示音立即按 CALL_VOLUME 播放。
 * 检出至少 300 ms 非提示音后，本条播放流保持通话音量；新通话自动重新检测。
 * 这是音频识别，不能判断对方是否真正接听；音乐回铃/其它频率及相似语音可能误判。
 * 0：关闭识别，SIP 200 后所有远端声音均使用 CALL_VOLUME；来电本地铃声不受影响。
 */
#ifndef VOIP_DEMO_ANSWERED_TONE_GUARD
#define VOIP_DEMO_ANSWERED_TONE_GUARD 1
#endif /* VOIP_DEMO_ANSWERED_TONE_GUARD */
#if VOIP_DEMO_ANSWERED_TONE_GUARD != 0 && VOIP_DEMO_ANSWERED_TONE_GUARD != 1
#error "VOIP_DEMO_ANSWERED_TONE_GUARD must be 0 or 1"
#endif /* VOIP_DEMO_ANSWERED_TONE_GUARD */
/** 被叫来电铃声 PCM 档位，曲线同 RINGBACK_VOLUME；0 静音，1 为 1/32 幅度。
 * 直接缩放解码后的铃声，不降低接听后的通话音量；修改后须重新编译、烧录。
 */
#ifndef VOIP_DEMO_RING_VOLUME
#define VOIP_DEMO_RING_VOLUME 1
#endif /* VOIP_DEMO_RING_VOLUME */
#if VOIP_DEMO_CALL_VOLUME < 0 || VOIP_DEMO_CALL_VOLUME > 11
#error "VOIP_DEMO_CALL_VOLUME must be in 0..11"
#endif /* VOIP_DEMO_CALL_VOLUME */
#if VOIP_DEMO_MIC_GAIN_PERCENT < 100 || VOIP_DEMO_MIC_GAIN_PERCENT > 800
#error "VOIP_DEMO_MIC_GAIN_PERCENT must be in 100..800"
#endif /* VOIP_DEMO_MIC_GAIN_PERCENT */
#if VOIP_DEMO_RING_VOLUME < 0 || VOIP_DEMO_RING_VOLUME > 11
#error "VOIP_DEMO_RING_VOLUME must be in 0..11"
#endif /* VOIP_DEMO_RING_VOLUME */

/** 来电响铃开关；1 开启，0 关闭，只作用于当前单路来电。 */
#ifndef VOIP_DEMO_RING_ENABLE
#define VOIP_DEMO_RING_ENABLE 1
#endif /* VOIP_DEMO_RING_ENABLE */
/** 响铃素材幅度百分比，0～100；与 RING_VOLUME 相乘。默认 50% × 1/32 = 原素材 1/64。 */
#ifndef VOIP_DEMO_RING_GAIN
#define VOIP_DEMO_RING_GAIN 50
#endif /* VOIP_DEMO_RING_GAIN */
#if VOIP_DEMO_RING_ENABLE != 0 && VOIP_DEMO_RING_ENABLE != 1
#error "VOIP_DEMO_RING_ENABLE must be 0 or 1"
#endif /* VOIP_DEMO_RING_ENABLE */
#if VOIP_DEMO_RING_GAIN < 0 || VOIP_DEMO_RING_GAIN > 100
#error "VOIP_DEMO_RING_GAIN must be in 0..100"
#endif /* VOIP_DEMO_RING_GAIN */

#if VOIP_DEMO_PORT < 1 || VOIP_DEMO_PORT > 65535
#error "VOIP_DEMO_PORT must be in 1..65535"
#endif /* VOIP_DEMO_PORT */
#if VOIP_DEMO_CODEC_ADDR != 0 && VOIP_DEMO_CODEC_ADDR != 0x18 && VOIP_DEMO_CODEC_ADDR != 0x19
#error "VOIP_DEMO_CODEC_ADDR must be 0 (auto), 0x18 or 0x19"
#endif /* VOIP_DEMO_CODEC_ADDR */
#endif /* VOIP_DEMO_CONFIG_H */
