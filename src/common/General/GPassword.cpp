#include "Lgi.h"
#include "GPassword.h"
#include "GVariant.h"

//////////////////////////////////////////////////////////////////////////////
// Password encryption for storing the passwords for accounts on the HD
//
// This isn't intended to be secure, but merely a obfusication of the 
// password to hide it from casual observers.
char HashString[] = "KLWEHGF)AS^*_GPYIOShakl;sjfhas0w89725l3jkhL:KHASFQ_DF_AS+_F";

GPassword::GPassword(GPassword *p)
{
	Data = 0;
	Len = 0;

	if (p)
	{
		*this = *p;
	}
}

GPassword::~GPassword()
{
	DeleteArray(Data);
}

GPassword &GPassword::operator =(GPassword &p)
{
	if (p.Data)
	{
		Data = new char[p.Len];
		if (Data)
		{
			memcpy(Data, p.Data, p.Len);
			Len = p.Len;
		}
	}
	
	return *this;
}

bool GPassword::operator ==(GPassword &p)
{
	if (Data AND p.Data AND Len == p.Len)
	{
		return memcmp(Data, p.Data, Len) == 0;
	}
	else if (Data == 0 AND p.Data == 0)
	{
		return true;
	}

	return false;
}

void GPassword::Process(char *Out, char *In, int Len)
{
	if (Out AND In AND Len > 0)
	{
		int HashLen = strlen(HashString);
		int i;

		for (i=0; i<Len; i++)
		{
			Out[i] = HashString[i % HashLen] ^ In[i];
		}
	}
}

void GPassword::Get(char *Buf)
{
	if (Buf)
	{
		Process(Buf, Data, Len);
		Buf[Len] = 0; // NULL terminate
	}
}

void GPassword::Set(char *Buf)
{
	DeleteArray(Data);
	Len = 0;

	if (Buf)
	{
		Len = strlen(Buf);
		Data = new char[Len];
		if (Data)
		{
			Process(Data, Buf, Len);
		}
	}
}

void GPassword::Serialize(char *Password, int Write)
{
	int *p = (int*)Password;

	if (Write)
	{
		if (Len > 28) Len = 28;

		*p++ = Len;
		if (Data)
		{
			memcpy(p, Data, Len);
		}
	}
	else // read
	{
		Len = *p++;
		DeleteArray(Data);
		if (Len > 0)
		{
			Data = new char[Len];
			if (Data)
			{
				memcpy(Data, p, Len);
			}
		}
	}
}

/*
bool GPassword::Serialize(ObjProperties *Options, char *Prop, int Write)
{
	bool Status = false;
	if (Options AND Prop)
	{
		if (Write)
		{
			Status = Options->Set(Prop, (void*)Data, Len);
		}
		else // read from prop list
		{
			void *d;
			int l;
			if (Status = Options->Get(Prop, d, l))
			{
				DeleteArray(Data);
				Len = l;
				Data = new char[Len];
				if (Data)
				{
					memcpy(Data, d, Len);
				}
			}
		}
	}
	return Status;
}
*/

bool GPassword::Serialize(GDom *Options, const char *Prop, int Write)
{
	bool Status = false;
	if (Options AND Prop)
	{
		GVariant v;
		if (Write)
		{
			v.SetBinary(Len, Data);
			Status = Options->SetValue(Prop, v);
		}
		else // read from prop list
		{
			if (Options->GetValue(Prop, v) AND
				v.Type == GV_BINARY)
			{
				DeleteArray(Data);
				Len = v.Value.Binary.Length;
				Data = new char[v.Value.Binary.Length];
				if (Data)
				{
					memcpy(Data, v.Value.Binary.Data, Len);
					Status = true;
				}
			}
		}
	}
	return Status;
}

void GPassword::Delete(GDom *Options, char *Option)
{
	if (Options && Option)
	{
		GVariant v;
		Options->SetValue(Option, v);
	}
}
