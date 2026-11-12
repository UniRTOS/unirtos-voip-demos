# unirtos-voip-demos

[中文](README.zh.md) | English

Bidirectional voice calls on the EG800Z QuecDuino EVB, using `unirtos-cli` and the official SDK.

## Features

- SIP/UDP and RTP/PCMA calls between two boards, or between a board and PC MicroSIP.
- Automatic registration, dialing, answering, cancellation, hangup, ringtone and network recovery.
- S2/S3 buttons, status LEDs, USB AT control and separate volume settings.
- Protocols provided by the independent [unirtos-voip](https://github.com/UniRTOS/unirtos-voip) component.

## Hardware Requirements

- EG800ZCN_LA / EG800Z QuecDuino EVB, onboard microphone and external speaker.
- A working data SIM, cellular antenna, USB cable and stable power.
- A reachable SIP server and two different accounts; use another board or MicroSIP as the peer.
- QFlash and a serial terminal on the PC. Send commands through **Quectel USB AT Port**.

## Quick Start

### 1. Install Tools

Follow the official [environment setup](https://www.quectel.com.cn/unirtos/docs?docs_page=快速上手/环境搭建/环境搭建.html)
to install Python, Git and the UniRTOS toolchain, then install the CLI:

```bash
pip install unirtos-cli
```

### 2. Get the Demo

```bash
unirtos-cli ls-demos -f
unirtos-cli new -r unirtos-voip-demos
```

Enter the project directory printed by the CLI after successful creation.

### 3. Configure Accounts

Edit the existing definitions in `voip_demo_config.h` at the project root and save:

```c
#define VOIP_DEMO_SERVER    "YOUR_SIP_SERVER"   // SIP server IPv4 address
#define VOIP_DEMO_PORT      9999                // Actual SIP UDP port
#define VOIP_DEMO_ACCOUNT   "1001"              // Local account
#define VOIP_DEMO_PASSWORD  "YOUR_SIP_PASSWORD" // Local account password
#define VOIP_DEMO_PEER      "1002"              // Default AT peer
#define VOIP_DEMO_KEY_PEER  "1002"              // Default S2 peer
```

**Replace both server and password placeholders. Changing only the account does not enable registration.**
For the second board, use account 1002 with its password and set both peers to 1001.
The configuration header is included; no additional download is needed.

| Macro | Default | Meaning |
| --- | --- | --- |
| `VOIP_DEMO_AUTOSTART` | 1 | 1: register on boot; 0: start with S2 or AT |
| `VOIP_DEMO_AUTO_DIAL` | 0 | 1: dial after registration; 0: dial manually |
| `VOIP_DEMO_AUTO_ANSWER` | 0 | 1: answer automatically; 0: answer manually |

### 4. Build and Flash

Open a terminal in the project directory and run:

```bash
unirtos-cli env-setup
unirtos-cli build
```

Dependencies are declared in `env_config.json`; the default module is `EG800ZCN_LA`.
No SDK changes are required. Use QFlash to flash:

```text
qos_build/release/<project-name>/at_command.hbinpkg
```

Run `unirtos-cli build` again after changing an account or volume. Use the same
project for both boards, saving the complete firmware directory before the next build.

### 5. Test a Call

1. Connect the SIM, antenna and speaker, then power on. Wait for steady D3 on both peers and `reg=1` in `AT+QVOIP?`.
2. Press S2 on the caller. When the other board rings, press its S3 and check both audio directions.
3. Press S3 on either board to hang up. The caller can also press S3 before answer to cancel.
4. For a single-board test, sign in to MicroSIP with another account, select UDP, PCMA and unencrypted media, then call either way.

Common USB AT commands:

| Command | Purpose |
| --- | --- |
| `AT+QVOIP?` | Query registration and call status |
| `AT+QVOIPCFG?` | Query the active account and peers |
| `AT+QVOIP=1` | Start registration |
| `AT+QVOIP=2,"1002"` | Call 1002 |
| `AT+QVOIP=3` | Answer |
| `AT+QVOIP=4` | Cancel, reject or hang up |
| `AT+QVOIP=0` | Stop SIP |

`OK` acknowledges the request; check `+QVOIPRESULT` for completion.
See the [AT reference](docs/AT_COMMANDS.md) for parameters, responses and account switching.

## Further Reading

- [User guide](docs/USER_GUIDE.zh.md): wiring, board/PC tests and troubleshooting.
- [Configuration](docs/CONFIGURATION.md): volume, buttons, LEDs and SIM recovery.
- [Porting notes](docs/PORTING.md): component, demo and SDK responsibilities.
- [Technical community](https://forumschinese.quectel.com/c/66-category/66).

Report issues or propose improvements through Issues and Pull Requests with steps and validation results.
