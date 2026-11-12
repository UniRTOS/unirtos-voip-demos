# unirtos-voip-demos

中文 | [English](README.md)

EG800Z QuecDuino EVB 双向语音通话示例，通过 `unirtos-cli` 获取依赖并使用官方 SDK 编译。

## 功能说明

- 开发板之间，或开发板与 PC MicroSIP 之间的 SIP/UDP、RTP/PCMA 通话。
- 支持自动注册、拨号、接听、取消、挂断、来电铃声和网络恢复。
- 支持 S2/S3 按键、状态指示灯、USB AT 控制及独立音量配置。
- 通话协议使用独立的 [unirtos-voip](https://github.com/UniRTOS/unirtos-voip) 组件。

## 硬件要求

- EG800ZCN_LA / EG800Z QuecDuino EVB、板载 MIC 和外接喇叭。
- 可用的数据 SIM、蜂窝天线、USB 数据线及稳定供电。
- 两端可访问的 SIP 服务器及两个不同账号；另一端使用开发板或 MicroSIP。
- PC 安装 QFlash 和串口助手，控制端口使用 **Quectel USB AT Port**。

## 快速开始

### 1. 准备工具

按官方[环境搭建](https://www.quectel.com.cn/unirtos/docs?docs_page=快速上手/环境搭建/环境搭建.html)
安装 Python、Git 和 UniRTOS 工具链，再安装 CLI：

```bash
pip install unirtos-cli
```

### 2. 拉取 Demo

```bash
unirtos-cli ls-demos -f
unirtos-cli new -r unirtos-voip-demos
```

进入 CLI 输出的工程目录。目录名称以创建成功时的提示为准。

### 3. 配置账号

打开工程根目录的 `voip_demo_config.h`，修改已有宏并保存：

```c
#define VOIP_DEMO_SERVER    "YOUR_SIP_SERVER"   // SIP 服务器 IPv4 地址
#define VOIP_DEMO_PORT      9999                // 实际 SIP UDP 端口
#define VOIP_DEMO_ACCOUNT   "1001"              // 本机账号
#define VOIP_DEMO_PASSWORD  "YOUR_SIP_PASSWORD" // 本机密码
#define VOIP_DEMO_PEER      "1002"              // AT 默认对端
#define VOIP_DEMO_KEY_PEER  "1002"              // S2 默认对端
```

**服务器和密码必须替换为实际值；只改账号、保留占位值时，不会自动注册。**
第二块板使用账号 1002 及其密码，对端改为 1001。配置文件随 Demo 提供，无需额外下载。

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `VOIP_DEMO_AUTOSTART` | 1 | 1：上电自动注册；0：通过 S2 或 AT 手动启动 |
| `VOIP_DEMO_AUTO_DIAL` | 0 | 1：注册后自动拨号；0：手动拨号 |
| `VOIP_DEMO_AUTO_ANSWER` | 0 | 1：来电自动接听；0：手动接听 |

### 4. 编译与烧录

在当前工程目录打开终端，执行：

```bash
unirtos-cli env-setup
unirtos-cli build
```

依赖由 `env_config.json` 管理，默认模组为 `EG800ZCN_LA`，不需要修改 SDK。
使用 QFlash 烧录当前工程下的：

```text
qos_build/release/<工程名称>/at_command.hbinpkg
```

修改账号或音量后再次执行 `unirtos-cli build`。双板可用同一份工程依次配置、编译，
每次先保存完整固件目录，再编下一块板，不需要下载两次。

### 5. 通话测试

1. 插好 SIM、天线和喇叭后上电，等待两端 D3 常亮；`AT+QVOIP?` 应显示 `reg=1`。
2. 主叫按 S2；被叫响铃后按 S3 接听，检查双向声音。
3. 任一端按 S3 挂断；主叫等待接听时按 S3 可取消呼叫。
4. 单板测试时，MicroSIP 登录另一个账号，使用 UDP、PCMA 和不加密媒体，再与板子互拨。

USB AT 常用命令：

| 命令 | 用途 |
| --- | --- |
| `AT+QVOIP?` | 查询注册和通话状态 |
| `AT+QVOIPCFG?` | 查询当前账号及呼叫目标 |
| `AT+QVOIP=1` | 启动注册 |
| `AT+QVOIP=2,"1002"` | 呼叫 1002 |
| `AT+QVOIP=3` | 接听 |
| `AT+QVOIP=4` | 取消、拒接或挂断 |
| `AT+QVOIP=0` | 停止 SIP |

`OK` 表示命令已受理，最终执行结果看 `+QVOIPRESULT`。
完整参数、返回值和运行时换号步骤见 [AT 指令说明](docs/AT_COMMANDS.md)。

## 更多说明

- [完整使用指南](docs/USER_GUIDE.zh.md)：接线、双板/PC 测试和问题排查。
- [配置说明](docs/CONFIGURATION.md)：音量、按键、指示灯及 SIM 恢复。
- [适配说明](docs/PORTING.md)：组件、Demo 和 SDK 的职责划分。
- [技术社区](https://forumschinese.quectel.com/c/66-category/66)。

欢迎通过 Issue 或 Pull Request 提交问题和改进，并说明操作步骤及验证结果。
