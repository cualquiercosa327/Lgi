#include "Lgi.h"
#include "Ber.h"

#define BER_CONSTRUCTED		0x20

//////////////////////////////////////////////////////////////////////////////
BerPtr::BerPtr(void *p, int l)
{
	Ptr = (uchar*)p;
	Len = l;
}

uchar *BerPtr::Uchar()
{
	return Ptr;
}

int *BerPtr::Int()
{
	return (int*)Ptr;
}

void *BerPtr::Void()
{
	return Ptr;
}

BerPtr &BerPtr::operator +=(int By)
{
	Ptr += By;
	Len -= By;
	if (Len <= 0)
	{
		Ptr = 0;
		Len = 0;
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////////
EncBer::EncBer(EncBer *p, int type, int arg)
{
	Type = type & 0x3;
	Arg = arg & 0x3f;
	Parent = p;
}

EncBer::~EncBer()
{
	if (Parent)
	{
		// Write sequence to parent
		uchar Ber[4];
		Ber[0] = (Type << 6) | Arg;
		
		int Bytes = 2;
		int Size = Buf.GetSize();
		if (Size > 127)
		{
			Ber[1] = 0x82;
			Ber[2] = (Size>>8)&0xff;
			Ber[3] = Size&0xff;
			Bytes = 4;
		}
		else
		{
			Ber[1] = Size;
		}

		uchar *d = new uchar[Size];
		if (d)
		{
			Buf.Read(d, Size);
			Parent->Buf.Write(Ber, Bytes);
			Parent->Buf.Write(d, Size);
			DeleteArray(d);
		}
	}
}

EncBer *EncBer::Sequence()
{
	return new EncBer(this, 0, BER_CONSTRUCTED | 16);
}

EncBer *EncBer::Set()
{
	return new EncBer(this, 0, BER_CONSTRUCTED | 17);
}

EncBer *EncBer::Application(int App)
{
	return new EncBer(this, 1, BER_CONSTRUCTED | App);
}

void EncBer::_Int(int Type, int Int)
{
	uchar Ber[2] = { Type, 1 };
	uchar *Ip = (uchar*)&Int;
	
	for (Ber[1]=sizeof(int)-1; Ber[1]>=2; Ber[1]--)
	{
		if (Ip[Ber[1]]) break;
	}

	Buf.Write(Ber, 2);
	Buf.Write((uchar*) &Int, Ber[1]);
}

void EncBer::Str(char *s)
{
	int Len = s ? strlen(s) : 0;
	if (Len < 0x80)
	{
		uchar Ber[2] = { 4, Len };
		Buf.Write(Ber, 2);
	}
	else
	{
		int Bytes =	((Len & 0x000000ff) ? 1 : 0) +
					((Len & 0x0000ff00) ? 1 : 0) + 
					((Len & 0x00ff0000) ? 1 : 0) + 
					((Len & 0xff000000) ? 1 : 0);
		uchar Ber[6] =
		{
			4,
			0x80 + Bytes,
			Len & 0xff,
			(Len >> 8) & 0xff,
			(Len >> 16) & 0xff,
			(Len >> 24) & 0xff
		};

		Buf.Write(Ber, 2 + Bytes);
	}

	if (s) Buf.Write((uchar*)s, Len);
}

//////////////////////////////////////////////////////////////////////////////
DecBer::DecBer(uchar *raw, int len, bool constructed)
{
	Constructed = constructed;
	Raw = raw;
	Len = len;
	This = 0;
	GetFunc = 0;
}

DecBer::DecBer(BerGetData Func, int Data, int len)
{
	Constructed = true;
	Raw = 0;
	Len = len;
	This = Data;
	GetFunc = Func;
}

DecBer::~DecBer()
{
	DeleteArray(Raw);
}

bool DecBer::HasData()
{
	return GetFunc || Raw;
}

bool DecBer::Get(uchar &c)
{
	if (GetFunc)
	{
		if (GetFunc(This, c))
		{
			Len = max(Len - 1, 0);
			return true;
		}
	}
	else
	{
		if (Len > 0)
		{
			c = Raw[0];
			memmove(Raw, Raw+1, Len-1);
			Len--;
			return true;
		}
	}

	return false;
}

void DecBer::_Skip(int Bytes)
{
	uchar c;
	while (Bytes-- > 0)
	{
		Get(c);
	}
}

int DecBer::_Dec(int &Type, int &Arg, int &Size, bool &Construt)
{
	int Bytes = 0;

	if (HasData())
	{
		uchar Uc[2];
		if (Get(Uc[0]) AND
			Get(Uc[1]))
		{
			Type = Uc[0] >> 6;
			Arg = Uc[0] & 0x1f;
			Construt = (Uc[0] & BER_CONSTRUCTED) != 0;
			if (Uc[1] & 0x80)
			{
				Bytes = Uc[1] & 0x7f;

				Size = 0;
				uchar *p = ((uchar*)&Size) + (Bytes - 1);
				for (int n=0; n<Bytes; n++)
				{
					uchar c;
					if (Get(c))
					{
						*p-- = c; // Ptr->Uchar()[1+Bytes-n];
					}
				}
				
				Bytes += 2;
			}
			else
			{
				Size = Uc[1];
				Bytes = 2;
			}
		}
	}

	return Bytes;
}

DecBer *DecBer::Sequence()
{
	DecBer *Ber = 0;
	int Type, Arg, Size;
	bool Construt;
	int Bytes = _Dec(Type, Arg, Size, Construt);

	if (Bytes)
	{
		if (Type == 0 AND
			Arg == 16)
		{
			uchar *Buf = GetLen(Size);
			if (Buf)
			{
				Ber = new DecBer(Buf, Size, Construt);
			}
		}
		else
		{
			_Skip(Size);
		}
	}

	return Ber;
}

DecBer *DecBer::Set()
{
	DecBer *Ber = 0;
	int Type, Arg, Size;
	bool Construt;
	int Bytes = _Dec(Type, Arg, Size, Construt);

	if (Bytes)
	{
		if (Type == 0 AND
			Arg == 17)
		{
			uchar *Buf = GetLen(Size);
			if (Buf)
			{
				Ber = new DecBer(Buf, Size, Construt);
			}
		}
	}

	return Ber;
}

uchar *DecBer::GetLen(int Len)
{
	uchar *Buf = new uchar[Len];
	uchar c;

	for (int i=0; i<Len; i++)
	{
		if (!Get(Buf ? Buf[i] : c))
		{
			DeleteArray(Buf);
			break;
		}
	}

	return Buf;
}

DecBer *DecBer::Application(int &App)
{
	DecBer *Ber = 0;

	int Type, Size;
	bool Construt;
	int Bytes = _Dec(Type, App, Size, Construt);

	if (Bytes)
	{
		if (Type == 1)
		{
			uchar *Buf = GetLen(Size);
			if (Buf)
			{
				Ber = new DecBer(Buf, Size, Construt );
			}
		}
	}

	return Ber;
}

DecBer *DecBer::Context(int &App)
{
	DecBer *Ber = 0;

	int Type, Size;
	bool Construt;
	int Bytes = _Dec(Type, App, Size, Construt);

	if (Bytes)
	{
		if (Type == 2)
		{
			uchar *Buf = GetLen(Size);
			if (Buf)
			{
				Ber = new DecBer(Buf, Size, Construt);
			}
		}
	}

	return Ber;
}

bool DecBer::_Int(int IntType, int &i)
{
	i = 0;

	int Type, Arg, Size;
	bool Construt;
	int Bytes = _Dec(Type, Arg, Size, Construt);

	if (Bytes AND
		Type == 0 AND
		Arg == IntType)
	{
		uchar *p = ((uchar*)&i) + (Size - 1);
		for (int n=0; n<Size; n++)
		{
			uchar c;
			if (Get(c))
			{
				*p-- = c; // Ptr->Uchar()[Size-1-n];
			}
		}

		return true;
	}

	return false;
}

bool DecBer::Bool(int &i)
{
	return _Int(1, i);
}

bool DecBer::Int(int &i)
{
	return _Int(2, i);
}

bool DecBer::Enum(int &i)
{
	return _Int(10, i);
}

bool DecBer::Str(char *&s)
{
	s = 0;

	if (Constructed)
	{
		int Type, Arg, Size;
		bool Construt;
		int Bytes = _Dec(Type, Arg, Size, Construt);

		if (Bytes AND
			Type == 0 AND
			Arg == 4)
		{
			if (Size > 0)
			{
				s = new char[Size+1];
				if (s)
				{
					for (int i=0; i<Size; i++)
					{
						if (!Get((uchar&)s[i]))
						{
							DeleteArray(s);
							break;
						}
					}
					if (s)
					{
						s[Size] = 0;
					}
				}
			}

			return Size == 0 || s != 0;
		}
	}
	else
	{
		s = NewStr((char*)Raw, Len);
		return s != 0;
	}

	return false;
}


