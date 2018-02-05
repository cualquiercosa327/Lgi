#include "Lgi.h"
#include "LDbTable.h"

enum OffType
{
	VariableOff,
	FixedOff
};

#define MAGIC(v) LgiSwap32(v)

enum DbMagic
{
	TableMagic = MAGIC('tbl\0'),
	FieldMagic = MAGIC('fld\0'),
	RowMagic = MAGIC('row\0'),
};

inline bool IsFixed(GVariantType t)
{
	return	t != GV_STRING &&
			t != GV_BINARY;
}

struct Info
{
	GVariantType Type;
	int Index;

	Info(GVariantType t = GV_NULL, int index = -1)
	{
		Type = t;
		Index = index;
	}

	bool operator !=(const Info &i)
	{
		return Type != i.Type &&
				Index != i.Index;
	}
};

struct DbTablePriv
{
	// Fields
	unsigned Fixed; // Count of fixed size fields.
	unsigned FixedSz; // Byte size of fixed size fields.
	unsigned Variable; // Count of variable sized fields.
	GArray<LDbField> Fields;
	GArray<int32> FixedOffsets;
	GHashTbl<int, Info> Map;

	// Rows
	int Rows;
	LDbRow *First, *Last;
	GArray<char> Data;
	bool Dirty;

	// Methods
	DbTablePriv() : Map(0, false, -1, {GV_NULL, -1})
	{
		First = Last = NULL;
		Rows = 0;
		Fixed = 0;
		Variable = 0;
		FixedSz = 0;
		Dirty = false;
	}

	void SetDirty(bool b = true)
	{
		Dirty = b;
	}

