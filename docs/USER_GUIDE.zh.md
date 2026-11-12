# VOIP语音通话

***Copyright © Quectel Wireless Solutions Co., Ltd. 2026. All rights reserved.***

---

## 修订记录

| 日期 | 作者 | 说明 |
| --- | --- | --- |
| 2026-11-11 | Clive Wu | 初始通话说明 |
| 2026-11-12 | Clive Wu | 完善 CLI 使用流程、配置及 AT 指令 |

## 项目概述

　　UniRTOS VoIP 通话方案是基于 EG800Z QuecDuino EVB 的双向语音示例。开发板通过蜂窝数据网络连接 SIP 服务，注册后可以与另一块开发板通话，也可以与 PC 上的 MicroSIP 通话。示例演示 SIP 注册、呼叫、接听和挂断。

　　方案将通话组件与示例应用分开。组件负责 SIP 信令、RTP 收发和 PCMA 编解码。示例负责蜂窝数据连接、账号配置、AT 控制，以及 QuecDuino EVB 的按键、指示灯和来电铃声。填好账号后，可以上电自动注册，用 S2 拨号、S3 接听或挂断，也可通过 USB AT 操作。

　　当前支持单路通话。双板用于验证两端按键、灯和双向语音；单板时可用 MicroSIP 作为另一端。

## 开发资源

| 资源 | 双板/单板+PC | 说明 |
| --- | --- | --- |
| EG800Z QuecDuino EVB | 2 / 1 | 与本文物理引脚映射一致 |
| 喇叭、SIM、天线、USB 线 | 各 2 / 各 1 | MIC 使用 EVB 板载器件 |
| SIP 服务和账号 | 1 套服务、2 个账号 | 允许 SIP/UDP 和 PCMA/RTP |
| PC 软件 | QFlash、串口助手、MicroSIP | MicroSIP 仅单板互拨时需要 |

## 功能特性

- 支持两块 EG800Z QuecDuino 开发板互相通话，也支持开发板与 PC MicroSIP 通话。
- 使用 SIP/UDP 注册与呼叫控制，支持拨号、接听、拒接、未接通取消及通话挂断。
- RTP 承载 G.711 A-law（PCMA，8 kHz，payload type 8）；板端使用 ES8311 的 16 kHz、16 bit、单声道 PCM。
- 支持有效配置下上电自动联网注册，也可手动启动、自动拨号一次或自动接听。
- S2 拨号，S3 接听/取消/挂断；D2、D3、D4 分别显示蜂窝网络、SIP 注册和呼叫状态。
- 支持来电铃声，通话音量、麦克风增益、来电铃声和主叫回铃分别配置。
- 支持 USB AT 修改本次开机的账号和对端、控制通话、查询当前诊断；不输出累加流水号和媒体计数。
- 支持无卡上电后插卡恢复、运行中拔卡/数据连接丢失后清理旧会话并重新注册；不自动重拨被打断的通话。
- 组件与 Demo 分仓库，兼容 GitHub 官方 SDK；不修改 SDK 源码和底包。

## 系统组成

| 模块 | 内容 |
| --- | --- |
| unirtos-voip 组件 | SIP/RTP、PCMA、重采样、通话控制及 UniRTOS 接口适配 |
| unirtos-voip-demos | 联网、配置、USB AT、SIM 恢复、ES8311、按键、灯和铃声 |
| 官方 UniRTOS SDK | 操作系统、网络、音频流和平台底包；由 CLI 获取，不修改 |
| SIP 服务 | 管理分机注册和呼叫路由，按部署转发 RTP 媒体 |
| 对端 | 另一块 EG800Z QuecDuino EVB，或运行 MicroSIP 的 PC |

　　上行：板载 MIC → ES8311 → 16 kHz PCM → 重采样到 8 kHz → PCMA 编码 → RTP。
下行：RTP → PCMA 解码 → 重采样到 16 kHz → ES8311 → PA → 喇叭。
SIP 信令和 RTP 是不同的数据通路；注册成功不能单独证明双向语音正常。

## 快速上手

### 硬件连接

1. 准备一块或两块 EG800Z QuecDuino EVB，接好 USB、蜂窝天线、支持数据业务的 SIM 和喇叭，录音使用板载 MIC。
2. 喇叭接实际原理图的 J10。需要模组控制功放时，J13 的 1–2 短接至模组 Pin54；2–3 为 3.3 V 上拉。按实际 EVB 接线核对，不能仅根据排针丝印判断。
3. 准备两端均可访问的 IPv4 SIP 服务及两个独立账号。下面使用 1001/1002 示例，密码和 UDP 端口由服务器分配，服务器允许 PCMA 并正确路由媒体。
4. PC 准备 QFlash 和串口助手；单板测试另外准备 MicroSIP。

