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
	int Used;
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

	bool GetVariant(const char *Name, GVariant &Value, char *Array = NULL)
	{
		if (GStringToProp(Name) == FileEncoding)
		{
			Value = (int)Type;
			return true;
		}
		
		return GFile::GetVariant(Name, Value, Array);
	}
		
	int Read(void *Buffer, int Size, int Flags = 0)
	{
		int Rd = GFile::Read(Buffer, Size, Flags);
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
					#define Detect(c) ((c) == ' ' || (c) == '\n')

					uint8 *end = buf + Rd;
					
					// Check for utf-8 first...
					GUtf8Ptr utf(buf);
					bool InvalidUtf = false;
					while (utf.GetPtr() < end - 6) // Leave enough bytes not to run over the end of the buffer
					{						
						int32 u = utf;
						if (u < 0)
						{
							InvalidUtf = true;
							break;
						}
						utf++;
					}
					
					if (!InvalidUtf)
					{
						Type = Utf8;
					}
					else
					{
						// Check for utf16 or utf32?
						GPointer p;
						p.u8 = buf;
						bool HasNull = false;

						int u16 = 0, u32 = 0;
						for (int i=0; i<min(Rd, 1024); i++)
						{
							if (p.u16 + i < (uint16*)(end-1))
							{
								if (p.u16[i] == 0)
									HasNull = true;
								else if (Detect(p.u16[i]))
									u16++;
							}							
							if (p.u32 + i < (uint32*)(end-3))
							{
								if (Detect(p.u32[i]))
									u32++;
							}
						}
						
						if (HasNull && u16 > 5)
							Type = Utf16LE;
						else
							Type = Utf32LE;
					}
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
			int BytePos = (int) (Pos.u8 - &Buf[0]);
			int Remaining = (int) (Used - BytePos);
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
			int Rd = Read(&Buf[Used], Remaining);
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
					int len = (int) (End - Pos.u8);
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