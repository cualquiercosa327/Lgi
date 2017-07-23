#ifndef _TEXT_FILE_H_
#define _TEXT_FILE_H_

#include "GUtf8.h"

class GTextFile : public GFile
{
public:
	enum EncodingType
	{
		Unknown,
		Utf8,
		Utf16BE,
		Utf16LE,
		Utf32BE,
		Utf32LE,
	};

protected:
	bool First;
	size_t Used;
	bool InEndOfLine;
	GPointer Pos;
	EncodingType Type;
	GArray<uint8> Buf;
	GAutoString Charset;

public:
	GTextFile(const char *charset = NULL)
	{
		First = true;
		InEndOfLine = false;
		Used = 0;
		Pos.u8 = NULL;
		Type = Unknown;
		if (charset)
			Charset.Reset(NewStr(charset));
	}
	
	EncodingType GetType()
	{
		return Type;
	}
	
	const char *GetTypeString()
	{
		switch (Type)
		{
			case Utf8: return "utf-8";
			case Utf16BE: return "utf-16be";
			case Utf16LE: return "utf-16";
			case Utf32BE: return "utf-32be";
			case Utf32LE: return "utf-32";
			default: break;
		}
		return NULL;
	}

	bool GetVariant(const char *Name, GVariant &Value, char *Array = NULL)
	{
		if (LgiStringToDomProp(Name) == FileEncoding)
		{
			Value = (int)Type;
			return true;
		}
		
		return GFile::GetVariant(Name, Value, Array);
	}
	
	/// Read the whole file as utf-8
	GAutoString Read()
	{
		GAutoString Ret;
		int Sz = (int)GetSize();
		if (Sz > 0)
		{
			GAutoPtr<uint8, true> Buf(new uint8[Sz]);
			if (Buf)
			{
				ssize_t Rd = Read(Buf, Sz);
				if (Rd > 0)
				{
					const char *Cs = GetTypeString();
					if (Cs)
						Ret.Reset((char*)LgiNewConvertCp("utf-8", Buf, Cs, Sz));
				}
			}
		}
		
		return Ret;
	}
	
	/// Read the whole file as wchar_t
	GAutoWString ReadW()
	{
		GAutoWString Ret;
		int Sz = (int)GetSize();
		if (Sz > 0)
		{
			GAutoPtr<uint8, true> Buf(new uint8[Sz]);
			if (Buf)
			{
				ssize_t Rd = Read(Buf, Sz);
				if (Rd > 0)
				{
					const char *Cs = GetTypeString();
					if (Cs)
						Ret.Reset((char16*)LgiNewConvertCp(LGI_WideCharset, Buf, Cs, Sz));
				}
			}
		}
		
		return Ret;
	}

	template<typename T>
	bool CheckForNull(T *ptr, uint8 *end)
	{
		while ((uint8*)ptr < (end-sizeof(T)))
		{
			if (*ptr == 0)
				return false;
			ptr++;
		}
		return true;
	}
		
	ssize_t Read(void *Buffer, ssize_t Size, int Flags = 0)
	{
		ssize_t Rd = GFile::Read(Buffer, Size, Flags);
		if (First)
		{
			if (Rd < 4)
				LgiAssert(!"Initial read is too small");
			else
			{
				First = false;

				uint8 *buf = (uint8*)Buffer;
				uint8 *start = buf;
				if (Used > 2 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF)
				{
					Type = Utf8;
					start += 3;
				}
				else if (Used > 1 && buf[0] == 0xFE && buf[1] == 0xFF)
				{
					Type = Utf16BE;
					start += 2;
				}
				else if (Used > 1 && buf[0] == 0xFF && buf[1] == 0xFE)
				{
					Type = Utf16LE;
					start += 2;
				}
				else if (Used > 3 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0xFE && buf[3] == 0xFF)
				{
					Type = Utf32BE;
					start += 4;
				}
				else if (Used > 3 && buf[0] == 0xFF && buf[1] == 0xFE && buf[2] == 0x00 && buf[3] == 0x00)
				{
					Type = Utf32LE;
					start += 4;
				}
				else
				{
					// Try and detect the char type
					uint8 *end = buf + Rd;
					if (CheckForNull(buf, end))
						Type = Utf8;
					else if (CheckForNull((uint16*)buf, end))
						Type = Utf16LE;
					else 
						Type = Utf32LE;
				}
				
				ptrdiff_t bytes = start - buf;
				if (bytes > 0 && bytes <= Rd)
				{
					// Remove byte order mark from the buffer
					memmove(buf, start, Rd - bytes);
					Rd -= bytes;
				}
			}
		}
		
		return Rd;
	}

	bool FillBuffer()
	{
		if (!Buf.Length())
		{
			Buf.Length(4 << 10);
			Pos.u8 = &Buf[0];
			Used = 0;
		}
		
		if (Buf.Length())
		{
			// Move any consumed data down to the start of the buffer
			size_t BytePos = Pos.u8 - &Buf[0];
			size_t Remaining = Used - BytePos;
			if (BytePos > 0 && Remaining > 0)
			{
				memmove(&Buf[0], &Buf[BytePos], Remaining);
				Used = Remaining;
			}
			else
			{
				Used = 0;
			}
			Pos.u8 = &Buf[0];
			
			// Now read some more data into the free space
			Remaining = Buf.Length() - Used;
			LgiAssert(Remaining > 0);
			ssize_t Rd = Read(&Buf[Used], Remaining);
			if (Rd <= 0)
				return 0;
			
			Used += Rd;			
		}
		
		return true;
	}
	
	template<typename T>
	int GetLine(GArray<T> &Out)
	{
		uint8 *End = NULL;
		
		int OutPos = 0;
		if (Buf.Length())
			End = &Buf[0] + Used;
		
		while (true)
		{
			if (!End || End - Pos.u8 < sizeof(T))
			{
				if (!FillBuffer())
					break;
				End = &Buf[0] + Used;
			}

			if (End - Pos.u8 < sizeof(T))
				break;
			
			T ch = 0;
			switch (Type)
			{
				case Utf16BE:
					LgiAssert(sizeof(ch) == 2);
					ch = LgiSwap16(*Pos.u16);
					Pos.u16++;
					break;
				case Utf16LE:
					LgiAssert(sizeof(ch) == 2);
					ch = *Pos.u16++;
					break;
				case Utf32BE:
					LgiAssert(sizeof(ch) == 4);
					ch = LgiSwap32(*Pos.u32);
					Pos.u32++;
					break;
				case Utf32LE:
					LgiAssert(sizeof(ch) == 4);
					ch = *Pos.u32++;
					break;
				default: // Utf8
				{
					ssize_t len = (int) (End - Pos.u8);
					ch = LgiUtf8To32(Pos.u8, len);
					break;
				}
			}
			
			if (ch == 0)
				break;

			if (ch == '\r' || ch == '\n')
			{
				if (InEndOfLine)
				{
					continue;
				}
				else
				{
					InEndOfLine = true;
					break;
				}
			}
			else
			{
				InEndOfLine = false;
			}
			
			Out[OutPos++] = ch;
		}
		
		Out[OutPos] = 0; // NULL terminate				
		return OutPos;
	}
};

#endif