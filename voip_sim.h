#ifndef VOIP_SIM_H
#define VOIP_SIM_H
#include <stdbool.h>

/** @brief 控制任务轮询无卡开机后的识卡恢复；allow 为本 Demo 当前允许自动注册。 */
void voip_sim_poll(bool allow);
/** @brief 是否正在恢复 CFUN；恢复期间暂缓 SIP 注册，防止使用被关闭的数据连接。 */
bool voip_sim_recovering(void);
/** @brief 只读查询实际检测电平、配置和恢复状态；不输出累计值，不修改模组状态。 */
int voip_sim_diag_text(char *text, unsigned size);
#endif
