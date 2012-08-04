#ifndef __BER_H
#define __BER_H

#undef Bool

class BerPtr
{
	uchar *Ptr;
	int Len;

public:
	BerPtr(void *p, int l);

	int GetLen() { return Len; }
	uchar *Uchar();
	int *Int();
	void *Void();

	BerPtr &operator +=(int By);
};

class EncBer
{
	int Type;
	int Arg;
	EncBer *Parent;
	void _Int(int Type, int Int);

public:
	GBytePipe Buf;

	EncBer(EncBer *p = 0, int type = 0, int arg = 0);
	~EncBer();

	// Encoding
	EncBer *Sequence();
	EncBer *Set();
	EncBer *Application(int App);

	void Enum(int i) { _Int(10, i); }
	void Int(int i) { _Int(2, i); }
	void Str(char *s = 0);
};

typedef bool (*BerGetData)(void *This, uchar &c);

class DecBer
{
	// Length of data
	int Len;

	// Type of data...
	bool Constructed;

	// Remote data
	void *This;
	BerGetData GetFunc;

	// Local data
	uchar *Raw;

	// Methods
	bool HasData();
	bool Get(uchar &c);
	uchar *GetLen(int Len);
	int _Dec(int &Type, int &Arg, int &Size, bool &Construct);
	bool _Int(int Type, int &i);
	void _Skip(int Bytes);

	// Private constructor
	DecBer(uchar *raw, int len, bool constructed);

public:
	DecBer(BerGetData Func, void *This = 0, int Len = 0);
	~DecBer();

	// Encoding
	DecBer *Sequence();
	DecBer *Set();
	DecBer *Application(int &App);
	DecBer *Context(int &App);

	bool Bool(int &i);
	bool Int(int &i);
	bool Enum(int &i);
	bool Str(char *&s);
};

#endif
