/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A password manager

#ifndef __GPASSWORD_H
#define __GPASSWORD_H

class LgiClass GPassword
{
	char *Data; // this is binary data... may contain NULL's
	int Len;

	void Process(char *Out, char *In, int Len);

public:
	GPassword(GPassword *p = 0);
	virtual ~GPassword();

	bool IsValid() { return Data && Len > 0; }
	void Get(char *Buf);
	void Set(char *Buf);
	
	// bool Serialize(ObjProperties *Options, char *Option, int Write);
	bool Serialize(GDom *Options, const char *Option, int Write);
	void Serialize(char *Password, int Write);
	
	void Delete(GDom *Options, char *Option);

	GPassword &operator =(GPassword &p);
	bool operator ==(GPassword &p);
};

#endif
