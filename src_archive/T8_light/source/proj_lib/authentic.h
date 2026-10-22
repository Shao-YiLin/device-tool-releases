
#ifndef _S256_H
#define _S256_H

#ifdef __cplusplus
extern "C" {
#endif

  void authentic(unsigned char input[], int inputLen, unsigned char output[], int outLen);

  char* getVersion();

#ifdef __cplusplus
}
#endif

#endif
