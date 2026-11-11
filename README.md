# UniRTOS VoIP 通话示例

版本 1.0.1，依赖 `unirtos-voip` 1.0.1。面向 EG800ZCN_LA、EG800Z QuecDuino EVB 和 ES8311，
支持板对板、板与 MicroSIP 的单路双向语音通话，以及账号配置、S2/S3、LED 和来电铃声。
信令使用 IPv4/SIP/UDP，网络语音为 PCMA（G.711 A-law，8 kHz）。
板级音频使用 16 kHz、16 bit、单声道 PCM，由组件完成采样率和编解码适配。

本包只提供通话示例，不包含独立录播、播放、单音、数字回环、寄存器测试或专用 Debug UART 程序。

## 下载与编译

完成源码仓库、组件/demo 索引及配套 SDK 的正式发布后：

```powershell
unirtos-cli new -r unirtos-voip-demos -v 1.0.1
cd unirtos-voip-demos-1.0.1
unirtos-cli env-setup
notepad .\voip_demo_config.h
unirtos-cli build -m EG800ZCN_LA -j 8 -v VOIP_DEMO
```

当前 `env_config.json` 使用 `1.0.4-voip.1` 本地配套 SDK；它尚不是官方发布版本。
该 SDK 配套音频头、库、平台底包及外部组件构建接口，不能直接混用官方原始 1.0.4。
本地未上传时的获取和构建结果见工作区交付说明；正式发布时必须同步登记对应 SDK 索引。

## 配置

所有用户配置均在随包提供的 `voip_demo_config.h`，无需另建或下载配置头。
服务器/密码初始为 `YOUR_SIP_SERVER`、`YOUR_SIP_PASSWORD`，填入实际值后重新编译。
保留占位值时，CMake 会提示配置未完成；开机报告 `+QVOIPRESULT: 0,10,-40`，不自动注册。
提交公开源码前恢复占位值。

| 宏 | 含义/默认值 |
|---|---|
| `VOIP_DEMO_SERVER`、`VOIP_DEMO_PORT` | SIP 服务器 IPv4 地址、实际 UDP 端口 |
| `VOIP_DEMO_ACCOUNT`、`VOIP_DEMO_PASSWORD` | 本板登录账号及密码，最长 31 字节 |
| `VOIP_DEMO_PEER` | AT 拨号不带号码时的默认对端 |
| `VOIP_DEMO_KEY_PEER` | S2 固定拨号对象，与运行时 AT 配置独立 |
| `VOIP_DEMO_AUTOSTART` | 1：有效配置下自动等待蜂窝附着、建立数据连接并注册 |
| `VOIP_DEMO_AUTO_DIAL` | 0：由 S2/AT 发起；1：首次注册成功后自动呼叫一次 |
| `VOIP_DEMO_AUTO_ANSWER` | 0：手动接听；1：自动接听来电 |
| `VOIP_DEMO_RETRY_MS` | 15000 毫秒：初始注册失败重试间隔；0 关闭该层重试 |
| `VOIP_DEMO_CODEC_ADDR` | 0：自动识别 ES8311 的 0x18/0x19 地址 |
| `VOIP_DEMO_CALL_VOLUME` | 9：接通后本板听到对端的音量 |
| `VOIP_DEMO_MIC_GAIN_PERCENT` | 200：本板上行 MIC 数字幅度为两倍，并限幅 |
| `VOIP_DEMO_RINGBACK_VOLUME` | 1：主叫 SIP 180/183 阶段收到的回铃媒体音量 |
| `VOIP_DEMO_RING_VOLUME` | 1：被叫来电铃声音量 |
| `VOIP_DEMO_RING_GAIN` | 50：铃声素材数字幅度百分比 |
| `VOIP_DEMO_RING_ENABLE` | 1：开启来电铃声 |

播放等级范围 0～11，0 静音；MIC 幅度范围 100～800，过高可能削顶。
服务器若先回复 SIP 200 再送等待音，该音频使用通话音量，不能单凭 SIP 状态区分真人语音。

板 A 使用账号 1001，PEER/KEY_PEER 为 1002；板 B 使用 1002，PEER/KEY_PEER 为 1001。
密码以服务器分配为准。先完成 1001 的编译测试；换账号时修改同一头文件重新编译即可，无需重复拉取。

## 通话测试

