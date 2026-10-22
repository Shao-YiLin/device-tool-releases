#pragma once
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	struct dllTestStruct {

		int type;
		int(*init)(struct dllTestStruct *dll, int type);
		int(*print)(struct dllTestStruct *dll);
		int(*unnint)(struct dllTestStruct *dll);
	};

	DllExport void dllTestConstruct(struct dllTestStruct *dll);

#ifdef __cplusplus
}
#endif
