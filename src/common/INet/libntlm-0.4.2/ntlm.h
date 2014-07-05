/* ntlm.h --- Header file for libntlm.                                -*- c -*-
 *
 * This file is part of libntlm.
 *
 * Libntlm is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libntlm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libntlm; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _LIBNTLM_H
# define _LIBNTLM_H

#if defined(WINDOWS)
	#ifdef LIBNTLM_EXPORTS
		#define LIBEXTERN extern __declspec(dllexport)
	#else
		#define LIBEXTERN extern __declspec(dllimport)
	#endif
#else
	#define LIBEXTERN extern
#endif

# ifdef __cplusplus
extern "C"
{
# endif

  /* Get FILE. */
#include <stdio.h>

  typedef unsigned short uint16;
  typedef unsigned int uint32;
  typedef unsigned char uint8;

#define NTLM_VERSION								"0.4.2"

/* Ref for these flags;
 * http://www.opensource.apple.com/source/smb/smb-431/kernel/netsmb/smb_gss.h
 */
#define NTLMSSP_NEGOTIATE_UNICODE					0x00000001
#define NTLM_NEGOTIATE_OEM							0x00000002
#define NTLMSSP_REQUEST_TARGET						0x00000004
#define NTLMSSP_RESERVED_9							0x00000008
#define NTLMSSP_NEGOTIATE_SIGN						0x00000010
#define NTLMSSP_NEGOTIATE_SEAL						0x00000020
#define NTLMSSP_NEGOTIATE_DATAGRAM					0x00000040
#define NTLMSSP_NEGOTIATE_LM_KEY					0x00000080
#define NTLMSSP_RESERVED_8							0x00000100
#define NTLMSSP_NEGOTIATE_NTLM						0x00000200
#define NTLMSSP_NEGOTIATE_NT_ONLY					0x00000400
#define NTLMSSP_RESERVED_7							0x00000800
#define NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED		0x00001000
#define NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED	0x00002000
#define NTLMSSP_RESERVED_6							0x00004000
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN				0x00008000
#define NTLMSSP_TARGET_TYPE_DOMAIN					0x00010000
#define NTLMSSP_TARGET_TYPE_SERVER					0x00020000
#define NTLMSSP_TARGET_TYPE_SHARE					0x00040000
#define NTLMSSP_NEGOTIATE_NTLM2						0x00080000
#define NTLMSSP_NEGOTIATE_IDENTIFY					0x00100000
#define NTLMSSP_RESERVED_5							0x00200000
#define NTLMSSP_REQUEST_NON_NT_SESSION_KEY			0x00400000
#define NTLMSSP_NEGOTIATE_TARGET_INFO				0x00800000
#define NTLMSSP_RESERVED_4							0x01000000
#define NTLMSSP_NEGOTIATE_VERSION					0x02000000
#define NTLMSSP_RESERVED_3							0x04000000
#define NTLMSSP_RESERVED_2							0x08000000
#define NTLMSSP_RESERVED_1							0x10000000
#define NTLMSSP_NEGOTIATE_128						0x20000000
#define NTLMSSP_NEGOTIATE_KEY_EXCH					0x40000000
#define NTLMSSP_NEGOTIATE_56						0x80000000

