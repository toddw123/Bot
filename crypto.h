#ifndef __CRYPTO_H
#define __CRYPTO_H

#define CLIENT_KEY_SIZE 0x100

class Crypto{
public:
	unsigned char m_key[CLIENT_KEY_SIZE];
	Crypto();
	void Encrypt(unsigned char* packet, size_t len);
	void Decrypt(unsigned char* packet, size_t len);
};

#endif