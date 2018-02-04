#include "Lgi.h"
#include "LDbTable.h"

inline bool IsFixed(GVariantType t)
{
	return	t != GV_STRING &&
			t != GV_BINARY;
}

struct Info
{
	GVariantType Type;
	int Offset;

	Info(GVariantType t = GV_NULL, int off = -1)
	{
		Type = t;
		Offset = off;
	}

	bool operator !=(const Info &i)
	{
		return Type != i.Type &&
				Offset != i.Offset;
	}
};

struct DbTablePriv
{
	// Fields
	int Fixed;
	int FixedSz;
	int Variable;
	GArray<LDbField> Fields;
	GHashTbl<int, Info> Map;

	// Rows
	int Rows;
	LDbRow *First, *Last;
	GArray<char> Data;

	// Methods
	DbTablePriv() : Map(0, false, -1, {GV_NULL, -1})
	{
		First = Last = NULL;
		Rows = 0;
		Fixed = 0;
		Variable = 0;
		FixedSz = 0;
	}

	// This is called when the fields change
	bool OffsetFields()
	{
		int Pos = 0;

		Fixed = 0;
		Variable = 0;
		Map.Empty();
		
		for (unsigned i=0; i<Fields.Length(); i++)
		{
			LDbField &f = Fields[i];
			f.Offset = Pos;
			Pos += IsFixed(f.Type) ? f.Size() : sizeof(uint32);
			
			if (IsFixed(f.Type))
			{
				Fixed++;
				Map.Add(f.Id, Info(f.Type, f.Offset));
			}
			else
			{
				Map.Add(f.Id, Info(f.Type, Variable));
				Variable++;
			}
		}

		return true;
	}

	bool AddField(int Id, GVariantType Type)
	{
		if (Rows > 0)
		{
			LgiAssert(!"Adding fields with records not supported yet.");
			return false;
		}

		LDbField f;
		f.Id = Type;
		f.Type = Type;
		f.Offset = -1;

		if (IsFixed(Type))
			Fields.AddAt(Fixed, f);
		else // Is variable size
			Fields.New() = f;

		return OffsetFields();
	}

	bool DeleteField(int Id)
	{
		for (unsigned i=0; i<Fields.Length(); i++)
		{
			LDbField &f = Fields[i];
			if (f.Id == Id)
			{
				if (IsFixed(f.Type))
				{
					LgiAssert(i < Fixed && Fixed > 0);
					Fixed--;
				}
				else
				{
					LgiAssert(i >= Fixed && Variable > 0);
					Variable--;
				}
				Fields.DeleteAt(i, true);
			}
		}

		return OffsetFields();
	}

