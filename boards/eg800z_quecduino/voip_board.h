#ifndef VOIP_BOARD_H
#define VOIP_BOARD_H
#include "qcm_voip.h"
int voip_board_init(void);
/** @brief 配置 SIM1/模组 Pin79 热插拔检测；仅控制任务调用，0 成功，其余为错误码。 */
int voip_board_sim_init(void);
int voip_board_audio_init(void);
void voip_board_update(const qcm_voip_status_t *status, bool started);
/** @enum voip_board_key_e
 * @brief EVB 业务键的新按下事件，可以同时置位。
 */
typedef enum {
    VOIP_BOARD_KEY_CALL = 1U, /**< S2：模组 Pin50，固定目标呼叫键。 */
    VOIP_BOARD_KEY_CONTROL = 2U /**< S3：模组 Pin51，接听或挂断键。 */
} voip_board_key_e;
/** @brief 轮询两键，返回新按下事件位图；无事件返回 0。 */
int voip_board_key(void);
void voip_board_audio_output(bool enable);
/** @brief Return the verified ES8311 address; zero means not initialized. */
unsigned voip_board_codec_address(void);
/** @brief Return the most recent GPIO error, or zero. */
int voip_board_io_error(void);
#endif
