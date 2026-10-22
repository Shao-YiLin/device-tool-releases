/*
 * @Author: xuhan
 * @Date: 2020-02-08 18:10:32
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-08 18:14:00
 * @Description: 
 */
#ifndef _LIBSPEECH_PROTOCOL_H_
#define _LIBSPEECH_PROTOCOL_H_

#ifdef __cplusplus
extern "C" {
#endif

struct libspeech_protocol_command_data
{
    // unsigned char *data;
    unsigned char data[128];
    unsigned char len;
    unsigned char sequence;
};

typedef void (*LIBSPEECH_PROTOCOL_CALLBACK)(void *param, unsigned char cmd, unsigned char sequence, unsigned char *data, unsigned int size);

struct libspeech_protocol {

    void *handle;
    void (*init)(struct libspeech_protocol *protocol, LIBSPEECH_PROTOCOL_CALLBACK listener, void *param);
    void (*uninit)(struct libspeech_protocol *protocol);
    void (*add_data)(struct libspeech_protocol *protocol, unsigned char *data, unsigned int len);
    struct libspeech_protocol_command_data *(*get_command)(struct libspeech_protocol *protocol, unsigned char cmd, unsigned char *payload, unsigned int len);
    void (*start_service)(struct libspeech_protocol *protocol);
    void (*stop_service)(struct libspeech_protocol *protocol);
    void (*set_sleep)(struct libspeech_protocol *protocol, unsigned int time);
};

void libspeech_protocol_construct(struct libspeech_protocol *protocol);

#ifdef __cplusplus
}
#endif

#endif