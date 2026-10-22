#pragma once
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )


	typedef void(*ifly_sm_cb_fn)(int status);

	DllExport void ifly_sm_adapter(ifly_sm_cb_fn cb);

#ifdef __cplusplus
}
#endif