/*hdr
**      FILE:           GDb-Dbf.h
**      AUTHOR:         Matthew Allen
**      DATE:           8/2/2000
**      DESCRIPTION:    Separated variables implementation of the GDb API
**
**      Copyright (C) 2003 Matthew Allen
**			fret@memecode.com
*/

#include <stdio.h>

#include "Lgi.h"
#include "GDb.h"
#include "GToken.h"

///////////////////////////////////////////////////////////////////
// Forward decl
class SvField;
class SvRecord;
class SvDb;

char *LgiTsvTok(char *&s)
{
	static const char *Ws = " \r\n";
	while (*s && strchr(Ws, *s)) s++;
	if (*s == '\'' || *s == '\"')
	{
		char delim = *s++;
		char *start = s;
		char *end = strchr(s, delim);
		if (!end)
			return NULL;
		s = end + 1;
		if (*s == '\t') s++;
		return NewStr(start, end-start);
	}

	char *start = s;
	while (*s && *s != '\t') s++;
	char *end = s;
	if (start == end)
		return NULL;

	while (end > start && strchr(Ws, end[-1])) end--;
	if (*s == '\t') s++;
	return NewStr(start, end-start);
}

class SvRecordset : public GDbRecordset
{
	friend class SvField;
	
	SvDb *Parent;

	char *FileName;
	List<SvField> F;
	List<SvRecord> R;
	List<SvRecord>::I *Cur;
	SvRecord *Temp;
	SvRecord *New;
	bool Dirty;

	void Empty();
	void Read();
	void Write();
	SvRecord *Record();

public:
	SvRecordset(SvDb *parent, const char *file);
	~SvRecordset();

	char *Name();
	GDbField &operator [](int Index);
	GDbField &operator [](const char *Name);
	GDbField *InsertField(const char *Name, int Type, int Len, int Index);
	bool DeleteField(GDbField *Fld);
	int Fields();
	bool End();
	bool AddNew();
	bool Edit();
	bool Update();
	void Cancel();
	bool MoveFirst();
	bool MoveNext();
	bool MovePrev();
	bool MoveLast();
	int SeekRecord(int i);
	int RecordIndex();
	int Records();
	bool DeleteRecord();
	char GetSep();
};

class SvDb : public GDb
{
	friend class SvRecordset;

	char Separator;
	List<SvRecordset> Tables;

public:
	SvDb(char sep)
	{
		Separator = sep;
	}

	~SvDb();
	bool Connect(const char *Init);
	bool Disconnect();
	GDbRecordset *Open(char *Name);
	GDbRecordset *TableAt(int i);
	bool Tsv() { return Separator == '\t'; }
	char GetSep() { return Separator; }
};

class SvRecord
{
	int RawLen;
	char *Raw;

public:
	SvRecordset *Rs;
	int Fields;
	char **Data;

	void EmptyField(int i)
	{
		if (Data &&
			i < Fields)
		{
			char *End = Raw + RawLen;
			if (!Raw || (Data[i] < Raw || Data[i] > End))
			{
				DeleteArray(Data[i]);
			}
		}
	}

	void Empty()
	{
		if (Data)
		{
			char *End = Raw + RawLen;
			for (int i=0; i<Fields; i++)
			{
				if (!Raw || (Data[i] < Raw || Data[i] > End))
				{
					DeleteArray(Data[i]);
				}
			}
		}

		DeleteArray(Raw);
	}

	bool SetFields(int i)
	{
		char **b = new char*[i];
		if (b)
		{
			memset(b, 0, sizeof(char*)*i);
			if (Data && Fields > 0)
			{
				int m = min(Fields, i);
				memcpy(b, Data, sizeof(char*)*m);
			}
			DeleteArray(Data);
			Data = b;
			Fields = i;

			return true;
		}

		return false;
	}

	SvRecord(SvRecordset *rs)
	{
		Rs = rs;
		RawLen = 0;
		Raw = 0;
		Fields = 0;
		Data = 0;
		SetFields(Rs->Fields());
	}

	SvRecord(SvRecordset *rs, SvRecord *r)
	{
		Rs = rs;
		RawLen = 0;
		Raw = 0;
		Fields = 0;
		Data = 0;
		if (r) *this = *r;
	}

