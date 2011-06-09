/* smbutil.c --- Main library functions.
 * Copyright (C) 2002, 2004, 2005, 2006, 2008 Simon Josefsson
 * Copyright (C) 1999-2001 Grant Edwards
 * Copyright (C) 2004 Frediano Ziglio
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifndef _WINDOWS
#include <unistd.h>
#endif
#include <byteswap.h>

#include "ntlm.h"
#include "md4.h"
#include "../../src/common/Hash/md5/md5.h"
#include <assert.h>

#include "des.h"
#include "md4.h"

char versionString[] = PACKAGE_STRING;

/* Utility routines that handle NTLM auth structures. */

/*
 * Must be multiple of two
 * We use a statis buffer of 1024 bytes for message
 * At maximun we but 48 bytes (ntlm responses) and 3 unicode strings so
 * NTLM_BUFSIZE * 3 + 48 <= 1024
 */
#define NTLM_BUFSIZE 320

/*
 * all bytes in our structures are aligned so just swap bytes so 
 * we have just to swap order 
 */
#ifdef WORDS_BIGENDIAN
# define UI16LE(n) bswap_16(n)
# define UI32LE(n) bswap_32(n)
#else
# define UI16LE(n) (n)
# define UI32LE(n) (n)
#endif

/* I am not crazy about these macros -- they seem to have gotten
 * a bit complex.  A new scheme for handling string/buffer fields
 * in the structures probably needs to be designed
 */
#define AddBytes(ptr, header, buf, count) \
{ \
	if (NTLM_VER(ptr) == 2) { \
		ptr->v2.header.len = ptr->v2.header.maxlen = UI16LE(count); \
		ptr->v2.header.offset = UI32LE((ptr->v2.buffer - ((uint8*)ptr)) + ptr->v2.bufIndex); \
		memcpy(ptr->v2.buffer+ptr->v2.bufIndex, buf, count); \
		ptr->v2.bufIndex += count; \
	} else { \
		ptr->v1.header.len = ptr->v1.header.maxlen = UI16LE(count); \
		ptr->v1.header.offset = UI32LE((ptr->v1.buffer - ((uint8*)ptr)) + ptr->v1.bufIndex); \
		memcpy(ptr->v1.buffer+ptr->v1.bufIndex, buf, count); \
		ptr->v1.bufIndex += count; \
	} \
}

#define AddString(ptr, header, string) \
{ \
	const char *p = (string); \
	size_t len = p ? strlen(p) : 0; \
	AddBytes(ptr, header, p, len); \
}

#define AddStringLen(ptr, header, string, len) \
{ \
	AddBytes(ptr, header, string, len); \
}

#define AddUnicodeStringLen(ptr, header, string, len) \
{ \
	unsigned char buf[NTLM_BUFSIZE]; \
	unsigned char *b = strToUnicode(string, len, buf); \
	AddBytes(ptr, header, b, len*2); \
}

#define AddUnicodeString(ptr, header, string) \
{ \
	size_t len = string ? strlen(string) : 0; \
	AddUnicodeStringLen(ptr, header, string, len); \
}

#define GetUnicodeString(structPtr, header, output) \
	NTLM_VER(structPtr) == 2 ? \
		getUnicodeString(UI32LE(structPtr->v2.header.offset), \
						UI16LE(structPtr->v2.header.len), \
						((char*)structPtr), \
						(structPtr->v2.buffer - (uint8*) structPtr), \
						sizeof(structPtr->v2.buffer), \
						output) : \
		getUnicodeString(UI32LE(structPtr->v1.header.offset), \
						UI16LE(structPtr->v1.header.len), \
						((char*)structPtr), \
						(structPtr->v1.buffer - (uint8*) structPtr), \
						sizeof(structPtr->v1.buffer), \
						output)

#define GetString(structPtr, header, output) \
		getString(structPtr, &structPtr->v1.header, output)

