#include "mspeech_vad.h"
#include "include/mvad_api.h"
#include "include/mvad_type.h"

int mspeech_vad_create_session(MSPEECH_VAD_INST *pVadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type, const char *szResBuf) {
	
	return VADCreateSession(pVadInst, sample_type, szResBuf);
}

int mspeech_vad_destroy_session(MSPEECH_VAD_INST vadInst)
{
	return VADDestroySession(vadInst);
}

int mspeech_vad_reset_session(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type) {
	return VADResetSession(vadInst, sample_type);
}

int mspeech_vad_reset_sentence(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SAMPLE_TYPE sample_type) {
	return VADResetSentence(vadInst, sample_type);
}

int mspeech_vad_append_pcm(MSPEECH_VAD_INST vadInst, const char* pcmBuf, unsigned int bufByteSize, int *pVolumLevel) {
	return VADAppendPCM(vadInst, pcmBuf, bufByteSize, pVolumLevel);
}

int mspeech_vad_get_seg(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SEG *pSeg) {
	return VADGetSeg(vadInst, (MVAD_SEG *)pSeg);
}

int mspeech_vad_set_param(MSPEECH_VAD_INST vadInst, const char* szKey, const char* szValue) {
	return VADSetParam(vadInst, szKey, szValue);
}

int mspeech_vad_get_sent_confidence(MSPEECH_VAD_INST vadInst, float *value) {
	return VADGetSentConfidence(vadInst, value);
 }

int mspeech_vad_get_last_seg(MSPEECH_VAD_INST vadInst, MSPEECH_VAD_SEG *seg) {
	return VADGetLastSeg(vadInst, (MVAD_SEG *)seg);
 }
