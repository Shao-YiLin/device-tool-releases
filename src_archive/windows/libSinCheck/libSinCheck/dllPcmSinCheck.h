
#ifndef  PCM_SIN_CHECK_H_
#define PCM_SIN_CHECK_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdint.h>

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	DllExport int dllPcmSinCheck();

#ifdef __cplusplus
}
#endif

#endif // ! PCM_SIN_CHECK_H_


