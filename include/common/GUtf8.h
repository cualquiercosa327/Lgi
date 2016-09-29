/// \file
/// \author Matthew Allen
#ifndef _GUTF_H_
#define _GUTF_H_

// Defines for decoding UTF8
#define IsUtf8_1Byte(c)		( ((uchar)(c) & 0x80) == 0x00 )
#define IsUtf8_2Byte(c)		( ((uchar)(c) & 0xe0) == 0xc0 )
#define IsUtf8_3Byte(c)		( ((uchar)(c) & 0xf0) == 0xe0 )
#define IsUtf8_4Byte(c)		( ((uchar)(c) & 0xf8) == 0xf0 )

#define IsUtf8_Lead(c)		( ((uchar)(c) & 0xc0) == 0xc0 )
#define IsUtf8_Trail(c)		( ((uchar)(c) & 0xc0) == 0x80 )

// Stand-alone functions

/// Convert a single utf-8 char to utf-32 or returns -1 on error.
inline int32 LgiUtf8To32(uint8 *&i, int &Len)
{
	int32 Out = 0;

	#define InvalidUtf()		{ Len--; i++; return -1; }

	if (Len > 0)
	{
		if (!*i)
		{
			Len = 0;
			return 0;
		}

		if (IsUtf8_1Byte(*i))
		{
			// 1 byte UTF-8
			Len--;
			return *i++;
		}
		else if (IsUtf8_2Byte(*i))
		{
			// 2 byte UTF-8
			if (Len > 1)
			{
				Out = ((int)(*i++ & 0x1f)) << 6;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= *i++ & 0x3f;
					Len--;
				}
				else InvalidUtf()
			}
		}
		else if (IsUtf8_3Byte(*i))
		{
			// 3 byte UTF-8
			if (Len > 2)
			{
				Out = ((int)(*i++ & 0x0f)) << 12;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= ((int)(*i++ & 0x3f)) << 6;
					Len--;

					if (IsUtf8_Trail(*i))
					{
						Out |= *i++ & 0x3f;
						Len--;
					}
					else InvalidUtf()
				}
				else InvalidUtf()
			}
		}
		else if (IsUtf8_4Byte(*i))
		{
			// 4 byte UTF-8
			if (Len > 3)
			{
				Out = ((int)(*i++ & 0x07)) << 18;
				Len--;

				if (IsUtf8_Trail(*i))
				{
					Out |= ((int)(*i++ & 0x3f)) << 12;
					Len--;

					if (IsUtf8_Trail(*i))
					{
						Out |= ((int)(*i++ & 0x3f)) << 6;
						Len--;

						if (IsUtf8_Trail(*i))
						{
							Out |= *i++ & 0x3f;
							Len--;
						}
						else InvalidUtf()
					}
					else InvalidUtf()
				}
				else InvalidUtf()
			}
		}
		else InvalidUtf()
	}

	return Out;
}

/// Convert a single utf-32 char to utf-8
inline bool LgiUtf32To8(uint32 c, uint8 *&i, int &Len)
{
	if ((c & ~0x7f) == 0)
	{
		if (Len > 0)
		{
			*i++ = c;
			Len--;
			return true;
		}
	}
	else if ((c & ~0x7ff) == 0)
	{
		if (Len > 1)
		{
			*i++ = 0xc0 | (c >> 6);
			*i++ = 0x80 | (c & 0x3f);
			Len -= 2;
			return true;
		}
	}
	else if ((c & 0xffff0000) == 0)
	{
		if (Len > 2)
		{
			*i++ = 0xe0 | (c >> 12);
			*i++ = 0x80 | ((c & 0x0fc0) >> 6);
			*i++ = 0x80 | (c & 0x3f);
			Len -= 3;
			return true;
		}
	}
	else
	{
		if (Len > 3)
		{
			*i++ = 0xf0 | (c >> 18);
			*i++ = 0x80 | ((c&0x3f000) >> 12);
			*i++ = 0x80 | ((c&0xfc0) >> 6);
			*i++ = 0x80 | (c&0x3f);
			Len -= 4;
			return true;
		}
	}

	return false;
}

// Defined for decoding UTF16
#define IsUtf16_Lead(c)		( ((uint16)(c) & 0xfc00) == 0xD800 )
#define IsUtf16_Trail(c)	( ((uint16)(c) & 0xfc00) == 0xDc00 )

