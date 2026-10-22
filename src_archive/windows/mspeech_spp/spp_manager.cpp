#include "stdafx.h"
#include "spp_manager.h"

#include <ws2bth.h>
#include <ws2def.h>
#include <conio.h>
#include <winsock.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <iostream>

//#include <bthsdpdef.h>
//#include <bluetoothapis.h>
//#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib,"Ws2_32.lib")

#ifdef DEBUG
#define log printf
#else
#define log
#endif // DEBUG


static GUID SppServiceClass_UUID = { 0x00001101, 0x0000, 0x1000, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb };
static int _connect(struct spp_manager *sm, struct device_info *device) {
	if (NULL == device) {
		log("device info is null \n");
		return 0;
	}

	if (INVALID_SOCKET != sm->socket) {
		closesocket(sm->socket);
		sm->socket = INVALID_SOCKET;
	}

	WSADATA wsa_data;
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
		log("failed to get socket version \n");
		return 0;
	}

	sm->socket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (INVALID_SOCKET == sm->socket) {
		log("create socket failed \n");
		return 0;
	}

	SOCKADDR_BTH socketBthServer;
	socketBthServer.addressFamily = AF_BTH;
	socketBthServer.port = 0;
	socketBthServer.btAddr = device->device_address;
	socketBthServer.serviceClassId = SppServiceClass_UUID;

	if (connect(sm->socket, (const sockaddr *)&socketBthServer, sizeof(SOCKADDR_BTH)) == 0) {
		log("socket connect success \n");
		return 1;
	}

	return 0;
}

static int _disconnect(struct spp_manager *sm) {

	if (INVALID_SOCKET != sm->socket) {
		closesocket(sm->socket);
		sm->socket = INVALID_SOCKET;
	}
	return 1;
}

static int _read(struct spp_manager *sm, char *data, int size) {
	if (INVALID_SOCKET == sm->socket)
		return 0;

	int read_count = recv(sm->socket, data, size, 0);
	if (read_count == SOCKET_ERROR)
		return -1;
	else
		return read_count;
}

static int _send(struct spp_manager *sm, char *data, int size) {
	if (INVALID_SOCKET == sm->socket)
		return 0;

	int send_count = send(sm->socket, data, size, 0);
	if (send_count == SOCKET_ERROR) {
		return 0;
	}
	else {
		log("send data count: %d \n", send_count);
	}
	return 1;
}

void spp_manager_construct(struct spp_manager *sm) {
	if (sm) {
		sm->connect = _connect;
		sm->disconnect = _disconnect;
		sm->read = _read;
		sm->send = _send;
		sm->socket = INVALID_SOCKET;
	}
}