typedef struct
{
	uint32 NEGOTIATE_UNICODE					: 1;
	uint32 NEGOTIATE_OEM						: 1;
	uint32 REQUEST_TARGET						: 1;
	uint32 RESERVED_9							: 1;
	uint32 NEGOTIATE_SIGN						: 1;
	uint32 NEGOTIATE_SEAL						: 1;
	uint32 NEGOTIATE_DATAGRAM					: 1;
	uint32 NEGOTIATE_LM_KEY						: 1;
	uint32 RESERVED_8							: 1;
	uint32 NEGOTIATE_NTLM						: 1;
	uint32 NEGOTIATE_NT_ONLY					: 1;
	uint32 RESERVED_7							: 1;
	uint32 NEGOTIATE_OEM_DOMAIN_SUPPLIED		: 1;
	uint32 NEGOTIATE_OEM_WORKSTATION_SUPPLIED	: 1;
	uint32 RESERVED_6							: 1;
	uint32 NEGOTIATE_ALWAYS_SIGN				: 1;
	uint32 TARGET_TYPE_DOMAIN					: 1;
	uint32 TARGET_TYPE_SERVER					: 1;
	uint32 TARGET_TYPE_SHARE					: 1;
	uint32 NEGOTIATE_NTLM2						: 1;
	uint32 NEGOTIATE_IDENTIFY					: 1;
	uint32 RESERVED_5							: 1;
	uint32 REQUEST_NON_NT_SESSION_KEY			: 1;
	uint32 NEGOTIATE_TARGET_INFO				: 1;
	uint32 RESERVED_4							: 1;
	uint32 NEGOTIATE_VERSION					: 1;
	uint32 RESERVED_3							: 1;
	uint32 RESERVED_2							: 1;
	uint32 RESERVED_1							: 1;
	uint32 NEGOTIATE_128						: 1;
	uint32 NEGOTIATE_KEY_EXCH					: 1;
	uint32 NEGOTIATE_56							: 1;

} tSmbNtlmFlagBits;

#define NTLM_BUF_SIZE							1024

#define NTLM_VER(ptr)	(((ptr)->v1.flagBits.NEGOTIATE_VERSION) ? 2 : 1)

#define SmbLength(ptr)	(NTLM_VER(ptr) == 2 ? \
							(((ptr)->v2.buffer - (uint8*)(ptr)) + (ptr)->v2.bufIndex) : \
							(((ptr)->v1.buffer - (uint8*)(ptr)) + (ptr)->v1.bufIndex))

/*
 * These structures are byte-order dependant, and should not
 * be manipulated except by the use of the routines provided
 */

typedef struct
{
	uint16 len;
	uint16 maxlen;
	uint32 offset;
} tSmbStrHeader;

typedef struct
{
	uint8 major;				// Use GetVersionEx to fill these out, e.g. 6
	uint8 minor;				// minor OS version number, e.g. 1
	uint16 buildNumber;			// win32 build number, e.g. 7600
	uint8 reserved[3];			// zero
	uint8 ntlmRevisionCurrent;	// NTLMSSP_REVISION_W2K3=0x0f
} tSmbOsVersion;

typedef struct
{
	char ident[8];
	uint32 msgType;
	union {
		uint32 flags;
		tSmbNtlmFlagBits flagBits;
	};
	tSmbStrHeader domainName;
	tSmbStrHeader workStation;
	uint8 buffer[NTLM_BUF_SIZE];
	uint32 bufIndex;
} NtlmAuthNegotiate1;

typedef struct
{
	char ident[8];
	uint32 msgType;
	union {
		uint32 flags;
		tSmbNtlmFlagBits flagBits;
	};
	tSmbStrHeader domainName;
	tSmbStrHeader workStation;
	tSmbOsVersion version;
	uint8 buffer[NTLM_BUF_SIZE];
	uint32 bufIndex;
} NtlmAuthNegotiate2;

typedef union
{
	NtlmAuthNegotiate1 v1;
	NtlmAuthNegotiate2 v2;
} tSmbNtlmAuthNegotiate;

typedef struct
{
	char ident[8];
	uint32 msgType;
	tSmbStrHeader targetName;
	union {
		uint32 flags;
		tSmbNtlmFlagBits flagBits;
	};
	uint8 challengeData[8];
	uint8 reserved[8];
	tSmbStrHeader targetInfo;
	uint8 buffer[NTLM_BUF_SIZE];
	uint32 bufIndex;
} NtlmAuthChallenge1;

