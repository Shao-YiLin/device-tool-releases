
#ifndef SM_H_
#define SM_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	typedef void(*sm_cb_fn)(void *pData, int len, int tag);

	DllExport void sm_adapter(void *pData, int len, int tag, sm_cb_fn cb);

#ifdef __cplusplus
}
#endif

#endif // !SM_H_