/// Convert a single utf-16 char to utf-32
inline uint32 LgiUtf16To32(uint16 *&i, int &Len)
{
	if (Len > 1)
	{
		if (!*i)
		{
			Len = 0;
			return 0;
		}

		int n = *i & 0xfc00;
		if (n == 0xd800 || n == 0xdc00)
		{
			// 2 word UTF
			if (Len > 3)
			{
				Len -= sizeof(uint16)<<1;
				int w = (*i & 0x3c0) >> 6;
				int zy = *i++ & 0x3f;
				return ((w + 1) << 16) | (zy << 10) | (*i++ & 0x3ff);
			}
		}

		// 1 word UTF
		Len -= sizeof(uint16);
		return *i++;
	}

	return 0;
}

/// Convert a single utf-32 char to utf-16
inline void LgiUtf32To16(uint32 c, uint16 *&i, int &Len)
{
	if (c & 0xffff0000)
	{
		// 2 word UTF
		if (Len > 3)
		{
			int w = (c >> 16) - 1;
			*i++ = 0xd800 | (w << 6) | ((c & 0xfc00) >> 10);
			*i++ = 0xdc00 | (c & 0x3ff);
			Len -= sizeof(uint16) << 1;
		}
	}

	// 1 word UTF
	if (Len > 1)
	{
		*i++ = c;
		Len -= sizeof(uint16);
	}
}

/// Seeks the pointer 'Ptr' to the next utf-8 character
inline bool LgiNextUtf8(char *&p)
{
	char *old = p;
	
	if (IsUtf8_Lead(*p))
		p++;

	while (IsUtf8_Trail(*p))
		p++;
	
	return p > old;
}

/// Seeks the pointer 'Ptr' to the previous utf-8 character
inline void LgiPrevUtf8(char *&p)
{
	p--;
	while (IsUtf8_Trail(*p))
		p--;
}

/// Pointer to utf-8 string
class LgiClass GUtf8Ptr
{
protected:
	uint8 *Ptr;

public:
	GUtf8Ptr(void *p = 0);

	/// Assign a new pointer to the string
	GUtf8Ptr &operator =(char *s) { Ptr = (uint8*)s; return *this; }
	/// Assign a new pointer to the string
	GUtf8Ptr &operator =(uint8 *s) { Ptr = s; return *this; }
	/// \returns the current character in the string or -1 on error.
	operator int32();
	/// Seeks 1 character forward
	/// \returns current character or -1 on error
	int32 operator ++(const int n);
	/// Seeks 1 character backward
	/// \returns current character or -1 on error
	int32 operator --(const int n);
	/// Seeks 'n' characters forward
	/// \returns current character or -1 on error
	int32 operator +=(const int n);
	/// Seeks 'n' characters backward
	/// \returns current character or -1 on error
	int32 operator -=(const int n);

	/// Gets the bytes between the cur pointer and the end of the buffer or string.
	int GetBytes();
	/// Gets the characters between the cur pointer and the end of the buffer or string.
	int GetChars();
	/// Encodes a utf-8 char at the current location and moves the pointer along
	void Add(char16 c);

	/// Returns the current pointer.
	uint8 *GetPtr() { return Ptr; }
};

/// Unicode string class. Allows traversing a utf-8 strings.
class LgiClass GUtf8Str : public GUtf8Ptr
{
	// Complete buffer
	uint8 *Start;
	uint8 *End;
	GUtf8Ptr Cur;
	bool Own;

	void Empty();

public:
	/// Constructor
	GUtf8Str
	(
		/// The string pointer to start with
		char *utf,
		/// The number of bytes containing characters, or -1 if NULL terminated.
		int bytes = -1,
		/// Copy the string first
		bool Copy = false
	);
	/// Constructor
	GUtf8Str
	(
		/// The string pointer to start with. A utf-8 copy of the string will be created.
		char16 *wide,
		/// The number of wide chars, or -1 if NULL terminated.
		int chars = -1
	);
	~GUtf8Str();

	/// Assign a new pointer to the string
	GUtf8Str &operator =(char *s);

	/// Allocates a block of memory containing the wide representation of the string.
	char16 *ToWide();
	/// \returns true if the class seems to be valid.
	bool Valid();
	/// \returns true if at the start
	bool IsStart();
	/// \returns true if at the end
	bool IsEnd();
};

#endif