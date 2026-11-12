# 官方 SDK 兼容与适配边界

本 Demo 依赖 `unirtos-voip` 组件和 GitHub 官方 UniRTOS SDK，
目标为 EG800ZCN_LA / EG800Z QuecDuino EVB。SDK 源码、头文件、静态库和底包均保持官方版本。
正常用户按 README 执行 `unirtos-cli env-setup`、`unirtos-cli build`，不需要应用 SDK 补丁。

## 构建与应用接入

| 接入点 | 实现方式 |
| --- | --- |
| CLI 依赖 | `env_config.json` 声明 SDK 和组件依赖，并设置默认模组 EG800ZCN_LA |
| 外部应用 | 根目录 `CMakeLists.txt` 使用官方 SDK 外部应用构建方式；`menuconfig/.config` 选择所需功能 |
| 启动 | `voip_demo_entry.c` 使用 `UNIRTOS_APP_EXPORT`，不改 SDK 的应用启动表 |
| 自定义 AT | `qosa_at_parser_add_cust_at` 注册 QVOIP/QVOIPCFG，不改 SDK 的 AT 命令表 |
| 时间接口 | 组件的 tick 适配处理官方 SDK 时钟及回绕，不依赖旧工程私有符号 |
| 音频流 | 使用官方音频配置结构，不增加旧工程私有 route 字段 |
| 音频库选择 | 由 SDK Kconfig 选择 MP3/AMR 依赖；通话本身使用 PCM/PCMA，不定义本地私有 PCM_ONLY 选项 |

组件的 `include/` 提供公共接口；`port/` 管理 RTOS、PCM、重采样、SDP 和提示音音量适配。
`vendor/` 保留导入协议代码和原版权信息。少量仅包含适配头的兼容头仍被 vendor 引用，不能按文件长度删除。

## 音频与板级资源

`boards/eg800z_quecduino/voip_board.c` 负责实际模组引脚、ES8311、PA、按键和指示灯。
原有已联调的音频时序、寄存器初始化和铃声数据保持；硬件映射按原理图，不根据排针丝印猜测。

Demo 通过公开 GPIO API 控制 Pin16 上的 D2，不依赖平台 LED 服务。
IO 任务每秒读取一次缓存的 PS 驻网状态，20 ms 更新灯电平；最近 Demo RTP 活动可切换快闪。
它只反映蜂窝网络及本 Demo 活动，不表示 SIP 在线。未来 SDK 若启用同一引脚的 LED 服务，需明确唯一控制方。

来电铃声由控制任务异步写入音频流，并以 200 ms 无进展超时保护写入；不依赖旧官方音频归档
是否透传 callback ctx。停止失败保留句柄供后续清理，不能在未释放铃声时同时打开通话流。

铃声/主叫提示音在 PCM 上缩放，底层设为原幅度 6 档；发布默认 1 档为 1/32 幅度。
正常通话音量及麦克风增益单独设置。服务器提前 SIP 200 后的主叫等待音通过可关闭的
425/450 Hz、440+480 Hz 音频识别兼容；这不是实际被叫接听通知，有误判和漏判边界。
详细曲线和限制见 [配置说明](CONFIGURATION.md)。

## SIM 检测与运行中恢复

`voip_sim.c` 通过公开 SIM 热插拔接口启用检测并读回配置，Pin79 仍由 SDK SIM 检测器管理。
当前 EVB 现场确认拔卡为 0、插卡为 1，因此默认 `VOIP_DEMO_SIM_INSERT_LEVEL=1`。
其它硬件版本需重新核对，不直接沿用此极性。

已稳定检测到插卡但 SDK 仍报 NOT_INSERTED 时，在允许注册、SIP 未活动、槽 0、CFUN1
等条件满足后执行有界 CFUN0→1 恢复。一个静态请求跟踪回调；手动停止或迟到回调发生后
仍尝试恢复 CFUN1，并限制重试。已就绪/PIN 锁定的卡和活动通话不会被此流程重置。

运行中 SIM/PDP 丢失时，事件回调只记录失联，不阻塞回调；控制任务清理旧 SIP 和媒体，
避免 D3 维持假在线。轮询作为事件漏报的补充。重新识卡/附着后获取当前 IP 再注册，
保留本次开机的账号和 S2 目标，不自动重拨被中断的通话。用户手动停止优先于自动恢复。
`RETRY_MS=0` 关闭自动重试，但仍清除失联状态。SDK 无需任何修改。

## 账号切换与输出兼容

运行时换号按“停止 → 配置 → 注册”顺序执行，每步等待 RESULT。
停止时注销本设备旧 Contact，最多等待 5 秒；新来电的初始 Request-URI 需匹配当前账号。
成功配置同时更新 AT 默认被叫和 S2 目标；重启恢复编译配置，AT 不保存账号到 Flash。

AT 输出不包含 seq/try/cid/tx/rx/capture/keys 等累计字段。内部协议计数、请求串行化和
RTP 活动判断仍保留。RESULT/STATE 采用固定的两字段格式，旧上位机须按
[AT 指令参考](AT_COMMANDS.md) 更新解析。显式查询才返回 CALL/NET/SIM/AUDIO 诊断。

## 发布和验证范围

本仓库只包含通话 Demo、板级适配、来电铃声、构建配置及文档；没有独立 PCM 播放/录播
测试或 Debug UART 测试程序。`voip_demo_config.h` 是随包提供的唯一客户配置头，默认用
服务器和密码占位值；需填写实际账号后才能注册。

开发时通过 `unirtos-cli env-setup` 和 `unirtos-cli build` 验证依赖获取与编译。
修改通话或板级逻辑后，按[完整指南](USER_GUIDE.zh.md) 检查双向声音、取消后重拨、账号切换、
提示音音量和 SIM 重插恢复。