| 资源 | 模组物理引脚/接口 | 用途 |
| --- | --- | --- |
| S2 | Pin50，低有效 | 配置有效但未注册时启动注册；已注册且空闲时呼叫当前 S2 对端 |
| S3 | Pin51，低有效 | 来电时接听；主叫等待时取消；接通后挂断；空闲时无动作 |
| D2 | Pin16 / NET_STATUS | 未驻网亮 200 ms/灭 1800 ms；已驻网亮 1800 ms/灭 200 ms；最近有本 Demo RTP 时亮灭各 125 ms |
| D3 | Pin55 | SIP 注册成功常亮；服务启动但未注册时闪烁；停止或确认失联时熄灭 |
| D4 | Pin56 | 拨号、振铃、来电时闪烁；SIP 接通常亮；空闲/结束熄灭 |
| PA | Pin54 | 由音频播放状态控制外部功放 |
| SIM_DET | Pin79 | 当前 EVB 拔卡为 0、插卡为 1 |
| USB AT | Quectel USB AT Port | 命令、执行结果和状态查询 |

　　D2 是网络状态闪灯，不是 PWM 渐亮渐暗的呼吸灯。官方底包未启用平台 LED 服务，
此 Demo 通过公开 GPIO 接口控制它。D3 注册过程较快时可能看不到明显闪烁，最终以 reg=1 和常亮为准。
服务器若提前 SIP 200，主叫 D4 可以先常亮，不能将它当作实际被叫已经接听的唯一证据。

### 软件部署

#### 代码拉取

　　先安装 Python、Git、UniRTOS 工具链和 unirtos-cli。安装 CLI 可用 `pip install unirtos-cli`，
然后检查以下命令是否可用：

```bash
python --version
git --version
unirtos --version
unirtos-cli version
```

　　在存放工程的目录执行：

```bash
unirtos-cli ls-demos -f
unirtos-cli new -r unirtos-voip-demos
```

　　进入 CLI 创建成功后提示的工程目录，其中已经包含 voip_demo_config.h。
env_config.json 管理官方 SDK、组件依赖和默认模组 EG800ZCN_LA。

#### 配置账号与音量

　　打开下载的 Demo 根目录 voip_demo_config.h，修改已有宏。此文件随源码上传并随 CLI 下载，
不需要另外下载或创建 voip_demo_local.h。下面为板 A 的示例；地址、端口和密码必须替换为实际配置。

```c
#define VOIP_DEMO_SERVER      "YOUR_SIP_SERVER"   // SIP 服务器 IPv4 地址
#define VOIP_DEMO_PORT        9999                // SIP UDP 端口
#define VOIP_DEMO_ACCOUNT     "1001"              // 本机账号
#define VOIP_DEMO_PASSWORD    "YOUR_SIP_PASSWORD" // 本机账号密码，不是对端密码
#define VOIP_DEMO_PEER        "1002"              // 默认 AT 拨号和自动拨号目标
#define VOIP_DEMO_KEY_PEER    "1002"              // 上电时 S2 拨号目标

// 1：配置有效时自动联网注册；0：等待 S2 或 AT+QVOIP=1 手动启动。
#define VOIP_DEMO_AUTOSTART   1
// 1：启动注册成功后自动拨号一次；0：使用 S2 或 AT 手动拨号。
#define VOIP_DEMO_AUTO_DIAL   0
// 1：收到来电自动接听；0：按 S3 或发送 AT+QVOIP=3 接听。
#define VOIP_DEMO_AUTO_ANSWER 0
```

| 设备 | 本机账号及对应密码 | PEER | KEY_PEER |
| --- | --- | --- | --- |
| 板 A | 1001 / 服务端分配给 1001 的密码 | 1002 | 1002 |
| 板 B | 1002 / 服务端分配给 1002 的密码 | 1001 | 1001 |
| PC MicroSIP | 单板测试时登录 1002 | 手动拨 1001 | 不适用 |

　　普通人工通话测试保持三个开关为 1/0/0。AUTOSTART=0 是合法配置，不会损坏功能，
只是上电不自动注册；配置有效后手动启动并等 reg=1，即可拨号和收来电。
AUTO_DIAL=1 时手动启动注册也会拨号一次，但断网恢复不自动重拨旧通话。

　　PEER 和 KEY_PEER 在启动时可不同；成功的 AT+QVOIPCFG 配置会把最后一个参数同时用于
本次开机的 AT 默认目标和 S2 目标。重启恢复头文件值。单次 AT 指定号码不修改默认目标。

| 音频宏 | 发布默认值 | 调节效果 |
| --- | --- | --- |
| VOIP_DEMO_CALL_VOLUME | 9 | 正常通话本板喇叭音量，0～11，0 静音 |
| VOIP_DEMO_MIC_GAIN_PERCENT | 200 | 本板上行数字增益，100～800%；影响对端听到的声音，过大可能削顶 |
| VOIP_DEMO_RING_VOLUME | 1 | 被叫本地来电铃声档位，0～11 |
| VOIP_DEMO_RINGBACK_VOLUME | 1 | 主叫接通前回铃及兼容识别出的提前应答“嘟嘟”档位，0～11 |
| VOIP_DEMO_RING_GAIN | 50 | 铃声素材幅度百分比，0～100 |
| VOIP_DEMO_RING_ENABLE | 1 | 1 开来电铃声，0 关闭 |
| VOIP_DEMO_ANSWERED_TONE_GUARD | 1 | 1 兼容服务器提前应答后的提示音，0 关闭该识别 |