	bool DeleteRow(LDbRow *r)
	{
		if (r->Prev)
		{
			r->Prev->Next = r->Next;
		}
		else
		{
			LgiAssert(r == First);
			First = r->Next;
		}
		if (r->Next)
		{
			r->Next->Prev = r->Prev;
		}
		else
		{
			LgiAssert(r == Last);
			Last = r->Prev;
		}
		r->Prev = NULL;
		r->Next = NULL;
		Rows--;

		DeleteObj(r);
	
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////////
LDbRow::LDbRow(DbTablePriv *priv)
{
	d = priv;
	Next = NULL;
	Prev = NULL;
	Pos = -1;
}

LDbRow::~LDbRow()
{
}

int LDbRow::GetFields()
{
	return d->Fields.Length();
}

LDbField &LDbRow::GetField(int Idx)
{
	return d->Fields[Idx];
}

bool LDbRow::Delete()
{
	return d->DeleteRow(this);
}

bool LDbRow::CopyProps(GDataPropI &p)
{
	for (int i=0; i<GetFields(); i++)
	{
		LDbField &f = GetField(i);
		Store3Status Status;
		if (f.Type == GV_INT32)
			Status = SetInt(f.Id, p.GetInt(f.Id));
		else if (f.Type == GV_STRING)
			Status = SetStr(f.Id, p.GetStr(f.Id));
		else
			LgiAssert(!"Impl type");
		if (Status <= Store3Error)
			return false;
	}

	return true;
}

char *LDbRow::GetStr(int id)
{
	Info i = d->Map.Find(id);
	if (i.Offset < 0 || i.Type != GV_STRING)
		return NULL;	
	return Fix + Var[i.Offset];
}

Store3Status LDbRow::SetStr(int id, const char *str)
{
	return Store3Error;
}

int64 LDbRow::GetInt(int id)
{
	Info i = d->Map.Find(id);

	if (i.Offset < 0)
		return NULL;

	GPointer p = {(int8*)Fix+i.Offset};
	if (i.Type == GV_INT32)
		return *p.s32;

	if (i.Type == GV_INT64)
		return *p.s64;
	
	return -1;
}

Store3Status LDbRow::SetInt(int id, int64 val)
{
	Info i = d->Map.Find(id);

	if (i.Offset < 0)
		return Store3Error;

	GPointer p = {(int8*)Fix+i.Offset};
	if (i.Type == GV_INT32)
	{
		*p.s32 = (int32)val;
		return Store3Success;
	}

	if (i.Type == GV_INT64)
	{
		*p.s64 = val;
		return Store3Success;
	}

	return Store3Error;
}

LDateTime *LDbRow::GetDate(int id)
{
	LgiAssert(0);
	return NULL;
}

Store3Status LDbRow::SetDate(int id, LDateTime *i)
{
	LgiAssert(0);
	return Store3Error;
}

GVariant *LDbRow::GetVar(int id)
{
	return NULL;
}

Store3Status LDbRow::SetVar(int id, GVariant *i)
{
	return Store3Error;
}

GDataPropI *LDbRow::GetObj(int id)
{
	return NULL;
}

Store3Status LDbRow::SetObj(int id, GDataPropI *i)
{
	return Store3Error;
}

GDataIt LDbRow::GetList(int id)
{
	return NULL;
}

Store3Status LDbRow::SetRfc822(GStreamI *Rfc822Msg)
{
	return Store3Error;
}

////////////////////////////////////////////////////////////////////////////////////
LDbTable::LDbTable(const char *File)
{
	d = new DbTablePriv;
}

LDbTable::~LDbTable()
{
	Empty();
	DeleteObj(d);
}

bool LDbTable::AddField(int Id, GVariantType Type)
{
	return d->AddField(Id, Type);
}

bool LDbTable::DeleteField(int Id)
{
	return d->DeleteField(Id);
}

int LDbTable::GetFields()
{
	return d->Fields.Length();
}

LDbField &LDbTable::GetField(int Idx)
{
	return d->Fields[Idx];
}

bool LDbTable::Iterate(LDbRow *&Ptr)
{
	if (Ptr && Ptr->d != d)
	{
		LgiAssert(!"Wrong table.");
		return false;
	}

	if (Ptr)
		Ptr = Ptr->Next;
	else
		Ptr = d->First;

	return Ptr != NULL;
}

int LDbTable::GetRows()
{
	return d->Rows;
}

LDbRow *LDbTable::NewRow()
{
	LDbRow *r = new LDbRow(d);
	if (!r)
		return NULL;

	if (d->Last)
	{
		LgiAssert(d->Last->Next == NULL);
		d->Last->Next = r;
		r->Prev = d->Last;
		d->Last = r;
		d->Rows++;
	}
	else
	{
		LgiAssert(d->Rows == 0);
		d->First = d->Last = r;
		d->Rows = 1;
	}

	return r;
}

bool LDbTable::Empty()
{
	LDbRow *n;
	for (LDbRow *r = d->First; r; r = n)
	{
		n = r->Next;
		delete r;
	}

	d->First = d->Last = NULL;
	d->Rows = 0;

	return true;
}

bool LDbTable::DeleteRow(LDbRow *r)
{
	if (!r || r->d != d)
		return false;

	return d->DeleteRow(r);
}
