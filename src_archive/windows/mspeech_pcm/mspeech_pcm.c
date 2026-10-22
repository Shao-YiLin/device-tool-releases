#include <malloc.h>

#include "include/libspeech_voice.h"
#include "mspeech_pcm.h"

//struct mspeech_pcm

static struct libspeech_voice *_get_handle(struct mspeech_pcm *pcm) {
	return pcm->handle;
}
static int _init(struct mspeech_pcm *pcm, int encodeType, int channels) {
	if (NULL == pcm->handle)
		pcm->handle = malloc(sizeof(struct libspeech_voice));

	libspeech_voice_construct(pcm->handle);

	return _get_handle(pcm)->init(_get_handle(pcm), encodeType, channels);
}
static int _convert2pcm(struct mspeech_pcm *pcm, unsigned short *in, unsigned short *out, unsigned short *out_len) {
	return _get_handle(pcm)->decode(_get_handle(pcm), in, out, out_len);
}
static void _uninit(struct mspeech_pcm *pcm) {
	_get_handle(pcm)->uninit(_get_handle(pcm));
	if (pcm->handle) {
		free(pcm->handle);
		pcm->handle = NULL;
	}
}

void mspeech_pcm_construct(struct mspeech_pcm *pcm) {
	if (pcm) {
		pcm->init = _init;
		pcm->convert2pcm = _convert2pcm;
		pcm->uninit = _uninit;
		pcm->handle = NULL;
	}
}