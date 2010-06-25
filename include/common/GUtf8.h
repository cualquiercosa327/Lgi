/// \file
/// \author Matthew Allen
#ifndef _GUTF_H_
#define _GUTF_H_

/// Pointer to utf-8 string
class LgiClass GUtf8Ptr
{
protected:
	uint8 *Ptr;

public:
	GUtf8Ptr(char *p = 0);

	/// Assign a new pointer to the string
	GUtf8Ptr &operator =(char *s) { Ptr = (uint8*)s; return *this; }
	/// Assign a new pointer to the string
	GUtf8Ptr &operator =(uint8 *s) { Ptr = s; return *this; }
	/// \returns the current character in the string
	operator uint32();
	/// Seeks 1 character forward
	uint32 operator ++(const int n);
	/// Seeks 1 character backward
	uint32 operator --(const int n);
	/// Seeks 'n' characters forward
	uint32 operator +=(const int n);
	/// Seeks 'n' characters backward
	uint32 operator -=(const int n);

	/// Gets the bytes between the cur pointer and the end of the buffer or string.
	int GetBytes();
	/// Gets the characters between the cur pointer and the end of the buffer or string.
	int GetChars();
	/// Gets the current ptr
	uint8 *GetCurrent() { return Ptr; }
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