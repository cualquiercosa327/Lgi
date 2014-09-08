#ifndef _TEXT_FILE_H_
#define _TEXT_FILE_H_

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
	int Used, BytePos;
	EncodingType Type;
	GArray<char> Buf;

public:
	GTextFile()
	{
		Used = 0;
		BytePos = 0;
		Type = Unknown;
	}
	
	EncodingType GetType()
	{
		return Type;
	}

	template<typename T>
	int GetLine(GArray<T> &Out)
	{
		if (!Buf.Length())
		{
			Buf.Length(4 << 10);
			
			Used = Read(&Buf[0], Buf.Length());
			char *Cur = &Buf[0];
			char *End = Cur + Used;

			if (Used > 2 && Cur[0] == 0xEF && Cur[1] == 0xBB && Cur[2] == 0xBF)
			{
				Type = Utf8;
				BytePos += 3;
			}
			else if (Used > 1 && Cur[0] == 0xFE && Cur[1] == 0xFF)
			{
				Type = Utf16BE;
				BytePos += 2;
			}
			else if (Used > 1 && Cur[0] == 0xFF && Cur[1] == 0xFE)
			{
				Type = Utf16LE;
				BytePos += 2;
			}
			else if (Used > 3 && Cur[0] == 0x00 && Cur[1] == 0x00 && Cur[2] == 0xFE && Cur[3] == 0xFF)
			{
				Type = Utf32BE;
				BytePos += 4;
			}
			else if (Used > 3 && Cur[0] == 0xFF && Cur[1] == 0xFE && Cur[2] == 0x00 && Cur[3] == 0x00)
			{
				Type = Utf32LE;
				BytePos += 4;
			}
			else
			{
				// try and detect the char type
				int u8 = 0, u16 = 0, u32 = 0;
				GPointer p;
				p.s8 = Cur;
				for (int i=0; i<min(Used, 256); i++)
				{
					if (p.u8 + i < (uint8*)End && p.u8[i] == ',')
						u8++;
					if (p.u16 + i < (uint16*)(End-1) && p.u16[i] == ',')
						u16++;
					if (p.u32 + i < (uint32*)(End-3) && p.u32[i] == ',')
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
		
		return Out.Length();
	}
};

#endif