#define DumpBuffer(fp, structPtr, header) \
	NTLM_VER(structPtr) == 2 ? \
		dumpBuffer(	fp, \
					UI32LE(structPtr->v2.header.offset), \
					UI16LE(structPtr->v2.header.len), \
					((char*)structPtr), \
					(structPtr->v2.buffer - (uint8*) structPtr), \
					sizeof(structPtr->v2.buffer)) : \
		dumpBuffer(	fp, \
					UI32LE(structPtr->v1.header.offset), \
					UI16LE(structPtr->v1.header.len), \
					((char*)structPtr), \
					(structPtr->v1.buffer - (uint8*) structPtr), \
					sizeof(structPtr->v1.buffer))


static void
dumpRaw (FILE * fp, const unsigned char *buf, size_t len)
{
  size_t i;

  for (i = 0; i < len; ++i)
    fprintf (fp, "%02x ", buf[i]);

  fprintf (fp, "\n");
}

static inline void
dumpBuffer (FILE * fp, uint32 offset, uint32 len, char *structPtr,
	    size_t buf_start, size_t buf_len)
{
  /* prevent buffer reading overflow */
  if (offset < buf_start || offset > buf_len + buf_start
      || offset + len > buf_len + buf_start)
    len = 0;
  dumpRaw (fp, structPtr + offset, len);
}

static char *
unicodeToString (const char *p, size_t len, char *buf)
{
	const char *e = p + len;
	char *out = buf;

	if (len >= NTLM_BUFSIZE)
		len = NTLM_BUFSIZE - 1;

	while (p < e)
	{
		*out++ = *p & 0x7f;
		p += 2;
	}

	*out = '\0';
	return buf;
}

static inline char *
getUnicodeString(uint32 offset,
				uint32 len,
				char *structPtr,
				size_t buf_start,
				size_t buf_len,
				char *output)
{
  /* prevent buffer reading overflow */
  if (offset < buf_start || offset > buf_len + buf_start
      || offset + len > buf_len + buf_start)
    len = 0;
  return unicodeToString (structPtr + offset, len / 2, output);
}

static unsigned char *
strToUnicode (const char *p, size_t l, unsigned char *buf)
{
	if (p)
	{
		int i = 0;

		if (l > (NTLM_BUFSIZE / 2))
			l = (NTLM_BUFSIZE / 2);

		while (l--)
		{
			buf[i++] = *p++;
			buf[i++] = 0;
		}
	}

	return buf;
}

static char *
toString (const char *p, size_t len, char *buf)
{
  if (len >= NTLM_BUFSIZE)
    len = NTLM_BUFSIZE - 1;

  memcpy (buf, p, len);
  buf[len] = 0;
  return buf;
}

typedef struct
{
	char ident[8];
	uint32 msgType;
} SmbCommonFields;

char *getString(void *ptr, tSmbStrHeader *hdr, char *output)
{
	SmbCommonFields *common = (SmbCommonFields*) ptr;
	uint8 *buffer = 0;
	tSmbNtlmFlagBits *flags = 0;
	int buf_start, unicode = 0;

	switch (common->msgType)
	{
		case 1:
		{
			tSmbNtlmAuthNegotiate *p = (tSmbNtlmAuthNegotiate*)ptr;
			buffer = p->v1.flagBits.NEGOTIATE_VERSION ? p->v2.buffer : p->v1.buffer;
			flags = &p->v1.flagBits;
			break;
		}
		case 2:
		{
			tSmbNtlmAuthChallenge *p = (tSmbNtlmAuthChallenge*)ptr;
			buffer = p->v1.flagBits.NEGOTIATE_VERSION ? p->v2.buffer : p->v1.buffer;
			flags = &p->v1.flagBits;
			break;
		}
		case 3:
		{
			tSmbNtlmAuthResponse *p = (tSmbNtlmAuthResponse*)ptr;
			buffer = p->v1.flagBits.NEGOTIATE_VERSION ? p->v2.buffer : p->v1.buffer;
			flags = &p->v1.flagBits;
			break;
		}
	}

	buf_start = buffer - (uint8*)ptr;
	if (hdr->offset < buf_start ||
		hdr->offset > NTLM_BUF_SIZE + buf_start ||
		hdr->offset + hdr->len > NTLM_BUF_SIZE + buf_start)
		hdr->len = 0;

	if (flags->NEGOTIATE_UNICODE)
		return unicodeToString ((uint8*)ptr + hdr->offset, hdr->len, output);
	else
		return toString ((uint8*)ptr + hdr->offset, hdr->len, output);
}

