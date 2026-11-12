/** @file voip_ringtone.h
 * @brief 来电响铃接口；所有接口仅由 VoIP 控制任务串行调用。
 */
#ifndef VOIP_RINGTONE_H
/** 响铃头文件的重复包含保护。 */
#define VOIP_RINGTONE_H
#include "qcm_voip.h"

/** @brief 按当前来电状态维护原始 PCM 响铃。
 * @param[in] const qcm_voip_status_t *status 当前状态快照。
 * @param[in] bool allowed 板级可用且 SIP 已启动。
 * @return 无；写入失败或超时会停止本次响铃，不改变 SIP 通话状态。
 */
void voip_ringtone_poll(const qcm_voip_status_t *status, bool allowed);
/** @brief 停止响铃并释放播放流；必须成功后才能接听或打开其他音频。
 * @param[in] bool inhibit true 禁止本次来电在异步接听期间重新响铃。
 * @return 0 已释放；-4 关闭失败，保留句柄供同一任务重试。
 */
int voip_ringtone_stop(bool inhibit);
#endif /* VOIP_RINGTONE_H */
