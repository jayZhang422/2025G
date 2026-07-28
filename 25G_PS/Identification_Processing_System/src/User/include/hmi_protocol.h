#ifndef USER_INCLUDE_HMI_PROTOCOL_H_
#define USER_INCLUDE_HMI_PROTOCOL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_PROTOCOL_FRAME_SIZE 9U
#define HMI_PROTOCOL_HEADER0 0x55U
#define HMI_PROTOCOL_HEADER1 0xAAU
#define HMI_PROTOCOL_TAIL 0xFFU

typedef enum {
    HMI_PROTOCOL_CMD_ENTER = 0x01U,
    HMI_PROTOCOL_CMD_START = 0x02U,
    HMI_PROTOCOL_CMD_STOP = 0x03U,
    HMI_PROTOCOL_CMD_BACK = 0x04U,
    HMI_PROTOCOL_CMD_RESET = 0x05U,
    HMI_PROTOCOL_CMD_FREQUENCY_HZ = 0x10U,
    HMI_PROTOCOL_CMD_VPP_MV = 0x11U,
    HMI_PROTOCOL_CMD_PHASE = 0x12U
} hmi_protocol_command_t;

typedef enum {
    HMI_PROTOCOL_MODE_SIGNAL_SOURCE = 0x01U,
    HMI_PROTOCOL_MODE_AMPLITUDE_CONTROL = 0x02U,
    HMI_PROTOCOL_MODE_FILTER_REPLAY = 0x03U,
    HMI_PROTOCOL_MODE_LEARNING = 0x04U
} hmi_protocol_mode_t;

typedef struct {
    uint8_t command;
    uint8_t mode;
    uint32_t data;
} hmi_protocol_frame_t;

typedef struct {
    uint8_t bytes[HMI_PROTOCOL_FRAME_SIZE];
    uint8_t count;
} hmi_protocol_parser_t;

void hmi_protocol_parser_init(hmi_protocol_parser_t *parser);
int hmi_protocol_parser_feed(hmi_protocol_parser_t *parser,
                             uint8_t byte,
                             hmi_protocol_frame_t *frame);
int hmi_protocol_mode_is_valid(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* USER_INCLUDE_HMI_PROTOCOL_H_ */