　　铃声档位采用 PCM 幅度控制，1 档是原始幅度的 1/32；来电素材再乘 RING_GAIN。
正常人声使用原通话曲线，默认 9 档保持不变。主叫提前 SIP 200 时，板端识别常见的
425/450 Hz 或 440+480 Hz 等待音并采用回铃档位，识别到非提示音后恢复通话音量。
这是音频启发式兼容，音乐、不同频率或相似人声可能误判；不等于检测到了实际被叫接听。
修改宏后必须重新编译烧录，可通过 AT+QVOIP? 的 QVOIPAUDIO 行检查实际固件值。

| 网络/SIM 宏 | 默认值 | 含义 |
| --- | --- | --- |
| VOIP_DEMO_RETRY_MS | 15000 | 注册失败或失联清理后重试间隔，毫秒；0 关闭重试 |
| VOIP_DEMO_SIM_HOT_SWAP | 1 | 使用公开 API 启用 SIM 检测；SDK 会保存其配置 |
| VOIP_DEMO_SIM_INSERT_LEVEL | 1 | 当前 EVB 插卡高有效；其它板须实测 |
| VOIP_DEMO_SIM_RECOVER | 1 | 稳定插卡但仍报未插卡时，允许有限次数 CFUN0→1 重新识卡 |
| VOIP_DEMO_CODEC_ADDR | 0 | 自动识别 ES8311 的 0x18/0x19 地址 |

#### 构建项目

　　完成配置后，在刚创建的 Demo 工程目录打开终端，执行：

```bash
unirtos-cli env-setup
unirtos-cli build
```

　　env-setup 拉取依赖，build 调用官方 SDK 构建入口，默认模组来自 env_config.json。
固件位于当前工程的以下路径，工程名称以实际目录为准：

```text
qos_build/release/<工程名称>/at_command.hbinpkg
```

　　配置变更后只需在同一目录再次运行 unirtos-cli build。更换账号不需重新下载工程。
两块板可先编译并保存 1001 产物，再改为 1002 并编译；下一次打包可能清理旧输出，先保存完整 release 目录。
若希望产物目录带账号标签，可选用：

```bash
unirtos-cli build -v VOIP_1001
unirtos-cli build -v VOIP_1002
```

　　-v 只改变固件标签，不会自动选择 SIP 账号。账号必须在头文件中修改，或烧录后使用 AT 临时配置。
发布占位配置能编译，但不能直接联网注册，开机可能报告 +QVOIPRESULT: 10,-40。

### 验证项目

#### 烧录与开机检查

1. 用 QFlash 加载本次保存的 release 目录/at_command.hbinpkg，选择目标板实际下载端口并执行下载。
2. 显示 PASS 后重启。USB AT 口发送 AT，正常返回 OK。
3. 发送 AT+QVOIPCFG?，核对本机账号、默认目标和 S2 目标。
4. 发送 AT+QVOIP?，先确认 sim=0，再等待 reg=1、D3 常亮；默认自动完成数据连接和 SIP 注册。
5. AUTOSTART=0 时手动发 AT+QVOIP=1。不要把 start=1 或 regcode=200 的历史结果单独当作当前在线。

#### 通话操作：双板互拨

1. 板 A 使用 1001，板 B 使用 1002；两端均查询到 reg=1。
2. 板 A 按 S2，或发送 AT+QVOIP=2,"1002"。
3. 板 B 来电时 D4 闪烁、播放铃声；按 S3 或发 AT+QVOIP=3 接听。
4. 两端分别对 MIC 讲话，确认双方都能听清；以实际双向声音验收，不能仅凭 state=3。
5. 任一端按 S3 或发 AT+QVOIP=4 挂断；确认两端 state=6/0、D4 熄灭，reg 仍为 1。
6. 交换主被叫重复测试。再做“被叫不接听→主叫 S3 取消→立即重拨”，确认被叫能重新响铃并接通。

#### 通话操作：开发板与 MicroSIP 互拨

　　开发板用 1001，PC 用另一个账号 1002。若另一块板也在使用 1002，先停用它的 VoIP，
避免同账号多终端绑定影响路由。无需把 PC 密码写成板子的密码，两端分别使用各自分机密码。

| MicroSIP 字段 | 设置 |
| --- | --- |
| Account Name | 任意便于识别的名称，例如 PC-1002 |
| SIP Server / Domain | 按服务端要求填写服务器地址或域 |
| SIP Proxy | 需要代理/非默认端口时按服务端要求填，例如服务器地址:9999 |
| Username / Login | PC 的 SIP 账号/认证用户名，通常为 1002 |
| Password | 分配给 PC 账号的密码 |
| Transport | UDP |
| Codec | 启用 PCMA / G.711 A-law；首次联调可仅保留 PCMA |
| Media Encryption | Disabled |
| 输入/输出音频设备 | 选择 PC 实际麦克风与扬声器/耳机 |

　　MicroSIP 显示 Online 且板端 reg=1 后，先由板子呼叫 1002，在 PC 接听；
再由 PC 拨打 1001，板子 S3 接听。每次检查双向声音，并分别从不同端挂断。
SIP Online 只代表注册成功，RTP 媒体还需要服务器、NAT/防火墙和音频设备正常。

