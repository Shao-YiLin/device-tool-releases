
#pragma once
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	struct mspeech_spp_info {
		unsigned long long device_address;
	};

	typedef void(*mspeech_spp_scan_cb_fn)(struct mspeech_spp_info *device, unsigned int size);

	DllExport void mspeech_spp_scan(char *scan_name, int size, int connected, int scan_time, mspeech_spp_scan_cb_fn callback);

	/**
	* @return  1: success -1:authentication fail -2:bind fail
	*/
	DllExport int msppech_spp_inquiry(struct mspeech_spp_info *device);

	struct mspeech_spp
	{
		void *handle;
		void(*init)(struct mspeech_spp *sm);
		void(*uninit)(struct mspeech_spp *sm);
		int(*connect)(struct mspeech_spp *sm, struct mspeech_spp_info *device);
		int(*disconnect)(struct mspeech_spp *sm);
		int(*read)(struct mspeech_spp *sm, char *data, int size);
		int(*write)(struct mspeech_spp *sm, char *data, int size);
	};

	DllExport void mspeech_spp_construct(struct mspeech_spp *spp);

#ifdef __cplusplus
}
#endif