1. 开发板连接蜂窝天线、SIM、喇叭；确认 SIM 能提供数据连接。
2. 另一块板或 MicroSIP 使用独立账号，配置相同 SIP 服务器及实际端口。
3. MicroSIP 使用 UDP、PCMA/G.711 A-law，关闭媒体加密；按服务器要求填写 SIP 代理。
4. 烧录对应账号固件，在 USB AT 口查询 `AT+QVOIP?`，等待 `reg=1`。
5. 主叫按 S2 或发送 `AT+QVOIP=2,"1002"`。被叫响铃后按 S3 或发送 `AT+QVOIP=3` 接听。
6. 双方讲话确认双向声音；S3 或 `AT+QVOIP=4` 用于未接通取消、来电拒接或接通后挂断。
7. 重复取消后再拨号、交替主被叫，确认呼叫状态和声音恢复。

## USB AT 命令

命令使用英文引号，以回车结束。保留原通话操作编号：

| 命令 | 作用 |
|---|---|
| `AT+QVOIPCFG=?` / `AT+QVOIPCFG?` | 查看配置格式/是否有效和默认对端，不返回密码 |
| `AT+QVOIP=?` / `AT+QVOIP?` | 查看通话操作/状态和收发计数 |
| `AT+QVOIP=0` | 停止 SIP |
| `AT+QVOIP=1` | 联网并注册 |
| `AT+QVOIP=2` / `AT+QVOIP=2,"1002"` | 呼叫默认对端/指定账号 |
| `AT+QVOIP=3` | 接听来电 |
| `AT+QVOIP=4` | 取消拨号、拒接或挂断 |

`AT+QVOIP=5..12` 不再受理；配置内部结果编号 10 仍保留在 `+QVOIPRESULT` 中。
如需运行时设置账号，先停止 SIP，再配置并注册，逐步等待对应操作的完成结果：

```text
AT+QVOIP=0
AT+QVOIPCFG="192.0.2.10",5060,"1001","YOUR_PASSWORD","1002"
AT+QVOIP=1
```

上面的地址为文档示例，必须替换。AT 配置只作用于本次运行，不写回头文件，也不修改 S2 的固定号码。
`+QVOIPACCEPT` 和 `OK` 表示受理；`+QVOIPRESULT: 序号,操作,结果` 表示执行结果，0 为软件操作成功。
注册成功还需看 `reg=1`，接通后还需确认实际双向声音。

`state`：0 空闲、1 保持、2 拨号、3 接通、4 来电、5 等待、6 结束、7 振铃。
`tx/rx/capture/errors` 为通话媒体计数。主动查询还返回 `+QVOIPCALL` 和 `+QVOIPNET`，
用于判断呼叫阶段、来电信令与注册错误；不主动刷寄存器或周期 Debug 日志。

## 板级连接

以实际模组引脚及 EVB 原理图为准。

| 功能 | 模组引脚 | 行为 |
|---|---|---|
| S2 | Pin50，低有效 | 已注册且空闲时呼叫 KEY_PEER；未注册时先注册 |
| S3 | Pin51，低有效 | 来电接听；拨号取消；通话挂断 |
| D3 | Pin55 | 注册完成常亮，启动期间闪烁 |
| D4 | Pin56 | 来电/拨号闪烁，接通常亮，空闲/结束熄灭 |
| PA | Pin54 | 通话及来电铃声期间使能 |

D2 由平台控制。ES8311、I2C、GPIO、DAI 和功放适配位于 `boards/eg800z_quecduino/`。
本 demo 不接管 DBG UART；控制、状态查询和异步结果统一使用 USB AT。

## 目录和资源

- `voip_demo.c`：账号、网络、AT、按键与单路通话控制。
- `voip_demo_entry.c`：外部应用启动和两条自定义 AT 命令注册。
- `boards/eg800z_quecduino/voip_board.c/.h`：通话所需板级适配。
- `voip_ringtone.c/.h`、`telefone_ring_ulaw.inc`：已确认的来电铃声及播放逻辑。
- `telefone_ring_audio.json`：来电铃声转换来源记录。
- `server/*.example`：SIP 服务器配置参考。

保留来源与版权声明，详见 `PORTING.md` 及组件的 `THIRD_PARTY_NOTICES.md`。
本地编译验证与硬件通话验证分别记录；编译成功不代替开发板回归。
