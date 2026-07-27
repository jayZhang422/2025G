/******************************************************************************
 * signal_errors.h
 *
 * First-failure error codes returned by the public signal API.
 ******************************************************************************/

#ifndef USER_INCLUDE_SIGNAL_ERRORS_H_
#define USER_INCLUDE_SIGNAL_ERRORS_H_

/** 公共 signal_api_* 使用的首个失败原因；零统一表示成功。 */
typedef enum {
    SIGNAL_OK = 0,                   /**< 调用成功。 */
    SIGNAL_ERR_ARGUMENT,             /**< 空指针、非法数值或错误 API 状态。 */
    SIGNAL_ERR_PROFILE_INVALID,      /**< profile 缺少当前模式要求的参数。 */
    SIGNAL_ERR_DMA_INIT,             /**< AXI DMA S2MM 初始化失败。 */
    SIGNAL_ERR_FFT_INIT,             /**< CMSIS 实数 FFT 实例初始化失败。 */
    SIGNAL_ERR_BUTTON_INIT,          /**< PS GPIO 按键初始化失败。 */
    SIGNAL_ERR_FRAME_ALIGN,          /**< S2MM 未能对齐到完整 AXIS 帧。 */
    SIGNAL_ERR_DMA_CAPTURE,          /**< DMA 传输启动、等待或完成失败。 */
    SIGNAL_ERR_FRAME_INVALID,        /**< 接收帧长度或元数据不满足要求。 */
    SIGNAL_ERR_ANALYSIS,             /**< 信号算法未能产生有效结果。 */
    SIGNAL_ERR_IQ_UNAVAILABLE,       /**< PL IQ 检测器初始化时不可用。 */
    SIGNAL_ERR_IQ_MEASURE,           /**< IQ 配置、等待或结果读取失败。 */
    SIGNAL_ERR_DDS_COMMIT            /**< DDS shadow 配置原子提交失败。 */
} signal_error_t;

const char *signal_error_string(signal_error_t error);

#endif /* USER_INCLUDE_SIGNAL_ERRORS_H_ */