#### SIM 后插卡与运行中重连

　　默认允许自动启动和重试。无卡开机后插卡，Demo 检查 Pin79、SIM 状态，必要时执行
有界 CFUN0→1 恢复，再等待蜂窝附着和 SIP 注册；无需手动发驻网 AT。
正常运行中拔卡或 PDP 失效，旧会话被标记失效、D3 不再保持假在线，控制任务停止旧 SIP/媒体；
插回卡并恢复数据连接后重新取得 IP、注册 SIP。当前 AT 配置的账号和对端会保留，旧通话不自动重拨。

1. 注册后记下 AT+QVOIPCFG? 返回，空闲或通话中拔卡。
2. 查询应出现 reg=0，D3 熄灭，旧通话结束。
3. 插回 SIM，等待识卡、驻网和默认 15 秒重试周期，再确认 sim=0、reg=1、D3 常亮。
4. 查询账号/对端与拔卡前一致，分别测试呼出和呼入。
5. 再验证 AT+QVOIP=0 手动停止后，拔插卡不会自动重启 SIP；需要手动启动。

　　RETRY_MS=0 时仍会清除失联状态，但不自动重新注册。AUTOSTART=0 只关闭开机自动启动，
手动启动之后仍可按重试配置恢复。拔卡后 unreg=-2 可能仅是无网络导致注销超时。

### AT 命令与状态查询

适用于 EG800ZCN_LA VoIP Demo。控制端口为
**Quectel USB AT Port**。命令使用英文半角引号和逗号，以 CRLF 结束。
下文“发送”和“返回”分开列出；`+QVOIP...:` 是返回文本，不能当成命令发送。
示例中的 `192.0.2.10` 是文档示例地址，必须换成可访问的 SIP 服务器。

#### 先理解三种返回

| 返回 | 含义 | 下一步 |
| --- | --- | --- |
| `+QVOIPACCEPT: 2`，随后 `OK` | 拨号请求已受理，2 是操作类型 | 等待执行结果 |
| `+QVOIPRESULT: 2,0` | 拨号请求执行成功/已提交 SIP 栈 | 等待呼叫状态；不等于被叫已接听 |
| `+QVOIPSTATE: 1,3` | SIP 已注册，SIP 呼叫状态为接通 | 与被叫状态和实际双向语音共同确认 |
| `ERROR` | 命令格式/参数有误，或控制任务正忙，未受理 | 查询状态，修正后重发 |

同一时刻只接受一个控制请求，**每条控制命令等待对应的 RESULT 后再发下一条**。
自动注册、按键和自动接听也会产生 RESULT/STATE，但没有命令 ACCEPT。
STATE 与 RESULT 的先后顺序可能不同；无状态变化时不重复发 STATE。
这些返回没有累加编号。操作 0/1/2/3/4/10 是固定含义，不是流水号。

#### 1. 检查 AT 通道

发送：
```text
AT
```
正常返回：
```text
OK
```
若无回复，先核对 USB AT 端口和 CRLF；`AT?` 不是此检查命令。

#### 2. 查询支持的控制操作

发送：
```text
AT+QVOIP=?
```
返回：
```text
+QVOIP: 0=stop,1=register,2=dial[,"peer"],3=answer,4=hangup
OK
```
只支持 0～4；历史 PCM 测试操作 6/7/8/9 不在发布 Demo 中。
`AT+QVOIP=10` 也无效，10 仅用于配置命令的异步结果标识。

#### 3. 查询账号配置格式

发送：
```text
AT+QVOIPCFG=?
```
返回：
```text
+QVOIPCFG: "server_ipv4",port,"account","password","default_peer"
OK
```

| 参数 | 内容与限制 |
| --- | --- |
| `server_ipv4` | SIP 服务的 IPv4 字符串，例如 `192.0.2.10`；此入口不支持域名/IPv6 |
| `port` | 实际 SIP UDP 端口，1～65535；不能把 RTP 端口填在这里 |
| `account` | 本机 SIP 账号，1～31 字节；字母、数字、`_`、`-`、`.`、`+` |
| `password` | 该账号的非空密码，最多 31 字节；不是对端账号密码 |
| `default_peer` | 被叫账号，1～31 字节；合法字符同 account，同时更新 S2 的目标 |

#### 4. 停止 SIP（包括换账号前的停止）

发送：
```text
AT+QVOIP=0
```
立即返回：
```text
+QVOIPACCEPT: 0
OK
```
执行完成：
```text
+QVOIPRESULT: 0,0
```
状态变化时还会收到 `+QVOIPSTATE: 0,0` 或清理过程中的结束状态。
此命令结束当前通话、停止 SIP 并取消自动重连；不会关闭共享 PDP 数据连接。
停止会注销本设备旧 Contact，最多等待服务器应答 5 秒。`0,0` 只证明本地停止完成。
查询中的 `unreg=200` 表示服务器确认注销；`unreg=-2` 表示超时，不等于本地停止失败。
之后需要 `AT+QVOIP=1` 或 S2 再启动。

#### 5. 配置或切换账号

