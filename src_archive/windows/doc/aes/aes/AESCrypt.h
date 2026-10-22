

#ifndef AES_CRYPT_H_
#define AES_CRYPT_H_

#ifdef __cplusplus

#include "aes.h"
#include <cstddef>

#define AES_KEY_LEN 16
#define AES_KEY_BITSET_LEN 128

// a AES CFB-128 encrypt-decrypt full-duplex wrapper
class AESCrypt {
    unsigned char m_vector[AES_KEY_LEN] = {0};
    unsigned char m_key[AES_KEY_LEN] = {0};
    openssl::AES_KEY m_aesKey = {0};
    int m_number = 0;

public:
    AESCrypt(const unsigned char *key, size_t keyLength);

    void encrypt(const unsigned char *input, unsigned char *output, size_t length);

    void decrypt(const unsigned char *input, unsigned char *output, size_t length);

    void reset();

    // output must have [AES_KEY_LEN] space
    void getKey(void *output) const;
};

#ifndef NDEBUG

// check if AESCrypt is encrypt-decrypt full-duplex
void testAESCrypt();

#endif

#endif
#endif /* AES_CRYPT_H_ */