	SvRecord(SvRecordset *rs, char *&In)
	{
		Rs = rs;
		RawLen = 0;
		Raw = 0;
		Fields = 0;
		Data = 0;

		int Flds = 1;
		char *Start = In;
		char Sep = Rs->GetSep();
		for (RawLen=0; In[RawLen]; )
		{
			if (In[RawLen] == Sep)
			{
				Flds++;
				RawLen++;
			}
			else if (In[RawLen] == '\"')
			{
				RawLen++;
				while (In[RawLen] && In[RawLen] != '\"') RawLen++;
				RawLen++;
			}
			else if (In[RawLen] == '\n' || In[RawLen] == '\r')
			{
				break;
			}
			else
			{
				RawLen++;
			}
		}

		if (RawLen > 0)
		{
			In += RawLen;
			while (*In == '\r' || *In == '\n') In++;
			Raw = NewStr(Start, RawLen);

			if (Raw && SetFields(Flds))
			{
				char *r = Raw;
				for (int n=0; n<Fields && r; n++)
				{
					while (*r == ' ') r++;

					char *e = r;
					if (*r == '\"')
					{
						e = ++r;
						while (*e && *e != '\"') e++;
						if (*e) *e++ = 0;
					}
					else
					{
						while (*e && *e != '\n' && *e != Sep) e++;
					}
					Data[n] = r;
					if (*e)
					{
						r = e + 1;
						*e = 0;

					}
					else
					{
						break;
					}
				}
			}
			else In = 0;
		}
	}

	~SvRecord()
	{
		DeleteArray(Data);
	}

	SvRecord &operator =(SvRecord &r)
	{
		Empty();
		SetFields(r.Fields);
		for (int i=0; i<Fields; i++)
		{
			Data[i] = NewStr(r.Data[i]);
		}

		return *this;
	}
};

class SvField : public GDbField
{
	int Index;
	GAutoString FldName;
	SvRecordset *Rs;

public:
	SvField(SvRecordset *rs, int i, GAutoString n)
	{
		Index = i;
		Rs = rs;
		FldName = n;
	}

	char *Name()
	{
		return FldName;
	}

	bool Name(char *Str)
	{
		return FldName.Reset(NewStr(Str));
	}

	int Type()
	{
		return GV_STRING;
	}

	bool Type(int NewType)
	{
		return false;
	}

	int Length()
	{
		return 255;
	}

	bool Length(int NewLength)
	{
		return false;
	}

	char *Description()
	{
		return 0;
	}

	bool Description(char *NewDesc)
	{
		return false;
	}

	bool Set(GVariant &v)
	{
		SvRecord *r = Rs->Record();
		if (r && v.Str())
		{
			if (Index < r->Fields ||
				r->SetFields(Index + 1))
			{
				r->EmptyField(Index);
				r->Data[Index] = NewStr(v.Str());
				return true;
			}
		}

		return false;
	}

	GDbField &operator =(char *ns)
	{
		SvRecord *r = Rs->Record();
		if (r)
		{
			if (Index < r->Fields)
			{
				r->EmptyField(Index);
				r->Data[Index] = NewStr(ns);
			}
		}
		
		return *this;		
	}

	operator char*()
	{
		SvRecord *r = Rs->Record();
		if (r)
		{
			if (Index < r->Fields)
			{
				return r->Data[Index];
			}
		}
		
		return 0;
	}

	bool Get(GVariant &v)
	{
		SvRecord *r = Rs->Record();
		if (r)
		{
			if (Index < r->Fields)
			{
				v = r->Data[Index];
				return true;
			}
		}
		
		return false;
	}
};

/////////////////////////////////////////////////////////////////////
SvRecordset::SvRecordset(SvDb *parent, const char *file)
{
	Parent = parent;
	Cur = 0;
	Temp = 0;
	New = 0;
	Dirty = false;
	FileName = NewStr(file);
	Read();
}

SvRecordset::~SvRecordset()
{
	if (Dirty)
	{
		Write();
	}
	if (Parent)
	{
		Parent->Tables.Delete(this);
	}

	DeleteArray(FileName);
}

char *SvRecordset::Name()
{
	char *d = strrchr(FileName, DIR_CHAR);
	return d ? d + 1 : 0;
}

void SvRecordset::Empty()
{
	F.DeleteObjects();
	R.DeleteObjects();
}