先停止并等到 `+QVOIPRESULT: 0,0`，再发送（替换地址、端口和密码）：
```text
AT+QVOIPCFG="192.0.2.10",9999,"1001","YOUR_SIP_PASSWORD","1002"
```
立即返回：
```text
+QVOIPACCEPT: 10
OK
```
成功返回：
```text
+QVOIPRESULT: 10,0
```
若 SIP 尚未停止，可能先受理再返回 `+QVOIPRESULT: 10,-1`，原账号保持不变。
格式错误、字符串过长、非法账号或忙时可以直接返回 `ERROR`；没有 ACCEPT 就不要等 RESULT。
配置成功不会自动执行注册，下一步发送 `AT+QVOIP=1`。

AT 配置只在本次开机有效，**不会写回头文件或保存到 Flash**。
最后一个参数同时更新 AT 默认拨号和 S2 目标；一次指定号码的拨号不会更改它们。
重新上电恢复 `voip_demo_config.h` 中的 ACCOUNT、PEER 和 KEY_PEER。

#### 6. 查询当前生效账号及两个默认目标

发送：
```text
AT+QVOIPCFG?
```
以上配置成功后的返回：
```text
+QVOIPCFG: 1,"1002","1001","1002"
OK
```

| 位置 | 示例 | 含义 |
| --- | --- | --- |
| 第 1 项 | 1 | 配置有效；0 表示未成功配置，不表示网络注册状态 |
| 第 2 项 | 1002 | AT 省略号码时的默认被叫 |
| 第 3 项 | 1001 | 当前本机 SIP 账号 |
| 第 4 项 | 1002 | 当前按 S2 呼叫的对端 |

查询不返回密码。命令回显可能由 AT 终端/解析器打开；含密码的配置命令回显不是本查询结果。

#### 7. 联网并注册 SIP

发送：
```text
AT+QVOIP=1
```
立即返回：
```text
+QVOIPACCEPT: 1
OK
```
注册成功：
```text
+QVOIPRESULT: 1,0
+QVOIPSTATE: 1,0
```
STATE 可能先于 RESULT。再发 `AT+QVOIP?`，以 `reg=1` 为已注册依据。
Demo 自行检查 SIM0、等待蜂窝附着、激活/复用 PDP1 并注册 SIP，不需要额外发送驻网 AT。
默认 AUTO_DIAL=0，只注册不拨号；AUTO_DIAL=1 则成功后自动呼叫默认对端一次。
已启动时重复发送不是刷新注册命令，可返回 `1,-1`。

常见失败：`1,-13` SIM 未就绪；`1,-10` 驻网超时；`1,-14` 注册期间失去数据连接。
`RETRY_MS>0` 时注册失败后会按配置重试；手动停止可终止重试。
保留占位配置时，启动默认配置通常会先报告 `10,-40`，应先填写真实配置。

#### 8. 拨号（主叫）

指定号码：
```text
AT+QVOIP=2,"1002"
```
使用默认号码：
```text
AT+QVOIP=2
```
两者立即返回：
```text
+QVOIPACCEPT: 2
OK
```
请求成功后：
```text
+QVOIPRESULT: 2,0
```
常见状态过程为 `1,2`（拨号）→ `1,7`（振铃）→ `1,3`（SIP 接通）；
服务器可以省略中间状态。需先注册成功且无其它活动呼叫；失败看 `2,<非零值>` 和查询诊断。
`2,"1002"` 只指定本次号码，不修改默认被叫或 S2 号码。

**服务器提前应答：** 现场 SIP 服务可能在被叫接听前返回 200 并播放“嘟嘟”。
此时主叫也会是 `state=3,sip=200`，不能据此判定被叫响铃或接听。
需同时查看被叫 `state=4/5`、按 S3/AT 接听及实际双向声音。

#### 9. 接听（被叫）

被叫收到来电时，常见通知：
```text
+QVOIPSTATE: 1,4
```
本地铃声响、D4 闪烁；发送：
```text
AT+QVOIP=3
```
立即返回和完成结果：
```text
+QVOIPACCEPT: 3
OK
+QVOIPRESULT: 3,0
```
接通时出现 `+QVOIPSTATE: 1,3`。也可按 S3 接听；AUTO_ANSWER=1 时自动接听。
没有来电时接听通常返回 `3,-1`，不能靠此命令发起新通话。

#### 10. 取消、拒接或挂断

发送：
```text
AT+QVOIP=4
```
立即返回和成功结果：
```text
+QVOIPACCEPT: 4
OK
+QVOIPRESULT: 4,0
```
同一条命令随当前状态执行：主叫等待时取消、被叫来电时拒接、接通后挂断。
最终常见状态为 `+QVOIPSTATE: 1,6`；SIP 仍在线，可以再拨号或收来电。
`4,0` 表示本地操作成功/请求已交 SIP 栈，不保证远端已经显示结束；等待 state=6/0。
无活动通话时重复挂断返回成功，不创建新通话。S3 在来电时执行的是**接听**，
主叫等待或通话时执行取消/挂断；需拒接来电时使用 AT 操作 4。

#### 11. 查询当前状态

