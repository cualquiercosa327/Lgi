/// \file
/// \author Matthew Allen
/// \created 1/6/98
/// \brief    Base64 encoding/decoding
#ifndef __BASE64_H_
#define __BASE64_H_

#include "LgiDefs.h"

// These buffer length macros round up to the nearest block of
// bytes. So take the value returned by the convert routine as
// the actual value... but use these to allocate buffers.
#define BufferLen_64ToBin(l)		( ((l)*3)/4 )
#define BufferLen_BinTo64(l)		( ((((l)+2)/3)*4) )

// Character Conversion Routines
//
// Format of Base64 char:
// 7               0
// |-|-|-|-|-|-|-|-|
// |-U-|--- Data --|
//
// Data	= Bits, 0-63
// U	= Unused, must be 0
//
LgiFunc uchar Base64ToBin(char c);
LgiFunc char BinToBase64(uchar c);

// String Conversion Routines
//
// Arguments:
//	Binary:		Pointer to binary buffer
//	OutBuf:		Size of output buffer (in bytes)
//	Base64:		Pointer to Base64 buffer
//	InBuf:		Size of input buffer (in bytes)
//
// Returns:
//	Number of bytes converted.
//
LgiFunc ssize_t ConvertBase64ToBinary(uchar *Binary, ssize_t OutBuf, char *Base64, ssize_t InBuf);
LgiFunc ssize_t ConvertBinaryToBase64(char *Base64, ssize_t OutBuf, uchar *Binary, ssize_t InBuf);

#endif
