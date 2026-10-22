#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define DllImport   __declspec( dllimport )
#define DllExport   __declspec( dllexport )

	typedef enum
	{
		MSPEECH_ENCODE_ICO = 0x1,
		MSPEECH_ENCODE_OPUS
	} MSpeechEncodeType;

	struct mspeech_pcm
	{
		void *handle;
		int(*init)(struct mspeech_pcm *pcm, int encodeType, int channels);
		int(*convert2pcm)(struct mspeech_pcm *pcm, unsigned short *in, unsigned short *out, unsigned short *out_len);
		void(*uninit)(struct mspeech_pcm *pcm);
	};

	DllExport void mspeech_pcm_construct(struct mspeech_pcm *pcm);

#ifdef __cplusplus
}
#endif