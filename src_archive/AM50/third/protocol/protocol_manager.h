#ifndef PROTOCOL_MANAGER_INC
#define PROTOCOL_MANAGER_INC

#include "protocol_define.h"
#include "../../common/types.h"

struct common_header
{
    u8 ucMagic;
    u8 ucSequence;
    u8 ucCmd;
    u32 ulLength;
};

struct protocol_header
{
    struct common_header xHead;
    u8 pcPayload[128];
    u32 ulSign;
};

struct command_data
{
	u8 pcData[128];
	u8 ucLen;
};

struct protocol_manager
{
    // struct command_data *(*login_pack)(void);
    struct command_data *(*event_pack)(u32 ulType, u32 ulValue);
    struct command_data *(*record)(u8 ucMode, u8 *pcData, u32 ulLen);
    struct protocol_header *(*parse_pack)(u8 *pcData, u32 ulLen);
    struct command_data *(*ack)(u8 ucSequence, u32 ulErrcode, u8 *pcData, u32 ulLen);
    struct command_data *(*get_prop_resp)(u8 ucSequence, u32 ulErrcode, u8 *pcData, u32 ulLen);
    struct command_data *(*authenticate)(u8 ucSequence, u8 ucPhase, u8 *pcData, u32 ulLen);
    struct command_data *(*large_event_pack)(u8 ulType, u8 *pcData, u32 ulLen);
};

struct protocol_manager *get_protocol_manager_instance(void);

#endif