enum EncodingType
{
	Unknown,
	Utf8,
	Utf16BE,
	Utf16LE,
	Utf32BE,
	Utf32LE,
};

void SvRecordset::Read()
{
	Empty();

	GFile f;
	if (f.Open(FileName, O_READ|O_SHARE))
	{
		GStringPipe p(1024);
		char Buf[1024];
		int r;
		bool First = true;
		EncodingType Type = Unknown;

		while ((r = f.Read(Buf, sizeof(Buf))) > 0)
		{
			uchar *Start = (uchar*)Buf;
			if (First)
			{
				if (Start[0] == 0xEF &&
					Start[1] == 0xBB &&
					Start[2] == 0xBF)
				{
					Type = Utf8;
					Start += 3;
					r -= 3;
				}
				else if (Start[0] == 0xFE &&
						 Start[1] == 0xFF)
				{
					Type = Utf16BE;
					Start += 2;
					r -= 2;
				}
				else if (Start[0] == 0xFF &&
						 Start[1] == 0xFE)
				{
					Type = Utf16LE;
					Start += 2;
					r -= 2;
				}
				else if (Start[0] == 0x00 &&
						 Start[1] == 0x00 &&
						 Start[2] == 0xFE &&
						 Start[3] == 0xFF)
				{
					Type = Utf32BE;
					Start += 4;
					r -= 4;
				}
				else if (Start[0] == 0xFF &&
						 Start[1] == 0xFE &&
						 Start[2] == 0x00 &&
						 Start[3] == 0x00)
				{
					Type = Utf32LE;
					Start += 4;
					r -= 4;
				}
			}

			switch (Type)
			{
				case Utf16LE:
				{
					GAutoString Utf((char*)LgiNewConvertCp("utf-8", Start, "utf-16", r));
					if (Utf)
					{
						p.Write(Utf, strlen(Utf));
					}
					break;
				}
				case Unknown:
				case Utf8:
				{
					p.Write(Start, r);
					break;
				}
				default:
				{
					LgiAssert(!"Not implemented.");
					break;
				}
			}

			while (p.Pop(Buf, sizeof(Buf)))
			{
				if (First)
				{
					// Headers...
					if (Parent->Tsv())
					{
						GAutoString t;
						char *s = Buf;
						int n = 0;
						while (t.Reset(LgiTsvTok(s)))
						{
							F.Insert(new SvField(this, n++, t));
						}
					}
					else
					{
						GToken Flds(Buf, ",");
						for (int n=0; n<Flds.Length(); n++)
						{
							GAutoString Name(TrimStr(Flds[n], " \r\t\n\""));
							if (Name)
							{
								F.Insert(new SvField(this, n, Name));
							}
						}
					}

					First = false;
				}
				else
				{
					char *t = Buf;
					R.Insert(new SvRecord(this, t));
					if (!t)
					{
						break;
					}
				}
			}
		}
		while (!f.Eof());
	}
}

void SvRecordset::Write()
{
	GFile f;
	if (FileName && f.Open(FileName, O_WRITE))
	{
		char s[256];
		int NewLine = strlen(EOL_SEQUENCE);
		f.SetSize(0);

		// Headers
		for (SvField *Fld = F.First(); Fld; )
		{
			SvField *Nxt = F.Next();
			char *Name = Fld->Name();
			f.Write(s, sprintf(s, "\"%s\"", Name));
			if (Nxt) f.Write((void*)",", 1);
			Fld = Nxt;
		}
		f.Write((void*)EOL_SEQUENCE, NewLine);
	
		// Records
		for (bool b = MoveFirst(); b; b = MoveNext())
		{
			GVariant v;
			for (int i=0; i<F.Length(); i++)
			{
				if (i)
				{
					f.Write((void*)",", 1);
				}

				if ((*this)[i].Get(v))
				{
					switch (v.Type)
					{
						case GV_STRING:
						{
							f.Write((char*)"\"", 1);
							for (char *c = v.Str(); *c; c++)
							{
								if (*c == '\"')
								{
									f.Write((char*)"\"\"\"", 3);
								}
								else f.Write(c, 1);
							}
							f.Write((char*)"\"", 1);
							break;
						}
					}
				}
			}
			f.Write((void*)EOL_SEQUENCE, NewLine);
		}
	}
}

SvRecord *SvRecordset::Record()
{
	if (Temp) return Temp;
	if (New) return New;
	if (Cur) return **Cur;
	return 0;
}

