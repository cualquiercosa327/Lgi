#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#include "Lzw.h"

#define HasLZW

#define TIFF_LZW_BUFFER					(512<<10)  // anyone know how big this should be??

#define MaxCode(CodeSize)  ((1 << (CodeSize))-1)
#define MaxHashTable  5003
#define MaxGIFBits  12
#if defined(HasLZW)
#define MaxGIFTable  (1 << MaxGIFBits)
#else
#define MaxGIFTable  MaxCode
#endif

class LzwPrivate
{
public:
	// Buffering
	int Datum;
	int BitPos;
	int BitsLeft;
	GStream *Pipe;

	// Read buffer
	int InBufSize;
	uchar *InPos;
	uchar *InBuf;

	// Codes
	int CodeSize;
	int ClearCode;
	int EoiCode;
	int NextCode;
	int MaxCode;

	int Codes[18041];
	int PrefixCode[18041];
	uchar AppendChar[18041];

	// Methods
	LzwPrivate()
	{
		InBufSize = 64 << 10;
		InBuf = 0;
	}
	
	~LzwPrivate()
	{
		DeleteArray(InBuf);
	}

	char *DecodeString(char *Buffer, int Code)
	{
		int i = 0;
		while (Code > 255)
		{
			*Buffer++ = AppendChar[Code];
			Code = PrefixCode[Code];
			if (Code < 0 ||
				Code >= NextCode ||
				Code == 256 ||
				Code == 257 ||
				i++ >= 4094)
			{
				printf("Fatal error during code expansion.\n");
				return 0;
			}
		}
		
		*Buffer = Code;
		return Buffer;
	}

	void InitializeTable()
	{
		CodeSize = 9;
		ClearCode = 1 << (CodeSize - 1);
		EoiCode = ClearCode + 1;
		NextCode = EoiCode + 1;
		MaxCode = (1 << CodeSize) - 1;

		ZeroObj(Codes);
		ZeroObj(PrefixCode);
		ZeroObj(AppendChar);
		for (int i=0; i<256; i++)
		{
			AppendChar[i] = i;
		}
	}