发送：
```text
AT+QVOIP?
```
已接通主叫的示例（数值随现场变化）：
```text
+QVOIP: init=1,start=1,reg=1,state=3,busy=0,op=2,result=0,board=0,codec=24,io=0
+QVOIPCALL: role=1,stage=8,fail=0,rc=0,event=7,sip=200,rec=1,play=1
+QVOIPNET: sim=0,simrc=0,hotrc=0,rc=0,regcode=200,unreg=0
+QVOIPSIM: det=1,detrc=0,enable=1,level=1,cfgrc=0,slot=0,slotrc=0,cfun=1,cfunrc=0,recover=0,recrc=0
+QVOIPAUDIO: guard=1,mode=4,level=9,kind=0,score=0,call=9,ringback=1,ring=1
OK
```
只读查询，不会拨号或启动服务。完整字段、状态和诊断码见下面附录。

#### 换账号的完整顺序

1. `AT+QVOIP=0` → 等 `+QVOIPRESULT: 0,0`。
2. `AT+QVOIPCFG="192.0.2.10",9999,"1009","PASSWORD_FOR_1009","1002"`
   → 等 `+QVOIPRESULT: 10,0`。
3. `AT+QVOIPCFG?` → 应为 `+QVOIPCFG: 1,"1002","1009","1002"`。
4. `AT+QVOIP=1` → 等 `+QVOIPRESULT: 1,0`，查询确认 `reg=1`。
5. 另一端呼叫 1009，应进入本板来电；再测旧号码。旧账号的初始 INVITE 会被本板拒绝，
   但服务器主动将旧号码转接/别名到新账号的行为不由此 Demo 管理。

#### 常见返回速查

| 位置/值 | 解释与处理 |
| --- | --- |
| `ERROR` | 参数或语法错误、忙/队列未就绪；先查 busy 和当前状态 |
| RESULT 的 0 | 本次操作成功；注册、拨号、接听、挂断的具体完成含义不同，见各节 |
| `-1` | 参数/状态不允许等通用错误；例如服务未停就配置、未注册就拨号或无来电却接听 |
| `-4` | 来电铃声停止/资源释放未成功，未继续打开通话音频 |
| `-10` / `-11` / `-12` | 蜂窝附着超时 / 数据连接对象无效 / 无有效 IPv4 |
| `-13` / `-14` | SIM 尚未就绪或正在恢复 / 当前 SIP 数据连接丢失 |
| `-20` / `-21` | 板级 codec 识别失败 / 音频时序配置失败 |
| `-30` / `-31` / `-32` / `-33` | IO 任务 / 信号量 / 控制队列 / 控制任务创建失败 |
| `-40` | 编译配置缺失或仍使用服务器/密码占位值 |
| `hotrc=-41` | 热插拔配置读回不一致 |
| `recrc=-42` | SIM 软件恢复回调超时或参数异常 |
| `unreg=-1` / `unreg=-2` | 注销请求构建/发送失败 / 注销等待超时，不是通用 RESULT 码 |

底层接口还可能返回其自身错误码，不能将所有非零值都解释成“密码错误”。
SIP `401/407` 可是正常鉴权挑战；最终是否注册成功看 `reg=1`，并结合 `regcode`。
`codec=24` 是十进制 I²C 地址（0x18），不是错误码；`state=6` 是结束，不是已接通。



#### 返回字段详解

##### +QVOIP：整体状态

| 字段 | 含义 |
| --- | --- |
| init | 1：组件已初始化；不是网络已通 |
| start | 1：SIP 服务已启动；不等于注册成功或通话中 |
| reg | 1：当前 SIP 已注册；0：未注册/失联 |
| state | 当前呼叫状态，见下表；6 结束，3 才是 SIP 接通 |
| busy | 1：控制任务正在处理请求；0：可以提交下一请求 |
| op、result | 最近完成的操作类型、结果；可能属于按键或自动操作，不是累计编号 |
| board | 板级初始化错误，0 表示未记录错误 |
| codec | 检测到的 ES8311 I²C 地址，十进制 24=0x18；0 表示尚未检测/无地址 |
| io | GPIO 运行错误，0 表示未记录错误 |

| state | 含义 |
| --- | --- |
| 0 | 空闲 |
| 1 | 保持（底层状态定义；本 Demo 没有独立保持/恢复命令） |
| 2 | 主叫拨号 |
| 3 | SIP 通话已接通 |
| 4 | 收到来电 |
| 5 | 等待来电处理 |
| 6 | 通话结束 |
| 7 | 主叫振铃/等待接听 |

##### +QVOIPCALL：最近一次呼叫诊断

| 字段 | 含义 |
| --- | --- |
| role | 0 无、1 主叫、2 被叫 |
| stage | 最近呼叫处理阶段，见下表 |
| fail、rc | 首个失败阶段及其原始错误码；0 表示没有记录失败 |
| event | 最近关联的 eXosip 事件编号，供协议定位；不是当前呼叫状态或次数 |
| sip | 最近相关 SIP 应答码，如 180、200、401、486；0 尚无应答 |
| rec、play | 0 尚未打开、1 流打开成功、负值为打开失败；不证明麦克风有信号或喇叭实际发声 |

呼叫结束后保留诊断，新呼叫开始时刷新；必须同时查看整体 state。

