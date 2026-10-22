#ifndef  SENSOR_PROXY_INC
#define  SENSOR_PROXY_INC

#include "sensor_manager.h"
#if SENSOR_FUN_ENABLE_AAA
// mos:mouse optical sensor
enum mos_index {
	MOS_SIGMA_MX8912,
	MOS_PAW_3220,
//	MOS_PAW_3311,
	MOS_INDEX_MAX,
};

struct mos_pid {
	uint16_t pid;
	enum mos_index index;
	struct mos_template *(*construct)(void);
};

struct mos_proxy {
	struct mos_template *current;
	struct mos_template *(*init)();
};

struct mos_template *mos_proxy_get_instance(void);

#endif
#endif   /* ----- #ifndef SENSOR_PROXY_INC  ----- */