/*
#define GetString(structPtr, header, output) \
	NTLM_VER(structPtr) == 2 ? \
		getString(	UI32LE(structPtr->v2.header.offset), \
					UI16LE(structPtr->v2.header.len), \
					((char*)structPtr), \
					(structPtr->v2.buffer - (uint8*) structPtr), \
					sizeof(structPtr->v2.buffer), output) : \
		getString(	UI32LE(structPtr->v1.header.offset), \
					UI16LE(structPtr->v1.header.len), \
					((char*)structPtr), \
					(structPtr->v1.buffer - (uint8*) structPtr), \
					sizeof(structPtr->v1.buffer), output)

static inline char *
getString (	uint32 offset,
			uint32 len,
			char *structPtr,
			size_t buf_start,
			size_t buf_len,
			char *output)
{
  if (offset < buf_start || offset > buf_len + buf_start
      || offset + len > buf_len + buf_start)
    len = 0;
  return toString (structPtr + offset, len, output);
}
*/

void
dumpSmbNtlmAuthNegotiate (FILE * fp, tSmbNtlmAuthNegotiate * request)
{
  char buf1[NTLM_BUFSIZE], buf2[NTLM_BUFSIZE];
  fprintf (fp, "NTLM Request:\n"
	   "      Ident = %.8s\n"
	   "      mType = %d\n"
	   "      Flags = %08x\n"
	   "       User = %s\n"
	   "     Domain = %s\n",
	   request->v1.ident,
	   UI32LE (request->v1.msgType),
	   UI32LE (request->v1.flags),
	   GetString (request, domainName, buf1),
	   GetString (request, workStation, buf2));
}

void
dumpSmbNtlmAuthChallenge (FILE * fp, tSmbNtlmAuthChallenge * challenge)
{
  unsigned char buf[NTLM_BUFSIZE];
  fprintf (fp, "NTLM Challenge:\n"
	   "      Ident = %.8s\n"
	   "      mType = %d\n"
	   "     Domain = %s\n"
	   "      Flags = %08x\n"
	   "  Challenge = ",
	   challenge->v1.ident,
	   UI32LE (challenge->v1.msgType),
	   GetUnicodeString (challenge, targetName, buf),
	   UI32LE (challenge->v1.flags));
  dumpRaw (fp, challenge->v1.challengeData, 8);
}

void
dumpSmbNtlmAuthResponse (FILE * fp, tSmbNtlmAuthResponse * response)
{
  unsigned char buf1[NTLM_BUFSIZE], buf2[NTLM_BUFSIZE], buf3[NTLM_BUFSIZE];
  fprintf (fp, "NTLM Response:\n"
	   "      Ident = %.8s\n"
	   "      mType = %d\n"
	   "     LmResp = ", response->v1.ident, UI32LE (response->v1.msgType));
  DumpBuffer (fp, response, lmResponse);
  fprintf (fp, "     NTResp = ");
  DumpBuffer (fp, response, ntResponse);
  fprintf (fp, "     Domain = %s\n"
	   "       User = %s\n"
	   "        Wks = %s\n"
	   "       sKey = ",
	   GetUnicodeString (response, domainName, buf1),
	   GetUnicodeString (response, user, buf2),
	   GetUnicodeString (response, workStation, buf3));
  DumpBuffer (fp, response, sessionKey);
  fprintf (fp, "      Flags = %08x\n", UI32LE (response->v1.flags));
}

static void
buildSmbNtlmAuthNegotiate_userlen(tSmbNtlmAuthNegotiate * request,
								const char *user,
								size_t user_len,
								const char *domain)
{
	request->v1.bufIndex = 0;
	memcpy (request->v1.ident, "NTLMSSP\0\0\0", 8);
	request->v1.msgType = UI32LE (1);
	request->v1.flags = UI32LE(	NTLMSSP_NEGOTIATE_UNICODE |
								NTLM_NEGOTIATE_OEM |
								NTLMSSP_REQUEST_TARGET |
								NTLMSSP_NEGOTIATE_NTLM |
								NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
								NTLMSSP_NEGOTIATE_NTLM2 );
	if (user)
		AddBytes (request, workStation, user, user_len);
	if (domain)
		AddString (request, domainName, domain);
}