| stage/fail | 阶段 |
| --- | --- |
| 0 / 1 / 2 | 无 / 主叫初始 INVITE / 接收来电 INVITE |
| 3 / 4 / 5 | 解析远端 SDP / 创建 ACK / 发送 ACK |
| 6 / 7 / 8 | 投递媒体请求 / 启动媒体 / 媒体任务就绪 |
| 9 / 10 / 11 | 被叫应答 / 呼叫超时 / 终止呼叫失败 |
| 12 / 13 / 14 | 等待可取消的 SIP 应答 / 已提交 CANCEL / 已提交 BYE 或拒接 |

##### +QVOIPNET：SIM 与 SIP 网络结果

| 字段 | 含义 |
| --- | --- |
| sim | SDK 当前 SIM 状态：0 READY、16 NOT_INSERTED、17 BUSY、18 BLOCKED、19 UNKNOWN；其它值含 PIN/PUK 状态 |
| simrc | SIM 状态读取结果，0 成功；失败时不能仅依据 sim 判断物理插卡 |
| hotrc | 热插拔设置/读回结果，0 成功；-41 为不一致；成功不代表检测极性正确或 SIM 已识别 |
| rc | 最近一次来电处理结果，0 成功；正值可为 SIP 拒接码，负值为本地错误 |
| regcode | 最近 SIP 注册应答码，通常 200 成功；判断当前在线仍以 reg 为准 |
| unreg | 最近注销结果：0 未尝试；2xx 服务器确认；-1 本地构建/发送失败；-2 超时；其它正值为 SIP 应答码 |

##### +QVOIPSIM：检测引脚与恢复结果

| 字段 | 含义 |
| --- | --- |
| det、detrc | 模组 Pin79 当前电平 0/1、读取结果；读取失败时 det=-1 |
| enable、level、cfgrc | SDK 实际热插拔开关、插卡有效电平、配置读取结果 |
| slot、slotrc | 当前 SIM 卡槽、查询结果；此 EVB 使用槽 0 |
| cfun、cfunrc | 当前功能状态、查询结果；正常完整功能为 cfun=1 |
| recover | 0 无恢复、1 等待卡稳定、2 等 CFUN0、3 恢复 CFUN1、4 此轮完成、5 出错 |
| recrc | 恢复错误，0 无错误；-42 回调超时/参数异常；其它为 SDK 原始错误 |

当前已测 QuecDuino EVB：拔卡 det=0，插卡 det=1，默认 level=1。
其它板型须实测，不能凭排针丝印或平台默认值猜测。
recover 是有限状态，不是累加次数；recover=4 也不等于注册成功，仍需检查 sim=0、reg=1。

##### +QVOIPAUDIO：当前音量和提示音识别

| 字段 | 含义 |
| --- | --- |
| guard | 0 关闭、1 开启服务器提前 SIP 200 的主叫提示音识别 |
| mode | 0 普通/无流；1 SIP 接通前回铃；2 主叫识别中；3 已识别提示音；4 本流已锁定普通语音 |
| level | 最近媒体帧采用的逻辑音量；提示音 PCM 处理时底层保持原幅度 6，不等于底层直接设置该值 |
| kind | 0 无匹配，1=425 Hz，2=450 Hz，3=440+480 Hz 模板；不是精确频率测量 |
| score | 最近模板能量百分比 0～100，不是接听可信度，也不是次数 |
| call、ringback、ring | 编译配置的正常通话、主叫提示音、本地来电铃声档位 |

默认提示音被识别时通常 mode=3、level=1；正常人声 level=9。
本流检测到至少 300 ms 有效非提示音后锁定普通语音；新流重新开始。
识别采用音频特征，有误判和漏判可能，不代替 SIP 或真实被叫接听状态。

#### 通知及兼容性

- `+QVOIPRESULT: 操作,结果`：执行结果，只有两项；旧版的“序号,操作,结果”格式已移除。
- `+QVOIPSTATE: 注册状态,呼叫状态`：仅在状态改变时通知，只有两项。
- `+QVOIPCALL/NET/SIM/AUDIO`：由 `AT+QVOIP?` 主动查询返回，不作为新增周期 URC 推送。
- 不打印 seq、try、cid、tx、rx、capture、errors、keys 等累计字段。
  SIP/RTP 协议所需内部计数仍保留，但不输出成这些 AT/URC。
- `AT+QVOIP` 不带 `?`、`=?` 或 `=操作` 不是有效控制命令。
  标准 `CPIN/QSIMDET/CFUN` 是否开放由 SDK 决定，本 Demo 的识卡和重连不依赖发送这些 AT。

### 验证与问题定位