	int InputCode()
	{
		int Code = 0;
		int b = CodeSize;
		uchar Mask[] = { 0, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

		while (b > 0)
		{
			if (BitsLeft < 1)
			{
				DeleteArray(InBuf);
				BitsLeft = 0;
				InBuf = new uchar[InBufSize];
				if (InBuf)
				{
					if (Pipe)
					{
						int Size = (int)Pipe->GetSize();
						int Len = min(InBufSize, Size);
						int r = Pipe->Read(InBuf, Len);
						BitsLeft += r * 8;
					}
					/*
					else if (File)
					{
						BitsLeft = File->Read(InBuf, InBufSize) * 8;
					}
					*/

					InPos = InBuf;
					BitPos = 0;
				}

				if (BitsLeft <= 0)
				{
					return EoiCode;
				}
			}

			int ThisBits = min(8-BitPos, b);
			int Temp = Mask[ThisBits] & (*InPos >> (8 - (ThisBits + BitPos)));
			Code |= Temp << (b - ThisBits);
			
			b -= ThisBits;
			BitsLeft -= ThisBits;
			BitPos += ThisBits;
			if (BitPos > 7)
			{
				BitPos -= 8;
				InPos++;
			}
		}
		
		return Code;
	}

/*
	while ((Code = GetNextCode()) != EoiCode)
	{
		if (Code == ClearCode)
		{
			InitializeTable();
			Code = GetNextCode();
			if (Code == EoiCode)
				break;
			WriteString(StringFromCode(Code));
			OldCode = Code;
		}
		else
		{
			if (IsInTable(Code))
			{
				WriteString(StringFromCode(Code));
				AddStringToTable(StringFromCode(OldCode) + FirstChar(StringFromCode(Code)));
				OldCode = Code;
			}
			else
			{
				OutString = StringFromCode(OldCode) + FirstChar(StringFromCode(OldCode));
				WriteString(OutString);
				AddStringToTable(OutString);
				OldCode = Code;
			}
		}
	}
*/
	bool Decompress(GStream *Out, GStream *in, Progress *Meter, int Block)
	{
		int Code = 0;

		Pipe = in;
		if (Out && Pipe)
		{
			CodeSize = 9;
			ClearCode = 256;
			EoiCode = ClearCode + 1;
			NextCode = EoiCode + 1;
			BitPos = 0;
			BitsLeft = 0;

			int OldCode = 0;
			char *s;
			int Char = 0;
			char Buf[4000];
			int Done = 0;
			int64 MeterPos = Meter ? Meter->Value() : 0;

			while ((Code = InputCode()) != EoiCode)
			{
				if (Code == ClearCode)
				{
					InitializeTable();
					Code = InputCode();
					if (Code == EoiCode)
					{
						break;
					}
					
					s = DecodeString(Buf, Code);
					Char = *s; 
					while (s >= Buf)
					{
						Out->Write(s--, 1);
					}
				}
				else
				{
					if (Code >= NextCode)
					{
						/*
						** This code checks for the special STRING+CHARACTER+STRING+CHARACTER+STRING
						** case which generates an undefined code.  It handles it by decoding
						** the last code, and adding a single character to the end of the decode string.
						*/
						Buf[0] = Char;
						s = DecodeString(Buf+1, OldCode);
					}
					else
					{
						/*
						** Otherwise we do a straight decode of the new code.
						*/
						s = DecodeString(Buf, Code);
					}

					if (!s)
					{
						return false;
					}
					
					// Now we output the decoded string in reverse order.
					Char = *s;
					while (s >= Buf)
					{
						Out->Write(s--, 1);
					}

					// Finally, if possible, add a new code to the string table.
					if (NextCode <= MaxCode)
					{
						PrefixCode[NextCode] = OldCode;
						AppendChar[NextCode] = Char;
						NextCode++;

						switch (NextCode)
						{
							case 511:
							case 1023:
							case 2047:
							{
								CodeSize++;
								MaxCode = (1 << CodeSize) - 1;
								break;
							}
							case 4095:
							{
								InitializeTable();
								break;
							}
						}
					}
					else
					{
						LgiAssert(0);
						break;
					}
				}

				OldCode = Code;
				if ((Done++ & 0xffff) == 0 && Meter)
				{
					int64 Blocks = Out->GetSize();
					if (Block) Blocks /= Block;
					Meter->Value(MeterPos + Blocks);
				}
			}

			Out->Write(0, 0);
			return true;
		}

		return false;
	}

	void Flush(bool Final = false)
	{
		if (Final || (InPos >= InBuf + InBufSize))
		{
			if (Final && BitPos > 0)
			{
				// Flush any buffered bits
				*InPos++ = Datum;
			}

			ptrdiff_t Len = InPos - InBuf;
			if (Pipe)
			{
				Pipe->Write(InBuf, (int)Len);
			}
			/*
			else
			{
				File->Write(InBuf, Len);
			}
			*/
			InPos = InBuf;
			BitsLeft = InBufSize * sizeof(*InPos);
		}
	}

	void OutputCode(int code)
	{
		// Emit a code.
		Datum |= ((long) code << BitPos);
		BitPos += CodeSize;
		
		while (BitPos >= 8)
		{
			// Add a character to current buffer
			Flush();
			if (InPos < InBuf + InBufSize)
			{
				*InPos++ = Datum & 0xff;
			}
			
			Datum >>= 8;
			BitPos -= 8;
		}
		
		if (NextCode > MaxCode) 
		{
			CodeSize++;
			
			if (CodeSize == MaxGIFBits)
			{
				MaxCode=MaxGIFTable;
			}
			else
			{
				MaxCode=MaxCode(CodeSize);
			}
		}
	}

	bool Compress(GStream *Out, GStream *In, Progress *Meter, int Block)
	{
		bool Status = false;
		short index, WaitingCode;
		int InputLen = (int)In->GetSize();

		// Allocate encoder tables.
		uchar *packet = new uchar[256];
		short *hash_code = new short[MaxHashTable];
		short *hash_prefix = new short[MaxHashTable];
		uchar *hash_suffix = new uchar[MaxHashTable];
		uchar *Input = new uchar[InputLen];

		if (In &&
			Out &&
			Input &&
			packet &&
			hash_code &&
			hash_prefix &&
			hash_suffix)
		{
			// Initialize GIF encoder.
			CodeSize = 9;
			MaxCode = MaxCode(CodeSize);
			ClearCode = 1 << (CodeSize - 1);
			EoiCode = ClearCode + 1;
			NextCode = ClearCode + 2;
			BitPos = Datum = 0;
			InPos = InBuf = new uchar[InBufSize];
			Pipe = Out;
			memset(hash_code, 0, sizeof(*hash_code)*MaxHashTable);

			OutputCode(ClearCode);

			// Encode pixels.
			In->Read(Input, InputLen);
			uchar *End = Input + InputLen;
			WaitingCode = *Input;

			DoEvery Do(200);
			if (Meter)
			{
				Meter->SetLimits(0, InputLen-1);
			}

			for (uchar *p = Input + 1; p < End; p++)
			{
				// Probe hash table.
				index = *p;
				int k = ((int) index << (MaxGIFBits - 8)) + WaitingCode;
				if (k >= MaxHashTable)
				{
					k -= MaxHashTable;
				}

				#if defined(HasLZW)
				if (hash_code[k] > 0)
				{
					if ((hash_prefix[k] == WaitingCode) &&
						(hash_suffix[k] == index))
					{
						WaitingCode = hash_code[k];
						continue;
					}
					
					bool next_pixel = false;
					int displacement = (k == 0) ? 1 : MaxHashTable - k;

					for (;;)
					{
						k -= displacement;
						
						if (k < 0)
						{
							k += MaxHashTable;
						}
						
						if (hash_code[k] == 0)
						{
							break;
						}
						
						if ((hash_prefix[k] == WaitingCode) &&
							(hash_suffix[k] == index))
						{
							WaitingCode = hash_code[k];
							next_pixel = true;
							break;
						}
					}
				
					if (next_pixel == true)
					{
						continue;
					}
				}
				#endif
				
				OutputCode(WaitingCode);
				if (NextCode < MaxGIFTable)
				{
					hash_code[k] = NextCode++;
					hash_prefix[k] = WaitingCode;
					hash_suffix[k] = (uchar)index;
				}
				else
				{
					// Fill the hash table with empty entries.
					memset(hash_code, 0, sizeof(*hash_code)*MaxHashTable);

					// Reset compressor and issue a clear code.
					NextCode = ClearCode+2;
					OutputCode(ClearCode);
					CodeSize = 9;
					MaxCode = MaxCode(CodeSize);
				}

				WaitingCode=index;

				int v = (int) (p-Input);
				if (Meter && (v%1000 == 0))
				{
					Meter->Value(v);
				}
			}

			// Flush out the buffered code.
			OutputCode(WaitingCode);
			OutputCode(EoiCode);

			// Flush accumulated data.
			Flush(true);
			Status = true;
		}
		
		// Free encoder memory.
		DeleteArray(hash_suffix);
		DeleteArray(hash_prefix);
		DeleteArray(hash_code);
		DeleteArray(packet);
		DeleteArray(Input);
		
		return Status;
	}
};

//////////////////////////////////////////////////////////////////
Lzw::Lzw()
{
	d = new LzwPrivate;
	Meter = 0;
	MeterBlock = 0;
}

Lzw::~Lzw()
{
	DeleteObj(d);
}

void Lzw::SetBufSize(int i)
{
}

bool Lzw::Decompress(GStream *out, GStream *in)
{
	return d->Decompress(out, in, Meter, MeterBlock);
}

bool Lzw::Compress(GStream *out, GStream *in)
{
	return d->Compress(out, in, Meter, MeterBlock);
}