static GDbField Null;

GDbField &SvRecordset::operator [](int Index)
{
	if (Index >= 0 && Index < F.Length())
		return *F[Index];

	return Null;
}

GDbField &SvRecordset::operator [](const char *Name)
{
	if (Name)
	{
		for (GDbField *f=F.First(); f; f=F.Next())
		{
			if (f->Name() && stricmp(Name, f->Name()) == 0)
			{
				return *f;
			}
		}
	}
	return Null;
}

GDbField *SvRecordset::InsertField(const char *Name, int Type, int Length, int Index)
{
	if (Index < 0)
	{
		Index = F.Length();
	}

    GAutoString n(NewStr(Name));
	SvField *f = new SvField(this, Index, n);
	if (f)
	{
		F.Insert(f, Index);
	}
	return f;
}

bool SvRecordset::DeleteField(GDbField *Fld)
{
	return 0;
}

int SvRecordset::Fields()
{
	return F.Length();
}

bool SvRecordset::End()
{
	return !Cur || !Cur->Current();
}

bool SvRecordset::AddNew()
{
	Update();
	New = new SvRecord(this);
	return New != 0;
}

bool SvRecordset::Edit()
{
	Update();
	if (Cur && **Cur)
	{
		Temp = new SvRecord(this, **Cur);
	}
	return Temp != 0;
}

bool SvRecordset::Update()
{
	if (Temp)
	{
		SvRecord *c = Cur ? **Cur : 0;
		if (c)
		{
			*c = *Temp;
			DeleteObj(Temp);
			Dirty = true;
			return true;
		}
	}
	else if (New)
	{
		R.Insert(New);
		New = 0;
		Dirty = true;
		return true;
	}

	return false;
}

void SvRecordset::Cancel()
{
	DeleteObj(Temp);
	DeleteObj(New);
}

bool SvRecordset::MoveFirst()
{
	Cancel();

	if (!Cur)
	{
		Cur = new List<SvRecord>::I(R.Start());
	}

	if (Cur)
	{
		return Cur->First();
	}

	return 0;
}

bool SvRecordset::MoveNext()
{
	Cancel();

	return Cur ? Cur->Next() : 0;
}

bool SvRecordset::MovePrev()
{
	Cancel();

	return Cur ? Cur->Prev() : 0;
}

bool SvRecordset::MoveLast()
{
	Cancel();

	if (!Cur)
	{
		Cur = new List<SvRecord>::I(R.Start());
	}

	if (Cur)
	{
		return Cur->Last();
	}

	return 0;
}

int SvRecordset::SeekRecord(int i)
{
	Cancel();

	if (!Cur)
	{
		Cur = new List<SvRecord>::I(R.Start());
	}

	if (Cur && (*Cur)[i])
	{
		return true;
	}
	
	return false;
}

int SvRecordset::RecordIndex()
{
	return Cur ? R.IndexOf(**Cur) : -1;
}

int SvRecordset::Records()
{
	return R.Length();
}

bool SvRecordset::DeleteRecord()
{
	return 0;
}

char SvRecordset::GetSep()
{
	return Parent->GetSep();
}

//////////////////////////////////////////////////////////////////////////
SvDb::~SvDb()
{
	Disconnect();
}

bool SvDb::Connect(const char *Init)
{
	Tables.Insert(new SvRecordset(this, Init));
	return Tables.First() != 0;
}

bool SvDb::Disconnect()
{
	Tables.DeleteObjects();
	return true;
}

GDbRecordset *SvDb::Open(char *Name)
{
	SvRecordset *rs = new SvRecordset(this, Name);
	if (rs)
	{
		Tables.Insert(rs);
	}
	return rs;
}

GDbRecordset *SvDb::TableAt(int i)
{
	return Tables[i];
}

///////////////////////////////////////////////////////////////////
GDb *OpenCsvDatabase(char *Path)
{
	SvDb *Db = new SvDb(',');
	if (Db && Db->Connect(Path))
	{
		return Db;
	}
	DeleteObj(Db);
	return 0;
}

GDb *OpenTsvDatabase(char *Path)
{
	SvDb *Db = new SvDb('\t');
	if (Db && Db->Connect(Path))
	{
		return Db;
	}
	DeleteObj(Db);
	return 0;
}