typedef struct
{
	char ident[8];
	uint32 msgType;
	tSmbStrHeader targetName;
	union {
		uint32 flags;
		tSmbNtlmFlagBits flagBits;
	};
	uint8 challengeData[8];
	uint8 reserved[8];
	tSmbStrHeader targetInfo;
	tSmbOsVersion version;
	uint8 buffer[NTLM_BUF_SIZE];
	uint32 bufIndex;
} NtlmAuthChallenge2;

typedef union
{
	NtlmAuthChallenge1 v1;
	NtlmAuthChallenge2 v2;
} tSmbNtlmAuthChallenge;

typedef struct
{
	char ident[8];
	uint32 msgType;
	tSmbStrHeader lmResponse;
	tSmbStrHeader ntResponse;
	tSmbStrHeader domainName;
	tSmbStrHeader user;
	tSmbStrHeader workStation;
	tSmbStrHeader sessionKey;
	union {
		uint32 flags;
		tSmbNtlmFlagBits flagBits;
	};
	uint8 buffer[NTLM_BUF_SIZE];
	uint32 bufIndex;
} NtlmAuthResponse1;

typedef struct
{
	char ident[8];
	uint32 msgType;
	tSmbStrHeader lmResponse;
	tSmbStrHeader ntResponse;
	tSmbStrHeader domainName;
	tSmbStrHeader user;
	tSmbStrHeader workStation;
	tSmbStrHeader sessionKey;
	union {
		uint32 flags;
		tSmbNtlmFlagBits flagBits;
	};
	tSmbOsVersion version;
	// uint8 MIC[16];
	uint8 buffer[NTLM_BUF_SIZE];
	uint32 bufIndex;
} NtlmAuthResponse2;

typedef union
{
	NtlmAuthResponse1 v1;
	NtlmAuthResponse2 v2;
} tSmbNtlmAuthResponse;

/* public: */
LIBEXTERN void
dumpSmbNtlmAuthNegotiate(FILE * fp, tSmbNtlmAuthNegotiate * Negotiate);

LIBEXTERN void
dumpSmbNtlmAuthChallenge(FILE * fp, tSmbNtlmAuthChallenge * challenge);

LIBEXTERN void
dumpSmbNtlmAuthResponse(FILE * fp, tSmbNtlmAuthResponse * response);

LIBEXTERN void
buildSmbNtlmAuthNegotiate(	tSmbNtlmAuthNegotiate *negotiate,
							const char *user,
							const char *domain);

/* Same as buildSmbNtlmAuthNegotiate, but won't treat @ in USER as a
 DOMAIN. */
LIBEXTERN void
buildSmbNtlmAuthNegotiate_noatsplit(	tSmbNtlmAuthNegotiate *negotiate,
									const char *user,
									const char *domain);

LIBEXTERN void
buildSmbNtlmAuthResponse(tSmbNtlmAuthChallenge * challenge,
						tSmbNtlmAuthResponse * response,
						const char *user,
						const char *workstation,
						const char *domain,
						const char *password,
						const uint8 time[8]);
/* smbencrypt.c */
LIBEXTERN void
ntlm_smb_encrypt (const char *passwd,
		const uint8 * challenge,
		uint8 * answer);
LIBEXTERN void
ntlm_smb_nt_encrypt (const char *passwd,
		   const uint8 * challenge,
		   uint8 * answer);

LIBEXTERN const char *ntlm_check_version (const char *req_version);

LIBEXTERN char *getString(void *ptr, tSmbStrHeader *hdr, char *output);

LIBEXTERN void
hmac_md5
(
	unsigned char *text, /* pointer to data stream */
	int text_len,        /* length of data stream */
	unsigned char *key,  /* pointer to authentication key */
	int key_len,         /* length of authentication key */
	unsigned char *digest/* caller digest to be filled in */
);

LIBEXTERN int
GenerateRandom(uint8 *ptr, int len);

# ifdef __cplusplus
}
# endif

#endif				/* NTLM_H */