| 现象 | 检查方法 |
| --- | --- |
| result=-40 | 检查本次构建的 voip_demo_config.h 是否仍是占位值；或先停止后通过 AT 配置 |
| sim=16、reg=0 | 核对实际插卡，Pin79 插卡是否为 1、level 是否为 1，查看 hotrc、recover、recrc |
| sim=0、注册失败 | 检查天线/SIM 数据业务、蜂窝附着、PDP/IPv4，再核对 SIP 地址、UDP 端口、账号密码和 regcode |
| 主叫显示已接通但被叫无反应 | 核对双方本机账号和 S2/AT 目标；服务器可能提前 SIP 200，需查被叫状态及服务端路由/绑定 |
| 改号后 S2 仍拨错 | 用 AT+QVOIPCFG? 查看第四项，确认配置 RESULT=10,0；重启会恢复 KEY_PEER |
| 来电时按 S3 不能拒接 | S3 在来电状态负责接听；拒接用 AT+QVOIP=4 |
| 通话无声/单向声音 | 两端查询 state、role、stage、fail、rc、sip、rec/play；核对 MIC、喇叭、PA、codec 及服务器 SDP/RTP 路径 |
| 需要提高对端听到的声音 | 调本板 MIC_GAIN_PERCENT；调本板 CALL_VOLUME 只改变本板听到的声音 |
| 提示音仍大或改变人声音量 | 查询 AUDIO 的 guard/mode/level/call/ringback/ring，核对烧录的是新包；未知提示音频率需进一步分析 |
| AT 返回 ERROR | 检查 USB AT 口、英文引号、逗号、CRLF、操作范围和 busy；不要把 +QVOIP 返回文本当命令发送 |
| CLI 找不到 Demo | 执行 unirtos-cli ls-demos -f 刷新列表；仍没有时联系维护者确认索引 |

　　本版不再输出 tx/rx/capture 等累计字段，不能沿用旧教程用它们的增长判定。
rec/play=1 只说明流打开，定位网络媒体仍可能需要服务端日志或抓包。
保留两端本次从拨号到结束的完整 AT 返回，关注状态转移和错误，不需要重新打开累计 URC。

| 验收项目 | 通过标准 |
| --- | --- |
| 纯 CLI 构建 | 新目录 env-setup、无参数 build 成功，SDK 跟踪文件无改动 |
| 上电注册 | 配置有效时自动注册；关闭 AUTOSTART 时按设定等待手动启动 |
| 双板/PC 双向通话 | 主被叫均覆盖，两端实际能听清持续讲话 |
| 取消后重拨 | 未接听取消成功，紧接着重拨仍能响铃并接通 |
| 临时换账号 | 等停止→配置→注册，查询本机和 S2 目标正确，旧账号不能直接触发本板响铃 |
| 音量独立 | 来电铃声及识别到的回铃为设定低档，正常通话档位保持独立 |
| SIM 拔插恢复 | 清除假在线并重新注册，账号保留，能重新呼入/呼出；手动停止后不自行启动 |
| AT 输出 | 无流水序号和累计收发/按键计数，固定操作码不会递增 |

　　此前联调已验证过双板/PC 通话、PCM 音频和相关按键功能。当前发布整理通过本地
CLI 获取、编译、链接及打包检查；这些检查不代替本版固件的上述实板回归，验收结果按现场补录。

## 软件设计详解

### 代码讲解

| 仓库/文件 | 职责 |
| --- | --- |
| Demo / voip_demo_config.h | 随包提供的客户配置：账号、对端、自动行为、音量、SIM |
| Demo / voip_demo_entry.c | 使用 SDK UNIRTOS_APP_EXPORT 注册启动，并注册两条自定义 AT 命令 |
| Demo / voip_demo.c | 唯一串行控制任务、联网、账号切换、AT、按键请求和断网重连 |
| Demo / voip_sim.c、voip_sim.h | SIM 检测和有界 CFUN 恢复状态机 |
| Demo / boards/eg800z_quecduino/voip_board.c | 真实模组 Pin、按键、D2/D3/D4、PA、ES8311 初始化 |
| Demo / voip_ringtone.c、assets/telefone_ring_ulaw.inc | 来电铃声状态与素材；编码素材供本地铃声使用，通话网络仍是 PCMA |
| Demo / env_config.json、menuconfig/.config、CMakeLists.txt | 官方 SDK/组件版本、所需功能选择、外部应用构建 |
| 组件 / include/qcm_voip.h、qcm_voip.c | 公共通话接口和单路业务控制 |
| 组件 / port/qcm_voip_pcm.c | 通话 PCM 采集/播放、PCMA 和重采样接入、媒体音量 |
| 组件 / port/qcm_voip_tone_guard.c | 主叫提前应答提示音识别 |
| 组件 / vendor/app、vendor/osip、vendor/ortp | 导入的呼叫管理、SIP/RTP 协议及编解码实现，保留原版权声明 |

　　AT 解析和按键只投递请求，阻塞的注册/停止/音频资源操作由同一个控制任务执行。
失联事件回调只记录状态，实际清理由控制任务处理，避免回调阻塞。
组件不直接决定 SIM、按键、LED 或账号策略；这些均由 Demo 管理，SDK 维持官方版本。

### 应用演示

　　演示顺序：两板注册成功 → A 按 S2 → B 响铃 → B 按 S3 接听 → 双向讲话 → 任一端 S3 挂断。
再演示主叫取消后重拨、MicroSIP 呼入、AT 换账号，以及拔插 SIM 后自动重新注册。
实际演示视频/现场结果可在此章节补充；本文不以状态打印或编译成功替代声音演示。
