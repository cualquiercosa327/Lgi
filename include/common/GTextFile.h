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
	int Used;
	GPointer Pos;
	EncodingType Type;
	GArray<uint8> Buf;

public:
	GTextFile()
	{
		Used = 0;
		Pos.u8 = NULL;
		Type = Unknown;
	}
	
	EncodingType GetType()
	{
		return Type;
	}

	bool FillBuffer()
	{
		if (!Buf.Length())
		{
			Buf.Length(4 << 10);
			
			Used = Read(&Buf[0], Buf.Length());
			if (Used <= 0)
				return false;

			Pos.u8 = &Buf[0];
			uint8 *End = Pos.u8 + Used;

			if (Used > 2 && Pos.u8[0] == 0xEF && Pos.u8[1] == 0xBB && Pos.u8[2] == 0xBF)
			{
				Type = Utf8;
				Pos.u8 += 3;
			}
			else if (Used > 1 && Pos.u8[0] == 0xFE && Pos.u8[1] == 0xFF)
			{
				Type = Utf16BE;
				Pos.u8 += 2;
			}
			else if (Used > 1 && Pos.u8[0] == 0xFF && Pos.u8[1] == 0xFE)
			{
				Type = Utf16LE;
				Pos.u8 += 2;
			}
			else if (Used > 3 && Pos.u8[0] == 0x00 && Pos.u8[1] == 0x00 && Pos.u8[2] == 0xFE && Pos.u8[3] == 0xFF)
			{
				Type = Utf32BE;
				Pos.u8 += 4;
			}
			else if (Used > 3 && Pos.u8[0] == 0xFF && Pos.u8[1] == 0xFE && Pos.u8[2] == 0x00 && Pos.u8[3] == 0x00)
			{
				Type = Utf32LE;
				Pos.u8 += 4;
			}
			else
			{
				// try and detect the char type
				int u8 = 0, u16 = 0, u32 = 0;
				for (int i=0; i<min(Used, 256); i++)
				{
					if (Pos.u8 + i < (uint8*)End && Pos.u8[i] == ',')
						u8++;
					if (Pos.u16 + i < (uint16*)(End-1) && Pos.u16[i] == ',')
						u16++;
					if (Pos.u32 + i < (uint32*)(End-3) && Pos.u32[i] == ',')
						u32++;
				}
				if (u32 > 5)
					Type = Utf32LE;
				else if (u16 > 5)
					Type = Utf16LE;
				else
					Type = Utf8;
			}
		}
		else
		{
			// Move any existing data down to the start of the buffer
			int BytePos = Pos.u8 - &Buf[0];
			int Remaining = Used - BytePos;
			if (Remaining > 0)
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
					int len = End - Pos.u8;
					ch = LgiUtf8To32(Pos.u8, len);
					break;
				}
			}
			
			if (ch == '\r')
				continue;
			if (ch == 0 || ch == '\n')
				break;
			Out[OutPos++] = ch;
		}
		
		Out[OutPos] = 0; // NULL terminate				
		return OutPos;
	}
};

#endif