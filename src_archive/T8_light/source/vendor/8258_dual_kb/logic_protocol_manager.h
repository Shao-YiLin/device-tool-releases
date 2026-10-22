#ifndef KEYBOARD_PROTOCOL_MANAGER_H
#define KEYBOARD_PROTOCOL_MANAGER_H

#include "common/types.h"
#include "../../third/protocol/protocol_manager.h"
#include "../../third/protocol/protocol_define.h"
#include "stack/ble/ble_common.h"

#define AUTHENTICATION_REG DEEP_ANA_REG0
bool authenticate_success_flag;

struct logic_protocol_manager
{
    void (*start_record_respond)(u32 error_code);
    void (*stop_record_respond)(u32 error_code);
    ble_sts_t (*send_notify_data)(struct command_data *command);
    u8 (*parse)(u8 *data, u32 len);
    void (*clear_cache)(void);
};

struct logic_protocol_manager* get_logic_protocol_manager();


#endif