	// This is called when the fields change
	bool OffsetFields()
	{
		int Pos = LDbRow::HeaderSz;

		Fixed = 0;
		FixedSz = 0;
		Variable = 0;
		Map.Empty();
		
		for (unsigned i=0; i<Fields.Length(); i++)
		{
			LDbField &f = Fields[i];
			f.Offset = Pos;
			Pos += IsFixed(f.Type) ? f.Size() : sizeof(uint32);
			
			if (IsFixed(f.Type))
			{
				FixedSz += f.Size();
				FixedOffsets[Fixed] = f.Offset;
				Map.Add(f.Id, Info(f.Type, Fixed++));
			}
			else
				Map.Add(f.Id, Info(f.Type, Variable++));
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
		f.Id = Id;
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
#define SERIALIZE(type, var) \
	if (Write) *p.type++ = var; \
	else var = *p.type++;

#define SERIALIZE_CAST(cast, type, var) \
	if (Write) *p.type++ = var; \
	else var = (cast) *p.type++;

bool LDbField::Serialize(GPointer &p, bool Write)
{
	char *Start = p.c;
	uint32 *Sz = NULL;
	if (Write)
	{
		*p.u32++ = FieldMagic;
		Sz = p.u32++;
	}
	else if (*p.u32 != FieldMagic)
		return false;
	else
		Sz = ++p.u32;
		
	SERIALIZE(s32, Id);
	SERIALIZE_CAST(GVariantType, u16, Type);

	if (Write)
		*Sz = p.c - Start;
	else
		p.c = Start + *Sz;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////
int LDbRow::HeaderSz = sizeof(uint32) << 1; // Magic + Size

LDbRow::LDbRow(DbTablePriv *priv)
{
	d = priv;
	Next = NULL;
	Prev = NULL;
	Pos = -1;
	Size = HeaderSz + d->FixedSz + (d->Variable * sizeof(uint32));
	Base.c = NULL;
	Offsets[FixedOff] = d->FixedOffsets.AddressOf();
	Offsets[VariableOff] = NULL;
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
	if (i.Index < 0 || i.Type != GV_STRING || !Base.c)
		return NULL;
	LgiAssert((unsigned)i.Index < d->Variable);
	return Base.c + Offsets[VariableOff][i.Index];
}

bool LDbRow::StartEdit()
{
	if (Edit.Length() == 0)
	{
		if (!Edit.Length(Size))
			return false;

		if (Base.c)
		{
			memcpy(Edit.AddressOf(), Base.c, Size);
		}
		else
		{
			// Initialize fixed fields to zero
			memset(Edit.AddressOf(), 0, d->FixedSz);
			// And the variable offset table to -1
			memset(Edit.AddressOf() + d->FixedSz, 0xff, Size - d->FixedSz);

			Base.c = Edit.AddressOf();
			Base.u32[0] = RowMagic;
			Base.u32[1] = Size;
		}

		PostEdit();
	}

	return true;
}

void LDbRow::PostEdit()
{
	Base.c = Edit.AddressOf();
	Offsets[VariableOff] = (int32*) Edit.AddressOf(d->FixedSz);
	Size = Edit.Length();
}

Store3Status LDbRow::SetStr(int id, const char *str)
{
	Info i = d->Map.Find(id);
	if (i.Index < 0)
		return Store3Error;

	if (!StartEdit())
		return Store3Error;

	size_t len = str ? strlen(str) + 1 : 1;
	Offsets[VariableOff][i.Index] = Edit.Length();
	if (str)
		Edit.Add((char*)str, len);
	else
		Edit.Add("", 1);

	PostEdit();
	d->SetDirty();

	return Store3Success;
}

int64 LDbRow::GetInt(int id)
{
	Info i = d->Map.Find(id);
	if (i.Index < 0 || !Base.c)
		return NULL;
	GPointer p = { Base.s8 + Offsets[FixedOff][i.Index] };
	if (i.Type == GV_INT32)
		return *p.s32;
	if (i.Type == GV_INT64)
		return *p.s64;
	return -1;
}

Store3Status LDbRow::SetInt(int id, int64 val)
{
	Info i = d->Map.Find(id);
	if (i.Index < 0)
		return Store3Error;
	if (!Base.c)
		StartEdit();
	GPointer p = { Base.s8 + Offsets[FixedOff][i.Index] };
	if (i.Type == GV_INT32)
	{
		*p.s32 = (int32)val;
		d->SetDirty();
		return Store3Success;
	}
	if (i.Type == GV_INT64)
	{
		*p.s64 = val;
		d->SetDirty();
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
	LgiAssert(0);
	return NULL;
}

Store3Status LDbRow::SetVar(int id, GVariant *i)
{
	LgiAssert(0);
	return Store3Error;
}

GDataPropI *LDbRow::GetObj(int id)
{
	LgiAssert(0);
	return NULL;
}

Store3Status LDbRow::SetObj(int id, GDataPropI *i)
{
	LgiAssert(0);
	return Store3Error;
}

GDataIt LDbRow::GetList(int id)
{
	LgiAssert(0);
	return NULL;
}

Store3Status LDbRow::SetRfc822(GStreamI *Rfc822Msg)
{
	LgiAssert(0);
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

bool LDbTable::Serialize(const char *Path, bool Write)
{
	GFile f;
	if (!f.Open(Path, Write ? O_WRITE : O_READ))
		return false;

	if (Write)
	{
		size_t Sz = sizeof(uint32) +
			d->Fields.Length() * (8 + 6);
		if (d->Data.Length() < Sz)
			d->Data.Length(Sz);
		GPointer p = {(int8*)d->Data.AddressOf()};
		*p.u32++ = TableMagic;
		for (unsigned i=0; i<d->Fields.Length(); i++)
		{
			if (!d->Fields[i].Serialize(p, true))
				return false;
		}
		f.SetSize(0);
		
		size_t Bytes = p.c - d->Data.AddressOf();
		LgiAssert(Bytes == Sz);
		if (f.Write(d->Data.AddressOf(), Bytes) != Bytes)
			return false;

		for (LDbRow *r = d->First; r; r = r->Next)
		{
			// Fix the size before we write
			LgiAssert(r->Base.u32[0] == RowMagic);
			r->Base.u32[1] = r->Size;
			if (f.Write(r->Base.c, r->Size) != r->Size)
				return false;
		}
	}
	else
	{
		// Read all the data into memory
		if (!d->Data.Length((size_t)f.GetSize()))
			return false;
		int Rd = f.Read(d->Data.AddressOf(0), (ssize_t)f.GetSize());
		if (Rd != f.GetSize())
			return false;
		GPointer p = {(int8*)d->Data.AddressOf()};
		if (*p.u32++ != TableMagic)
			return false;

		// Create all the fields
		while (*p.u32 == FieldMagic)
			d->Fields.New().Serialize(p, false);
		d->OffsetFields();
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum TestFields
{
	TestUid = 100,
	TestInt32,
	TestString,
	TestDate,
	TestString2,
};

bool LDbTable::UnitTests()
{
	LDbTable t;

	t.AddField(TestUid, GV_INT64);
	t.AddField(TestInt32, GV_INT32);
	t.AddField(TestString, GV_STRING);
	t.AddField(TestDate, GV_DATETIME);
	t.AddField(TestString2, GV_STRING);

	const char *Days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
	for (unsigned i=0; i<CountOf(Days); i++)
	{
		LDbRow *r = t.NewRow();
		r->SetInt(TestUid, 1000+i);
		r->SetInt(TestInt32, i * 3);
		if (i % 2)
		{
			r->SetStr(TestString, Days[i]);
			r->SetStr(TestString2, "asdasdasdasdasd");
		}
		else
		{
			r->SetStr(TestString2, "asdasdasdasdasd");
			r->SetStr(TestString, Days[i]);
		}
	}

	for (LDbRow *r = NULL; t.Iterate(r); )
	{
		int64 Id = r->GetInt(TestUid);
		int64 Int = r->GetInt(TestInt32);
		char *s = r->GetStr(TestString);
		char *s2 = r->GetStr(TestString2);
		LgiTrace("%i: %i, %s, %s\n", (int)Id, (int)Int, s, s2);
	}

	t.Serialize("test.db", true);

	return false;
}