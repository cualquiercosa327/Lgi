#include "Lgi.h"
#include "GDb.h"
#include "mysql.h"
#include "INet.h"

GDbField Null;

class MysqlField : public GDbField
{
	class MysqlRs *Rs;
	MYSQL_FIELD *f;
	int Idx;

public:
	MysqlField(MysqlRs *rs, MYSQL_FIELD *fld, int idx);

	operator char*();

	char *Name();
	bool Name(char *Str);
	int Type();
	bool Type(int NewType);
	int Length();
	bool Length(int NewLength);
	char *Description();
	bool Description(char *NewDesc);
	bool Set(GVariant &v);
	bool Get(GVariant &v);
};

class MysqlRs : public GDbRecordset
{
public:
	char *Sql;
	MYSQL_RES *Res;
	MYSQL_ROW Row;
	GHashTable Idx;
	GArray<MysqlField*> Field;
	int Rows;
	int Cursor;

	MysqlRs(char *s, MYSQL_RES *r);
	~MysqlRs();

	char *Name();

	GDbField &operator [](int Index);
	GDbField &operator [](char *Name);	
	GDbField *InsertField(	char *Name,
							int Type,
							int Length = 0,
							int Index = -1);
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
};

MysqlField::MysqlField(MysqlRs *rs, MYSQL_FIELD *fld, int idx)
{
	Rs = rs;
	f = fld;
	Idx = idx;
}

MysqlField::operator char*()
{
	if (!Rs->End())
	{
		return Rs->Row[Idx];
	}
	return 0;
}

char *MysqlField::Name()
{
	return f->name;
}

bool MysqlField::Name(char *Str)
{
	return false;
}

int MysqlField::Type()
{
	return GV_STRING;
}

bool MysqlField::Type(int NewType)
{
	return false;
}

int MysqlField::Length()
{
	return f->length;
}

bool MysqlField::Length(int NewLength)
{
	return false;
}

char *MysqlField::Description()
{
	return 0;
}

bool MysqlField::Description(char *NewDesc)
{
	return false;
}

bool MysqlField::Set(GVariant &v)
{
	return false;
}

bool MysqlField::Get(GVariant &v)
{
	if (!Rs->End())
	{
		v = Rs->Row[Idx];
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////
MysqlRs::MysqlRs(char *s, MYSQL_RES *r)
{
	Sql = NewStr(s);
	Res = r;
	Rows = mysql_num_rows(r);
	Cursor = -1;

	MYSQL_FIELD *f;
	int n = 0;
	while (f = mysql_fetch_field(Res))
	{
		MysqlField *fld = new MysqlField(this, f, n);
		if (fld)
		{
			Field.Add(fld);
			Idx.Add(f->name, (void*) ++n);
		}
	}
}

MysqlRs::~MysqlRs()
{
	DeleteArray(Sql);
}

char *MysqlRs::Name()
{
	return Sql;
}

GDbField &MysqlRs::operator [](int Index)
{
	if (Index >= 0 AND Index < Field.Length())
		return *Field[Index];

	return Null;
}

GDbField &MysqlRs::operator [](char *Name)
{
	int n = (int)Idx.Find(Name);
	if (n > 0 AND n <= Field.Length())
		return *Field[n - 1];

	return Null;
}

GDbField *MysqlRs::InsertField(	char *Name,
						int Type,
						int Length,
						int Index)
{
	return 0;
}

bool MysqlRs::DeleteField(GDbField *Fld)
{
	return 0;
}

int MysqlRs::Fields()
{
	return Field.Length();
}

bool MysqlRs::End()
{
	return Cursor < 0 || Cursor >= Rows;
}

bool MysqlRs::AddNew()
{
	return 0;
}

bool MysqlRs::Edit()
{
	return 0;
}

bool MysqlRs::Update()
{
	return 0;
}

void MysqlRs::Cancel()
{
}

bool MysqlRs::MoveFirst()
{
	return SeekRecord(0);
}

bool MysqlRs::MoveNext()
{
	return SeekRecord(Cursor + 1);
}

bool MysqlRs::MovePrev()
{
	return SeekRecord(Cursor - 1);
}

bool MysqlRs::MoveLast()
{
	return SeekRecord(Rows - 1);
}

int MysqlRs::SeekRecord(int i)
{
	Cursor = i;

	if (!End())
	{
		mysql_data_seek(Res, Cursor);
		Row = mysql_fetch_row(Res);
		return true;
	}

	return false;
}

int MysqlRs::RecordIndex()
{
	return Cursor;
}

int MysqlRs::Records()
{
	return Rows;
}

bool MysqlRs::DeleteRecord()
{
	return 0;
}

//////////////////////////////////////////////////////////////////////
class MysqlDb : public GDb
{
	MYSQL Db;

public:
	bool Connected;

	MysqlDb(char *s)
	{
		mysql_init(&Db);
		if (s)
			Connect(s);
	}

	~MysqlDb()
	{
	}

	bool GetVariant(const char *n, GVariant &val, char *arr)
	{
		if (!stricmp(n, Dom_IsConnected))
		{
			val = Connected;
		}
		else return false;

		return true;
	}

	bool Connect(char *Init)
	{
		if (!Init)
			return false;

		GUri u(Init);

		if (u.Path)
		{
			Connected = mysql_real_connect
			(
				&Db,
				u.Host,
				u.User,
				u.Pass,
				u.Path + 1,
				u.Port ? u.Port : 3306,
				0, // unix_socket,
				0 // client_flag
			) != 0;
		}

		return Connected;
	}

	bool Disconnect()
	{
		if (Connected)
		{
			mysql_close(&Db);
			Connected = false;
		}

		return true;
	}

	GDbRecordset *Open(char *Name)
	{
		GDbRecordset *Rs = 0;
		
		if (Connected)
		{
			if (!mysql_query(&Db, Name))
			{
				MYSQL_RES *Res = mysql_store_result(&Db);
				if (Res)
				{
					Rs = new MysqlRs(Name, Res);
				}
			}
		}

		return Rs;
	}

	bool Execute(char *Name)
	{
		bool Status = false;

		if (Connected AND Name)
		{
			Status = !mysql_query(&Db, Name);
		}

		return Status;
	}

	const char *GetErrorMsg()
	{
		return mysql_error(&Db);
	}

	GDbRecordset *TableAt(int i)
	{
		return 0;
	}
};

GDb *OpenMysqlDatabase(char *s)
{
	static bool Init = false;
	if (!Init)
	{
		my_init();
		Init = true;
	}

	return new MysqlDb(s);
}

