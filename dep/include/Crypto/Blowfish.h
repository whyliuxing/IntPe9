#ifndef __BLOWFISH_H__
#define __BLOWFISH_H__


typedef unsigned long long uint64;
typedef unsigned int uint32;
#define LODWORD(l) ((uint32)((uint64)(l)))
#define HIDWORD(l) ((uint32)(((uint64)(l)>>32)&0xFFFFFFFF))
#define MAKEDWORDLONG(a,b) ((uint64)(((uint32)(a))|(((uint64)((uint32)(b)))<<32)))
uint64 ntohll(uint64 a);

//Block Structure
struct SBlock
{
	//Constructors
	SBlock(unsigned int l=0, unsigned int r=0) : m_uil(l), m_uir(r) {}
	//Copy Constructor
	SBlock(const SBlock& roBlock) : m_uil(roBlock.m_uil), m_uir(roBlock.m_uir) {}
	SBlock& operator^=(SBlock& b) { m_uil ^= b.m_uil; m_uir ^= b.m_uir; return *this; }
	unsigned int m_uil, m_uir;
};

class Blowfish
{
public:
	enum { ECB=0, CBC=1, CFB=2 };

	//Constructor - Initialize the P and S boxes for a given Key
	Blowfish(unsigned char* ucKey, size_t n, const SBlock& roChain = SBlock(0UL,0UL));

	//Resetting the chaining block
	void ResetChain() { m_oChain = m_oChain0; }

	// Encrypt/Decrypt 8 bytes in uint64 types
	uint64 Encrypt(uint64 buf);
	uint64 Decrypt(uint64 buf);

	// Encrypt/Decrypt Buffer in Place
	void Encrypt(unsigned char* buf, size_t n, int iMode=ECB);
	void Decrypt(unsigned char* buf, size_t n, int iMode=ECB);

	// Encrypt/Decrypt from Input Buffer to Output Buffer
	void Encrypt(const unsigned char* in, unsigned char* out, size_t n, int iMode=ECB);
	void Decrypt(const unsigned char* in, unsigned char* out, size_t n, int iMode=ECB);

//Private Functions
private:
	unsigned int F(unsigned int ui);
	void Encrypt(SBlock&);
	void Decrypt(SBlock&);

private:
	//The Initialization Vector, by default {0, 0}
	SBlock m_oChain0;
	SBlock m_oChain;
	unsigned int m_auiP[18];
	unsigned int m_auiS[4][256];
	static const unsigned int scm_auiInitP[18];
	static const unsigned int scm_auiInitS[4][256];
};

//Extract low order byte
inline unsigned char Byte(unsigned int ui)
{
	return (unsigned char)(ui & 0xff);
}

//Function F
inline unsigned int Blowfish::F(unsigned int ui)
{
	return ((m_auiS[0][Byte(ui>>24)] + m_auiS[1][Byte(ui>>16)]) ^ m_auiS[2][Byte(ui>>8)]) + m_auiS[3][Byte(ui)];
}

#endif // __BLOWFISH_H__