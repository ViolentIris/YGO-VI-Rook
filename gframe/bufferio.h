#ifndef BUFFERIO_H
#define BUFFERIO_H

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#endif

#include <cstdint>
#include "../ocgcore/buffer.h"

class BufferIO {
public:
	inline static int ReadInt32(unsigned char*& p) {
		int ret = *(int*)p;
		p += 4;
		return ret;
	}
	inline static unsigned int ReadUInt32(unsigned char*& p) {
		unsigned int ret = *(unsigned int*)p;
		p += 4;
		return ret;
	}
	inline static short ReadInt16(unsigned char*& p) {
		short ret = *(short*)p;
		p += 2;
		return ret;
	}
	inline static unsigned short ReadUInt16(unsigned char*& p) {
		unsigned short ret = *(unsigned short*)p;
		p += 2;
		return ret;
	}
	inline static char ReadInt8(unsigned char*& p) {
		char ret = *(char*)p;
		p++;
		return ret;
	}
	inline static unsigned char ReadUInt8(unsigned char*& p) {
		unsigned char ret = *(unsigned char*)p;
		p++;
		return ret;
	}
	inline static void WriteInt32(unsigned char*& p, int val) {
		(*(int*)p) = val;
		p += 4;
	}
	inline static void WriteInt16(unsigned char*& p, short val) {
		(*(short*)p) = val;
		p += 2;
	}
	inline static void WriteInt8(unsigned char*& p, char val) {
		*p = val;
		p++;
	}
	template<typename T1, typename T2>
	inline static int CopyWStr(T1* src, T2* pstr, int bufsize) {
		int l = 0;
		while(src[l] && l < bufsize - 1) {
			pstr[l] = src[l];
			l++;
		}
		pstr[l] = 0;
		return l;
	}
	template<typename T1, typename T2>
	inline static int CopyWStrRef(T1* src, T2*& pstr, int bufsize) {
		int l = 0;
		while(src[l] && l < bufsize - 1) {
			pstr[l] = src[l];
			l++;
		}
		pstr += l;
		*pstr = 0;
		return l;
	}
	// UTF-16/UTF-32 to UTF-8
	static int EncodeUTF8(const wchar_t * wsrc, char * str) {
		char* pstr = str;
		while(*wsrc != 0) {
			if(*wsrc < 0x80) {
				*str = *wsrc;
				++str;
			} else if(*wsrc < 0x800) {
				str[0] = ((*wsrc >> 6) & 0x1f) | 0xc0;
				str[1] = ((*wsrc) & 0x3f) | 0x80;
				str += 2;
			} else if(*wsrc < 0x10000 && (*wsrc < 0xd800 || *wsrc > 0xdfff)) {
				str[0] = ((*wsrc >> 12) & 0xf) | 0xe0;
				str[1] = ((*wsrc >> 6) & 0x3f) | 0x80;
				str[2] = ((*wsrc) & 0x3f) | 0x80;
				str += 3;
			} else {
#ifdef _WIN32
				unsigned unicode = 0;
				unicode |= (*wsrc++ & 0x3ff) << 10;
				unicode |= *wsrc & 0x3ff;
				unicode += 0x10000;
				str[0] = ((unicode >> 18) & 0x7) | 0xf0;
				str[1] = ((unicode >> 12) & 0x3f) | 0x80;
				str[2] = ((unicode >> 6) & 0x3f) | 0x80;
				str[3] = ((unicode) & 0x3f) | 0x80;
#else
				str[0] = ((*wsrc >> 18) & 0x7) | 0xf0;
				str[1] = ((*wsrc >> 12) & 0x3f) | 0x80;
				str[2] = ((*wsrc >> 6) & 0x3f) | 0x80;
				str[3] = ((*wsrc) & 0x3f) | 0x80;
#endif // _WIN32
				str += 4;
			}
			wsrc++;
		}
		*str = 0;
		return str - pstr;
	}
	// UTF-8 to UTF-16/UTF-32
	static int DecodeUTF8(const char * src, wchar_t * wstr) {
		const char* p = src;
		wchar_t* wp = wstr;
		while(*p != 0) {
			if((*p & 0x80) == 0) {
				*wp = *p;
				p++;
			} else if((*p & 0xe0) == 0xc0) {
				*wp = (((unsigned)p[0] & 0x1f) << 6) | ((unsigned)p[1] & 0x3f);
				p += 2;
			} else if((*p & 0xf0) == 0xe0) {
				*wp = (((unsigned)p[0] & 0xf) << 12) | (((unsigned)p[1] & 0x3f) << 6) | ((unsigned)p[2] & 0x3f);
				p += 3;
			} else if((*p & 0xf8) == 0xf0) {
#ifdef _WIN32
				unsigned unicode = (((unsigned)p[0] & 0x7) << 18) | (((unsigned)p[1] & 0x3f) << 12) | (((unsigned)p[2] & 0x3f) << 6) | ((unsigned)p[3] & 0x3f);
				unicode -= 0x10000;
				*wp++ = (unicode >> 10) | 0xd800;
				*wp = (unicode & 0x3ff) | 0xdc00;
#else
				*wp = (((unsigned)p[0] & 0x7) << 18) | (((unsigned)p[1] & 0x3f) << 12) | (((unsigned)p[2] & 0x3f) << 6) | ((unsigned)p[3] & 0x3f);
#endif // _WIN32
				p += 4;
			} else
				p++;
			wp++;
		}
		*wp = 0;
		return wp - wstr;
	}
	template<size_t N>
	static int EncodeUTF8(const wchar_t* src, char(&dst)[N]) {
		return EncodeUTF8String(src, dst, N);
	}
	template<size_t N>
	static int DecodeUTF8(const char* src, wchar_t(&dst)[N]) {
		return DecodeUTF8String(src, dst, N);
	}
	template<size_t N, typename T>
	static void NullTerminate(T(&str)[N]) {
		str[N - 1] = 0;
	}
	static int GetVal(const wchar_t* pstr) {
		int ret = 0;
		while(*pstr >= L'0' && *pstr <= L'9') {
			ret = ret * 10 + (*pstr - L'0');
			pstr++;
		}
		return ret;
	}
};

#endif //BUFFERIO_H
