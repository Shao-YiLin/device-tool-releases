#include "sensor_proxy.h"
#include "sensor_base.h"
#if SENSOR_FUN_ENABLE_AAA
static struct mos_pid sensor_map[MOS_INDEX_MAX] = {
	{0x300, MOS_SIGMA_MX8912, mos_8912_base_get_instance},
	{0x305, MOS_PAW_3220, mos_3220_base_get_instance},
//	{0x500, MOS_PAW_3311, mos_3311_base_get_instance},
};

#define PID_addr_1 0
#define PID_addr_2 1

static struct mos_template *__init()
{
	uint16_t pid, i;
	struct mos_template *tmp = NULL;
	pid = base_get_construct()->sensor_identity(PID_addr_1, PID_addr_2) >> 4;
	for (i = 0; i < sizeof(sensor_map) / sizeof(sensor_map[0]); i++) {
		if (pid == sensor_map[i].pid) {
			tmp = sensor_map[i].construct();
			return tmp;
		}
	}
	return tmp;

}

struct mos_proxy *sp = NULL;
struct mos_proxy _sp = {
	.init = __init,
	.current = NULL
};

struct mos_template *mos_proxy_get_instance()
{
	if (sp == NULL) {
		sp = &_sp;
		sp->current = sp->init();
	}
	return sp->current;
}
#endif
