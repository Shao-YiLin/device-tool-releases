

#include "AESCrypt.h"
#include "aes.h"
#include <cassert>
#include <cstring>

AESCrypt::AESCrypt(const unsigned char *key, size_t keyLength) {
	if (key && keyLength > 0) {
		memcpy((void*)m_key, (void*)key, (keyLength > AES_KEY_LEN) ? AES_KEY_LEN : keyLength);
		memcpy((void*)m_vector, (void*)m_key, AES_KEY_LEN);

		int ret = openssl::AES_set_encrypt_key(m_key, AES_KEY_BITSET_LEN, &m_aesKey);
		assert(ret == 0);
	}
}

void AESCrypt::reset() {
	m_number = 0;
	memcpy((void*)m_vector, (void*)m_key, AES_KEY_LEN);
}

void AESCrypt::getKey(void *output) const {
	if (output) {
		memcpy((void*)output, (void*)m_key, AES_KEY_LEN);
	}
}

void AESCrypt::encrypt(const unsigned char *input, unsigned char *output, size_t length) {
	if (!input || !output || length == 0) {
		return;
	}
	openssl::AES_cfb128_encrypt(input, output, length, &m_aesKey, m_vector, &m_number, AES_ENCRYPT);
}

void AESCrypt::decrypt(const unsigned char *input, unsigned char *output, size_t length) {
	if (!input || !output || length == 0) {
		return;
	}
	openssl::AES_cfb128_encrypt(input, output, length, &m_aesKey, m_vector, &m_number, AES_DECRYPT);
}
