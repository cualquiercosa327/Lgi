#include <stdlib.h>

#include "Lgi.h"
#include "GDb.h"

// Metakit
#include "mk4.h"

/////////////////////////////////////////////////////////////////////////////////////////
// Classes
class MkDb;
class MkTable;

class MkField : public GDbField
{
public:
	MkDb *Db;
	MkTable *Tbl;
	c4_Property *Fld;

	// Object
	MkField(MkTable *tbl, c4_Property *Fld);
	~MkField();

	// Properties
	char *Name();
	bool Name(char *Str);
	int Type();
	bool Type(int NewType);
	int Length();
	bool Length(int NewLength);
	char *Description();
	bool Description(char *NewDesc);

	// Set/Get
	bool Set(GVariant &v);
	bool Get(GVariant &v);
};

class MkTable : public GDbRecordset
{
public:
	MkDb *Db;
	c4_Property *Tbl;
	c4_View View;
	List<MkField> F;
	int Index;
	int Rows;

	// Object
	MkTable(MkDb *db, c4_Property *tbl);
	~MkTable();
	char *Name() { return (char*) Tbl->Name(); }

	// Fields
	GDbField *operator [](int Index);
	GDbField *operator [](char *Name);
	bool InsertField(GDbField *Fld, int Index = -1);
	bool DeleteField(GDbField *Fld);
	int Fields();

	// Markers
	bool Bof();
	bool Eof();

	// New/Edit
	bool AddNew();
	bool Edit();
	bool Update();
	void Cancel();

	// Moving around the records
	bool MoveFirst();
	bool MoveNext();
	bool MovePrev();
	bool MoveLast();
	int SeekRecord(int i); // from the beginning

	// Numbers/Counts
	int RecordIndex();
	int Records();
	
	// Deleting
	bool DeleteRecord(); // delete's the current record
};

class MkDb : public GDb
{
public:
	c4_Storage *File;
	List<MkTable> T;

	MkDb(char *s);
	~MkDb();

	bool Connect(char *Init);
	bool Disconnect();

	GDbRecordset *Open(char *Name);
	GDbRecordset *TableAt(int i);
};

/////////////////////////////////////////////////////////////////////////////////////////
// Field
MkField::MkField(MkTable *tbl, c4_Property *fld)
{
	Tbl = tbl;
	Db = tbl->Db;
	Fld = fld;
}

MkField::~MkField()
{
}

char *MkField::Name()
{
	return (char*) Fld->Name();
}

bool MkField::Name(char *Str)
{
	return false;
}

int MkField::Type()
{
	switch (Fld->Type())
	{
		case 'S':
		{	
			return GV_STRING;
			break;
		}
		case 'I':
		{
			return GV_INT32;
			break;
		}
	}

	return GV_NULL;
}

bool MkField::Type(int NewType)
{
	return false;
}

int MkField::Length()
{
	return 0;
}

bool MkField::Length(int NewLength)
{
	return false;
}

char *MkField::Description()
{
	return 0;
}

bool MkField::Description(char *NewDesc)
{
	return 0;
}

bool MkField::Set(GVariant &v)
{
	switch (Fld->Type())
	{
		case 'S':
		{	
			c4_StringProp f(Fld->Name());
			f(Tbl->View[Tbl->Index]) = v.Str();

			return true;
			break;
		}
		case 'I':
		{
			c4_IntProp f(Fld->Name());
			f(Tbl->View[Tbl->Index]) = atoi(v.Str());

			return true;
			break;
		}
	}

	return false;
}

bool MkField::Get(GVariant &v)
{
	switch (Fld->Type())
	{
		case 'S':
		{
			c4_StringProp s(Fld->Name());
			v = (char*)s.Get(Tbl->View[Tbl->Index]);
			break;
		}
		case 'I':
		{
			c4_IntProp i(Fld->Name());
			v = i.Get(Tbl->View[Tbl->Index]);
			break;
		}
		default:
		{
			return false;
		}
	}
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Table
MkTable::MkTable(MkDb *db, c4_Property *tbl)
{
	Db = db;
	Tbl = tbl;
	Index = -1;
	View = Db->File->View(Tbl->Name());
	Rows = View.GetSize();

	for (int n=0; n<View.NumProperties(); n++)
	{
		F.Insert(new MkField(this, (c4_Property*) &View.NthProperty(n)));
	}
}

MkTable::~MkTable()
{
	F.DeleteObjects();
}

GDbField *MkTable::operator [](int Index)
{
	return F[Index];
}

GDbField *MkTable::operator [](char *Name)
{
	if (Name)
	{
		for (MkField *f=F.First(); f; f=F.Next())
		{
			if (stricmp(Name, f->Fld->Name()) == 0)
			{
				return f;
			}
		}
	}

	return 0;
}

bool MkTable::InsertField(GDbField *Fld, int Index)
{
	return 0;
}

bool MkTable::DeleteField(GDbField *Fld)
{
	return 0;
}

int MkTable::Fields()
{
	return F.GetItems();
}

bool MkTable::Bof()
{
	return Index < 0;
}

bool MkTable::Eof()
{
	return Index > Rows;
}

bool MkTable::AddNew()
{
	return 0;
}

bool MkTable::Edit()
{
	return 0;
}

bool MkTable::Update()
{
	return 0;
}

void MkTable::Cancel()
{
}

bool MkTable::MoveFirst()
{
	Index = 0;
	return Index >= 0 AND Index < Rows;
}

bool MkTable::MoveNext()
{
	Index++;
	return Index >= 0 AND Index < Rows;
}

bool MkTable::MovePrev()
{
	Index--;
	return Index >= 0 AND Index < Rows;
}

bool MkTable::MoveLast()
{
	Index = Rows - 1;
	return Index >= 0 AND Index < Rows;
}

int MkTable::SeekRecord(int i)
{
	Index = i;;
	if (Index >= 0 AND Index < Rows)
	{
		return Index;
	}
	return -1;
}

int MkTable::RecordIndex()
{
	return Index;
}

int MkTable::Records()
{
	return Rows;
}

bool MkTable::DeleteRecord()
{
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Database
MkDb::MkDb(char *s)
{
	File = 0;
	if (s) Connect(s);
}

MkDb::~MkDb()
{
	Disconnect();
}

bool MkDb::Connect(char *Init)
{
	File = new c4_Storage(Init, true);
	if (File)
	{
		for (int n=0; n<File->NumProperties(); n++)
		{
 			T.Insert(new MkTable(this, (c4_Property *) &File->NthProperty(n)));
		}
	}

	return File != 0;
}

bool MkDb::Disconnect()
{
	T.DeleteObjects();
	DeleteObj(File);
	return true;
}

GDbRecordset *MkDb::Open(char *Name)
{
	if (Name)
	{
		for (MkTable *t=T.First(); t; t=T.Next())
		{
			if (stricmp(Name, t->Tbl->Name()) == 0)
			{
				return t;
			}
		}
	}

	return 0;
}

GDbRecordset *MkDb::TableAt(int i)
{
	return T[i];
}

//////////////////////////////////////////////////////////////////
GDb *OpenMetakitDatabase(char *s)
{
	return new MkDb(s);
}

