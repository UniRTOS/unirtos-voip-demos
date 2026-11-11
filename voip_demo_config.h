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
/** 默认被叫；PC 或第二块板使用独立账号 1002。 */
#ifndef VOIP_DEMO_PEER
#define VOIP_DEMO_PEER "1002"
#endif /* VOIP_DEMO_PEER */
/** S2 固定呼叫账号，独立于 AT 配置中的默认对端；示例对端账号为 1002。 */
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
/** 初始注册失败后的重试间隔，单位毫秒；0 表示不自动重试。 */
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
/** 主叫 180/183 振铃阶段的下行媒体等级；接通后恢复 CALL_VOLUME，0 为静音。 */
#ifndef VOIP_DEMO_RINGBACK_VOLUME
#define VOIP_DEMO_RINGBACK_VOLUME 1
#endif /* VOIP_DEMO_RINGBACK_VOLUME */
#if VOIP_DEMO_RINGBACK_VOLUME < 0 || VOIP_DEMO_RINGBACK_VOLUME > 11
#error "VOIP_DEMO_RINGBACK_VOLUME must be in 0..11"
#endif /* VOIP_DEMO_RINGBACK_VOLUME */
/** 被叫来电铃声独立播放等级，1 为最轻的非静音档；0 为静音。 */
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
/** 响铃数字幅度百分比，0～100；50 将素材幅度减半，另受播放音量影响。 */
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
