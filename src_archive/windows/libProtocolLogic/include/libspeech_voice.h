#ifndef _LIBSPEECH_VOICE_H_
#define _LIBSPEECH_VOICE_H_

#ifdef __cplusplus
extern "C"
{
#endif

    struct libspeech_voice
    {
        void *handle;
        int (*init)(struct libspeech_voice *voice);
        void (*reset)(struct libspeech_voice *voice);
        int (*decode)(struct libspeech_voice *voice, unsigned short *in, unsigned short *out, unsigned short *outLen);
        void (*uninit)(struct libspeech_voice *voice);
    };

    void libspeech_voice_construct(struct libspeech_voice *voice);

#ifdef __cplusplus
}
#endif

#endif