void
buildSmbNtlmAuthNegotiate(tSmbNtlmAuthNegotiate * request,
						 const char *user,
						 const char *domain)
{
	const char *p = user ? strchr(user, '@') : 0;
	size_t user_len = user ? strlen(user) : 0;

	if (p)
	{
		if (!domain)
			domain = p + 1;
		user_len = p - user;
	}

	buildSmbNtlmAuthNegotiate_userlen(request, user, user_len, domain);
}

void
buildSmbNtlmAuthNegotiate_noatsplit(	tSmbNtlmAuthNegotiate * request,
									const char *user,
									const char *domain)
{
	buildSmbNtlmAuthNegotiate_userlen(request, user, strlen (user), domain);
}

/*
The general idea:

	UserDom = ConcatenationOf
				(
					Uppercase(User),
					UserDom
				)
	ResponseKeyLM = HMAC_MD5
					(
						MD4(UNICODE(Passwd)),
						UserDom
					)
	
	ClientChallenge = {0, 0, 0, 0, 0, 0, 0, 0} // wtf is this supposed to be?
	ServerChallenge = challenge->v2.challengeData
	ResponseRVersion = {0x01}
	HiResponseRVersion = {0x01}
	Time = The 8-byte little-endian time in GMT... ???
	Temp = ConcatenationOf
			(
				ResponseRVersion,
				HiResponseRVersion,
				Z(6),
				Time,
				ClientChallenge,
				Z(4),
				ServerName,
				Z(4)
			)

	
	LmChallengeResponse = ConcatenationOf
							(
								HMAC_MD5
								(
									ResponseKeyLM,
									ConcatenationOf
									(
										ServerChallenge,
										ClientChallenge
									)
								),
								ClientChallenge
							)
	NtChallengeResponse = ConcatenationOf
							(
								HMAC_MD5
								(
									ResponseKeyNT,
									ConcatenationOf
									(
										ServerChallenge,
										Temp
									)
								),
								Temp
							)

*/
#define Z(num)		memset(t, 0, num); t += num

void
NTOWFv2(const char *password, const char *user, const char *domain, uint8 *output)
{
	uint16 uPass[256] = {0};
	int uPassLen = 0;
	uint8 PassMD4[16];
	char UserDom[256] = "";

	if (password)
	{	// convert password to unicode
		const char *in = password;
		uint16 *out = uPass;
		while (*in)
			*out++ = *in++ & 0x7f;
		uPassLen = (out - uPass) * sizeof(uPass[0]);
		*out++ = 0;
	}

	strcpy(UserDom, user);
	strupr(UserDom);
	if (domain)
		strcat(UserDom, domain);

	md4_buffer((const char *)uPass, uPassLen, PassMD4);

	hmac_md5(	PassMD4, 16,
				UserDom, strlen(UserDom),
				output);
}

#define LM_HASH_LEN 16
#define LM_RESP_LEN 24

#define NTLM_HASH_LEN 16
#define NTLM_RESP_LEN 24

void
md5sum(uint8 *in, int len, uint8 *out)
{
	md5_state_t s;
	md5_init(&s);
	md5_append(&s, in, len);
	md5_finish(&s, out);
}

bool
NTLM_Hash(const char *password, unsigned char *hash)
{
	uint16 uPass[256] = {0};
	int uPassLen = 0;
	const char *in = password;
	uint16 *out = uPass;

	if (!password)
		return false;

	// convert password to unicode
	while (*in)
		*out++ = *in++ & 0x7f;
	uPassLen = (out - uPass) * sizeof(uPass[0]);
	*out++ = 0;

	// md4 it	
	md4_buffer((const char *)uPass, uPassLen, hash);
	return true;
}

