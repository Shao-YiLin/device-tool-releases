#ifndef COMMON_PROTOCOL_LOGIC_MANAGER_H_
#define COMMON_PROTOCOL_LOGIC_MANAGER_H_

#include "libprotocol_logic_define.h"
#include "libspeech_protocol.h"

typedef void(*PROTOCOL_PARSE_CALLBACK)(void* param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size);

struct CommonProtocolManager {

	LOGIC_ACTION_CALLBACK pCBfn;
	PROTOCOL_PARSE_CALLBACK pParseCBfn;
	struct libspeech_protocol pProtocolHandle;
	void *pThreadHandle;
	int threadState;
	void *pParam;

	void(*init)(struct CommonProtocolManager *pm, LOGIC_ACTION_CALLBACK callback, PROTOCOL_PARSE_CALLBACK parseCB, void *param);
	void(*uninit)(struct CommonProtocolManager *pm);
	void(*append)(struct CommonProtocolManager *pm, unsigned char *data, int size);
};

void CommonProtocolManagerConstruct(struct CommonProtocolManager *manager);

#endif // !COMMON_PROTOCOL_LOGIC_MANAGER_H_

