#pragma once

#include <Winsock2.h>

struct device_info {
	char device_name[64];
	unsigned long long device_address;
};

struct spp_manager {
	SOCKET socket;
	int(*connect)(struct spp_manager *sm, struct device_info *device);
	int(*disconnect)(struct spp_manager *sm);
	int(*read)(struct spp_manager *sm, char *data, int size);
	int(*send)(struct spp_manager *sm, char *data, int size);
};

void spp_manager_construct(struct spp_manager *sm);
