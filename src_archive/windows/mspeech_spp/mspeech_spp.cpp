#include "stdafx.h"
#include <string.h>
#include <malloc.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>
#include <string>

#include "mspeech_spp.h"
#include "spp_manager.h"

#pragma comment(lib, "Bthprops.lib")

#ifdef DEBUG
#define log printf
#else
#define log printf
#endif // DEBUG

void mspeech_spp_scan(char *scan_name, int size, int connected, int scan_time, mspeech_spp_scan_cb_fn callback) {

	HANDLE hRadio;
	BLUETOOTH_FIND_RADIO_PARAMS btfrp = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
	BLUETOOTH_RADIO_INFO bRInfo = { sizeof(BLUETOOTH_RADIO_INFO) };
	BLUETOOTH_DEVICE_INFO bDInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
	BLUETOOTH_DEVICE_SEARCH_PARAMS btdsp = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
	HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&btfrp, &hRadio);
	HBLUETOOTH_DEVICE_FIND hDFind = NULL;

	struct mspeech_spp_info sppDeviceArray[5] = { 0 };
	int sppDeviceSize = 0;

	if (NULL != hFind) {
		do {
			if (BluetoothGetRadioInfo(hRadio, &bRInfo) == ERROR_SUCCESS) {
	
				btdsp.hRadio = hRadio;
				if (connected) {
					btdsp.fReturnConnected = TRUE;
					btdsp.cTimeoutMultiplier = 0;
				}
				else {
					btdsp.fReturnConnected = FALSE;
					btdsp.fReturnUnknown = TRUE;
					btdsp.cTimeoutMultiplier = scan_time;
				}
				

				hDFind = BluetoothFindFirstDevice(&btdsp, &bDInfo);

				if (NULL != hDFind) {
					log("Find Device Follow ======>\n");
					do
					{
					//	log("1\n");
						char _deviceName[48] = { 0 };
						int len = WideCharToMultiByte(CP_ACP, 0, bDInfo.szName, wcslen(bDInfo.szName), _deviceName, sizeof(_deviceName), NULL, NULL);
						log("device name: %s, address:%llx \n", _deviceName, bDInfo.Address.ullLong);
						//std::string patternName = new std::string(scan_name,size);
						if (memcmp(scan_name, _deviceName, size) == 0) {
							sppDeviceArray[sppDeviceSize].device_address = bDInfo.Address.ullLong;
							sppDeviceSize++;
						}
					} while (BluetoothFindNextDevice(hDFind, &bDInfo));
				}
				else {
					log("not Find Device Follow ======>\n");
				}
				BluetoothFindDeviceClose(hDFind);
			}
			CloseHandle(hRadio);
		} while (BluetoothFindNextRadio(hFind, &hRadio));
	}
	BluetoothFindRadioClose(hFind);
	if (sppDeviceSize > 0 && callback) {
		callback(sppDeviceArray, sppDeviceSize);
	}
}

 int msppech_spp_inquiry(struct mspeech_spp_info *device)
{
	 HANDLE hRadio;
	 BLUETOOTH_FIND_RADIO_PARAMS btfrp = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
	 BLUETOOTH_RADIO_INFO bRInfo = { sizeof(BLUETOOTH_RADIO_INFO) };
	 BLUETOOTH_DEVICE_INFO bDInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };
	 BLUETOOTH_DEVICE_SEARCH_PARAMS btdsp = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
	 HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio(&btfrp, &hRadio);
	 HBLUETOOTH_DEVICE_FIND hDFind = NULL;
	 if (NULL != hFind) {
		 do {
			 if (BluetoothGetRadioInfo(hRadio, &bRInfo) == ERROR_SUCCESS) {

				 btdsp.hRadio = hRadio;
				 btdsp.fReturnConnected = FALSE;
				 btdsp.fReturnUnknown = TRUE;
				 btdsp.fReturnAuthenticated = TRUE;
				 btdsp.fReturnRemembered = TRUE;
				 btdsp.cTimeoutMultiplier = 10;
				 btdsp.fIssueInquiry = true;

				 hDFind = BluetoothFindFirstDevice(&btdsp, &bDInfo);
				 int error = WSAGetLastError();

				 if (NULL != hDFind) {
					 do
					 {
						 if (device->device_address == bDInfo.Address.ullLong) {

							 if (bDInfo.fAuthenticated == 0) {
								 //BLUETOOTH_OOB_DATA_INFO oobDataInfo = { 0 };
								 //oobDataInfo.C[0] = '1';
								 //oobDataInfo.C[1] = '2';
								 //oobDataInfo.C[2] = '3';
								 //oobDataInfo.C[3] = '4';
								 //oobDataInfo.C[4] = '\0';

								 //BLUETOOTH_AUTHENTICATE_RESPONSE authRespond = { 0 };
								 //authRespond.passkeyInfo.passkey = 1234;
								 //authRespond.authMethod = BLUETOOTH_AUTHENTICATION_METHOD_PASSKEY;
								 //BluetoothSendAuthenticationResponseEx(hRadio, &authRespond);

								 DWORD errorCode = BluetoothUpdateDeviceRecord(&bDInfo);
								 errorCode = BluetoothAuthenticateDeviceEx(NULL, hRadio, &bDInfo, NULL, MITMProtectionNotRequired);
								 if (errorCode == ERROR_SUCCESS) {
									 return 1;
								 }
								 else {
									 log("bluetooth  authenticate failed \n");
									 return -1;
								 }
							 }
							 else
							 {
								 DWORD errorCode = BluetoothUpdateDeviceRecord(&bDInfo);
								 errorCode = BluetoothAuthenticateDeviceEx(NULL, hRadio, &bDInfo, NULL, MITMProtectionRequiredBonding);
								 if (errorCode == ERROR_SUCCESS) {
									 return 1;
								 }
								 else {
									 log("bluetooth  bind failed \n");
									 return -2;
								 }
							 }

						 }

					 } while (BluetoothFindNextDevice(hDFind, &bDInfo));
				 }
				 BluetoothFindDeviceClose(hDFind);
			 }
			 CloseHandle(hRadio);
		 } while (BluetoothFindNextRadio(hFind, &hRadio));

	 }

	 BluetoothFindRadioClose(hFind);
	 return 0;
}

static struct spp_manager *_get_handle(struct mspeech_spp *spp) {
	return (struct spp_manager *)spp->handle;
}

static void _init(struct mspeech_spp *spp) {
	if (NULL == spp->handle)
		spp->handle = malloc(sizeof(struct spp_manager));
	spp_manager_construct(_get_handle(spp));
}

static void _uninit(struct mspeech_spp *spp) {

	spp->disconnect(spp);

	if (spp->handle) {
		free(spp->handle);
	}
}

static int _connect(struct mspeech_spp *spp, struct mspeech_spp_info *device) {
	struct device_info info;
	info.device_address = device->device_address;
	return _get_handle(spp)->connect(_get_handle(spp), &info);
}

static int _disconnect(struct mspeech_spp *spp) {
	return _get_handle(spp)->disconnect(_get_handle(spp));
}

static int _read(struct mspeech_spp *spp, char *data, int size) {
	return _get_handle(spp)->read(_get_handle(spp), data, size);
}

static int _write(struct mspeech_spp *spp, char *data, int size) {
	return _get_handle(spp)->send(_get_handle(spp), data, size);
}

void mspeech_spp_construct(struct mspeech_spp *spp) {

	if (spp) {
		spp->init = _init;
		spp->uninit = _uninit;
		spp->connect = _connect;
		spp->disconnect = _disconnect;
		spp->read = _read;
		spp->write = _write;
		spp->handle = NULL;
	}
}

