/**
 * @file voip_demo_entry.c
 * @brief 独立 VoIP 示例的启动入口和自定义 AT 命令注册。
 * @author UniRTOS VoIP contributors
 * @date 2026-11-10
 * @copyright 保留所导入源码的原有版权声明。
 * @par EDIT HISTORY FOR MODULE
 * <table>
 * <tr><th>Date <th>Version <th>Author <th>Description
 * <tr><td>2026-11-10 <td>1.0.0 <td>UniRTOS VoIP contributors <td>接入外部应用启动注册。
 * </table>
 */
#include "qosa_at_cmd.h"
#include "unirtos_app_init_registry.h"

/**
 * @brief 初始化已验证的板级资源和 VoIP 控制任务。
 * @note 实现在 voip_demo.c；仅由本示例启动入口调用一次。
 */
extern void voip_demo_start(void);

/**
 * @brief 将 SIP 配置命令交给既有串行控制任务。
 * @param[in] qosa_at_cmd_t *cmd AT 解析器提供的命令及来源通道。
 */
extern void voip_demo_config_cmd(qosa_at_cmd_t *cmd);

/**
 * @brief 将呼叫控制与状态查询命令交给既有处理入口。
 * @param[in] qosa_at_cmd_t *cmd AT 解析器提供的命令及来源通道。
 */
extern void voip_demo_control_cmd(qosa_at_cmd_t *cmd);

/** 静态描述符在 AT 解析器使用期间保持有效。 */
static const qosa_at_desc_t g_voip_demo_at_commands[] = {
    {"+QVOIPCFG", voip_demo_config_cmd, 0},
    {"+QVOIP", voip_demo_control_cmd, 0},
};

/**
 * @brief 在标准 AT 应用完成初始化后注册命令并启动 VoIP 示例。
 * @note SDK 注册接口返回 void；单个应用统一提交自己的完整命令表。
 */
static void voip_demo_init(void)
{
    qosa_at_parser_add_cust_at(g_voip_demo_at_commands,
        (qosa_uint32_t)(sizeof(g_voip_demo_at_commands) / sizeof(g_voip_demo_at_commands[0])));
    voip_demo_start();
}

// 标准 AT 应用使用顺序 100；本示例沿用 SDK 的外部应用注册机制。
UNIRTOS_APP_EXPORT(330, "voip_demo", voip_demo_init);
