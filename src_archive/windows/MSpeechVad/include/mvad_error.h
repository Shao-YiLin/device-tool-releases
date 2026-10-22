#ifndef __MVAD_ERROR_H__
#define __MVAD_ERROR_H__

#define VAD_SUCCESS             (0x0000)
#define VAD_FAILED              (0x1000)

#define VAD_FINDSTART           (VAD_SUCCESS | 0x01)
#define VAD_FINDSUBSTART        (VAD_SUCCESS | 0x02)
#define VAD_FINDSUBEND           (VAD_SUCCESS | 0x03)
#define VAD_FINDEND             (VAD_SUCCESS | 0x04)
#define VAD_HAS_SEG             (VAD_SUCCESS | 0x05)
#define VAD_NO_SEG              (VAD_SUCCESS | 0x06)

#define VAD_INVALID_PARAM       (VAD_FAILED | 0x01)
#define VAD_UNSUPPORT_SAMPLE    (VAD_FAILED | 0x02)
#define VAD_ALIGNERROR          (VAD_FAILED | 0x03)


#endif //__MVAD_ERROR_H__