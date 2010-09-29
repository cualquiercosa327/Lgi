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
	// Codes
	int CodeSize;
	int ClearCode;
	int EoiCode;
	int NextCode;
	int MaxCode;
	uchar *NextBuf;
	int CodeLength[4096];
	uchar *CodePtr[4096];
	int CodeBufSize;
	uchar *CodeBuf;

	// Buffering
	int Datum;
	int BitPos;
	int BitsLeft;
	GStream *Pipe;

	// Read buffer
	int InBufSize;
	uchar *InPos;
	uchar *InBuf;

	#define OutputBytes(buf, len) \
		Out->Write(buf, len);

	// Methods
	LzwPrivate()
	{
		InBufSize = 64 << 10;
		InBuf = 0;
		
		CodeBufSize = TIFF_LZW_BUFFER;
		CodeBuf = new uchar[TIFF_LZW_BUFFER];
	}
	
	~LzwPrivate()
	{
		DeleteArray(InBuf);
		DeleteArray(CodeBuf);
	}

	void BufSizeCheck(int Size)
	{
		if (Size >= CodeBufSize)
		{
			// Grow the buffer
			int NewSize = CodeBufSize << 2;
			uchar *NewBuf = new uchar[NewSize];
			if (NewBuf)
			{
				// Copy over the codes
				memcpy(CodeBuf, NewBuf, CodeBufSize);

				// Update all the pointers
				NextBuf = NewBuf + (NextBuf - CodeBuf);
				for (int i=0; i<=NextCode; i++)
				{
					CodePtr[i] = NewBuf + (CodePtr[i] - CodeBuf);
				}

				// Free the old memory
				DeleteArray(CodeBuf);

				// Put the new block in place
				CodeBuf = NewBuf;
				CodeBufSize = NewSize;
			}
		}
	}

	void InitializeTable()
	{
		CodeSize = 9;
		ClearCode = 1 << (CodeSize - 1);
		EoiCode = ClearCode + 1;
		NextCode = EoiCode + 1;

		ZeroObj(CodeLength);
		ZeroObj(CodePtr);
		NextBuf = CodeBuf;
		for (int i=0; i<256; i++)
		{
			CodeLength[i] = 1;
			CodePtr[i] = NextBuf++;
			*CodePtr[i] = i;
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
						int Size = Pipe->GetSize();
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

	bool Decompress(GStream *Out, GStream *in, Progress *Meter)
	{
		int Code = 0, OldCode = 0;
		uchar *OutString;
		int Max = 0;

		Pipe = in;
		if (Out AND Pipe)
		{
			CodeSize = 9;
			ClearCode = 1 << (CodeSize - 1);
			EoiCode = ClearCode + 1;
			NextCode = EoiCode + 1;
			BitPos = 0;
			BitsLeft = 0;

			while ((Code = InputCode()) != EoiCode AND NextCode < 4096)
			{
				if (Code == ClearCode)
				{
					InitializeTable();
					Code = InputCode();
					if (Code == EoiCode)
					{
						break;
					}
					OutputBytes(CodePtr[Code], CodeLength[Code]);
					OldCode = Code;
				}
				else
				{
					if (Code < NextCode)
					{
						Out->Write(CodePtr[Code], CodeLength[Code]);

						uchar *d = CodePtr[OldCode];
						int Length = CodeLength[OldCode];
						CodeLength[NextCode] = Length + 1;
						CodePtr[NextCode] = NextBuf;

						BufSizeCheck( (NextBuf - CodeBuf) + Length + 1 );

						while (Length--)
						{
							*NextBuf++ = *d++;
						}
						*NextBuf++ = *CodePtr[Code];
						NextCode++;

						OldCode = Code;
					}
					else
					{
						if (Code > NextCode)
						{
							break;
						}
						else
						{
							int Length = CodeLength[OldCode];
							CodeLength[NextCode] = Length + 1;
							CodePtr[NextCode] = NextBuf;

							BufSizeCheck( (NextBuf - CodeBuf) + Length + 1 );

							uchar *d = CodePtr[OldCode];
							while (Length--)
							{
								*NextBuf++ = *d++;
							}
							*NextBuf++ = *CodePtr[OldCode];
							OutputBytes(CodePtr[NextCode], CodeLength[NextCode]);
							NextCode++;

							int n = (int)NextBuf-(int)CodeBuf;
							Max = max(n, Max);

							OldCode = Code;
						}
					}

					switch (NextCode)
					{
						case 511:
						case 1023:
						case 2047:
						{
							CodeSize++;
							break;
						}
						case 4095:
						{
							InitializeTable();
							break;
						}
					}
				}
			}

			OutputBytes(0, 0);
			return true;
		}

		return false;
	}

	void Flush(bool Final = false)
	{
		if (Final OR (InPos >= InBuf + InBufSize))
		{
			int Len = (int)InPos - (int)InBuf;
			if (Pipe)
			{
				Pipe->Write(InBuf, Len);
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

	bool Compress(GStream *Out, GStream *In, Progress *Meter)
	{
		bool Status = false;
		short index, WaitingCode;
		int InputLen = In->GetSize();

		// Allocate encoder tables.
		uchar *packet = new uchar[256];
		short *hash_code = new short[MaxHashTable];
		short *hash_prefix = new short[MaxHashTable];
		uchar *hash_suffix = new uchar[MaxHashTable];
		uchar *Input = new uchar[InputLen];

		if (In AND
			Out AND
			Input AND
			packet AND
			hash_code AND
			hash_prefix AND
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
					if ((hash_prefix[k] == WaitingCode) AND
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
						
						if ((hash_prefix[k] == WaitingCode) AND
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
					hash_suffix[k] = index;
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
				if (Meter AND (v%1000 == 0))
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
	return d->Decompress(out, in, Meter);
}

bool Lzw::Compress(GStream *out, GStream *in)
{
	return d->Compress(out, in, Meter);
}