static uint8
des_setkeyparity(uint8 x)
{
  if ((((x >> 7) ^ (x >> 6) ^ (x >> 5) ^
        (x >> 4) ^ (x >> 3) ^ (x >> 2) ^
        (x >> 1)) & 0x01) == 0)
    x |= 0x01;
  else
    x &= 0xfe;
  return x;
}

static void
des_makekey(const uint8 *raw, uint8 *key)
{
  key[0] = des_setkeyparity(raw[0]);
  key[1] = des_setkeyparity((uint8) ((raw[0] << 7) | (raw[1] >> 1)) );
  key[2] = des_setkeyparity((uint8) ((raw[1] << 6) | (raw[2] >> 2)) );
  key[3] = des_setkeyparity((uint8) ((raw[2] << 5) | (raw[3] >> 3)) );
  key[4] = des_setkeyparity((uint8) ((raw[3] << 4) | (raw[4] >> 4)) );
  key[5] = des_setkeyparity((uint8) ((raw[4] << 3) | (raw[5] >> 5)) );
  key[6] = des_setkeyparity((uint8) ((raw[5] << 2) | (raw[6] >> 6)) );
  key[7] = des_setkeyparity((uint8) (raw[6] << 1));
}

static void
des_encrypt(const uint8 *key, const uint8 *src, uint8 *hash)
{
	gl_des_ctx c;
	gl_des_setkey(&c, key);
	gl_des_ecb_encrypt(&c, src, hash);
}

void
LM_Response(const uint8 *hash, const uint8 *challenge, uint8 *response)
{
	uint8 keybytes[21], k1[8], k2[8], k3[8];

	memcpy(keybytes, hash, 16);
	memset(keybytes + 16, 0, 5);

	des_makekey(keybytes     , k1);
	des_makekey(keybytes +  7, k2);
	des_makekey(keybytes + 14, k3);

	des_encrypt(k1, challenge, response);
	des_encrypt(k2, challenge, response + 8);
	des_encrypt(k3, challenge, response + 16);
}

void
buildSmbNtlmAuthResponse(tSmbNtlmAuthChallenge * challenge,
						tSmbNtlmAuthResponse * response,
						const char *user,
						const char *workstation,
						const char *domain,
						const char *password,
						const uint8 time[8])
{

	uint8 lmResp[LM_RESP_LEN], ntlmResp[NTLM_RESP_LEN], ntlmHash[NTLM_HASH_LEN];
	if (challenge->v2.flagBits.NEGOTIATE_NTLM2)
	{
		uint8 sessionHash[16], temp[16];

		GenerateRandom(lmResp, 8);
		memset(lmResp + 8, 0, LM_RESP_LEN - 8);

		memcpy(temp, challenge->v2.challengeData, 8);
		memcpy(temp + 8, lmResp, 8);
		md5sum(temp, 16, sessionHash);

		NTLM_Hash(password, ntlmHash);
		LM_Response(ntlmHash, sessionHash, ntlmResp);
	}
	else
	{
		NTLM_Hash(password, ntlmHash);
		LM_Response(ntlmHash, challenge->v2.challengeData, ntlmResp);
	}


	response->v1.flags =	NTLMSSP_NEGOTIATE_UNICODE |
							NTLMSSP_REQUEST_TARGET |
							NTLMSSP_NEGOTIATE_NTLM |
							NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
							NTLMSSP_NEGOTIATE_NTLM2;
	memcpy (response->v1.ident, "NTLMSSP\0\0\0", 8);
	response->v1.msgType = UI32LE (3);

	if (NTLM_VER(response) == 2)
		response->v2.bufIndex = 0;
	else
		response->v1.bufIndex = 0;

	if (response->v1.flagBits.NEGOTIATE_UNICODE)
	{
		AddUnicodeString(response, domainName, domain);
		AddUnicodeString(response, user, user);
		AddUnicodeString(response, workStation, workstation);
	}
	else
	{
		AddString (response, domainName, domain);
		AddString(response, user, user);
		AddString(response, workStation, workstation);
	}

	AddBytes(response, lmResponse, lmResp, sizeof(lmResp));
	AddBytes(response, ntResponse, ntlmResp, sizeof(ntlmResp));
	// AddString(response, sessionKey, NULL);
}

