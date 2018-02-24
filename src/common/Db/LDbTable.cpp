#include "Lgi.h"
#include "LDbTable.h"

///////////////////////////////////////////////////////////////////
#define MAGIC(v) LgiSwap32(v)

#define OBJ_HEAD(magic) \
	char *Start = p.c; \
	uint32 *Sz = NULL; \
	if (Write) \
	{ \
		*p.u32++ = magic; \
		Sz = p.u32++; \
	} \
	else if (*p.u32 != FieldMagic) \
		return false; \
	else \
	{ \
		p.u32++; \
		Sz = p.u32++; \
	}

#define SERIALIZE(type, var) \
	if (Write) *p.type++ = var; \
	else var = *p.type++;

#define SERIALIZE_FN(fn, type) \
	if (Write) *p.type++ = fn(); \
	else fn(*p.type++);

#define SERIALIZE_CAST(cast, type, var) \
	if (Write) *p.type++ = var; \
	else var = (cast) *p.type++;

#define OBJ_TAIL() \
	if (Write) \
		*Sz = p.c - Start; \
	else \
		p.c = Start + *Sz; \
	LgiAssert(Sizeof() == *Sz); \
	return true;

#define DB_DATE_SZ \
	( \
		2 + /* Year  */ \
		1 + /* Month */ \
		1 + /* Day   */ \
		1 + /* Hour  */ \
		1 + /* Min   */ \
		1 + /* Sec   */ \
		2   /* TimeZone */ \
	)

///////////////////////////////////////////////////////////////////
enum OffType
{
	VariableOff,
	FixedOff
};

enum DbMagic
{
	TableMagic = MAGIC('tbl\0'),
	FieldMagic = MAGIC('fld\0'),
	RowMagic = MAGIC('row\0'),
};

///////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////////////
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

	// Indexes
	GArray<DbIndex*> Indexes;

	// Methods
	DbTablePriv() : Map(0, false, -1, Info())
	{
		First = Last = NULL;
		Rows = 0;
		Fixed = 0;
		Variable = 0;
		FixedSz = 0;
		Dirty = false;
	}

	~DbTablePriv()
	{
		Indexes.DeleteObjects();
	}

	void SetDirty(bool b = true)
	{
		Dirty = b;
	}

	LDbField *FindField(int Id)
	{
		for (unsigned i=0; i<Fields.Length(); i++)
		{
			if (Fields[i].Id == Id)
				return Fields.AddressOf(i);
		}
		return NULL;
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
			Pos += IsFixed(f.Type) ? f.DataSize() : sizeof(uint32);
			
			if (IsFixed(f.Type))
			{
				FixedSz += f.DataSize();
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

//////////////////////////////////////////////////////////////////////////////////////
DbIndex::DbIndex(DbTablePriv *priv)
{
	d = priv;
}

DbIndex::~DbIndex()
{
	LgiAssert(d->Indexes.HasItem(this));
	d->Indexes.Delete(this);
}

DbArrayIndex::DbArrayIndex(DbTablePriv *priv) : DbIndex(priv)
{
	Fld.Id = -1;
	Fld.Type = GV_NULL;
	Fld.Offset = -1;
}

bool DbArrayIndex::OnNew(LDbRow *r)
{
	return Delete(r);
}

bool DbArrayIndex::OnDelete(LDbRow *r)
{
	Add(r);
	return true;
}

struct CompareParams
{
	int Id;
	bool Ascend;

	CompareParams(int i, bool a)
	{
		Id = i;
		Ascend = a;
	}
};

DeclGArrayCompare(RowIntCompare, LDbRow*, CompareParams)
{
	int64 A = (*a)->GetInt(param->Id);
	int64 B = (*b)->GetInt(param->Id);
	return (int) (param->Ascend ? A - B : B - A);
}

DeclGArrayCompare(RowStrCompare, LDbRow*, CompareParams)
{
	const char *A = (*a)->GetStr(param->Id);
	if (!A) A = "";

	const char *B = (*b)->GetStr(param->Id);
	if (!B) B = "";

	return param->Ascend ? stricmp(A, B) : stricmp(B, A);
}

DeclGArrayCompare(RowDateCompare, LDbRow*, CompareParams)
{
	LDateTime *A = (*a)->GetDate(param->Id);
	LDateTime *B = (*b)->GetDate(param->Id);
	if (!A || !B)
	{
		LgiAssert(0);
		return 0;
	}

	uint64 UtcA, UtcB;
	if (!A->Get(UtcA) ||
		!B->Get(UtcB))
	{
		LgiAssert(0);
		return 0;
	}

	int64 r = param->Ascend ? UtcA - UtcB : UtcB - UtcA;
	if (r < 0)
		return -1;
	if (r > 0)
		return 1;
	return 0;
}

bool DbArrayIndex::Sort(LDbField *fld, bool ascend)
{
	if (!fld)
		return false;

	Fld = *fld;
	Ascend = ascend;

	CompareParams p(Fld.Id, Ascend);
	switch (Fld.Type)
	{
		case GV_INT32:
		case GV_INT64:
			GArray<LDbRow*>::Sort(RowIntCompare, &p);
			break;
		case GV_STRING:
			GArray<LDbRow*>::Sort(RowStrCompare, &p);
			break;
		case GV_DATETIME:
			GArray<LDbRow*>::Sort(RowDateCompare, &p);
			break;
		default:
			LgiAssert(0);
			return false;
	}

	return true;
}

bool DbArrayIndex::Resort()
{
	return Sort(&Fld, Ascend);
}

///////////////////////////////////////////////////////////////////////////////////
size_t LDbDate::Sizeof()
{
	return DB_DATE_SZ;
}

bool LDbDate::Serialize(GPointer &p, LDateTime &dt, bool Write)
{
	#ifdef _DEBUG
	char *Start = p.c;
	#endif

	SERIALIZE_FN(dt.Year, u16);
	SERIALIZE_FN(dt.Month, u8);
	SERIALIZE_FN(dt.Day, u8);

	SERIALIZE_FN(dt.Hours, u8);
	SERIALIZE_FN(dt.Minutes, u8);
	SERIALIZE_FN(dt.Seconds, u8);

	uint16 Tz = dt.GetTimeZone();
	SERIALIZE(u16, Tz);
	if (!Write) dt.SetTimeZone(Tz, false);

	LgiAssert(p.c - Start == DB_DATE_SZ);
	return true;
}


///////////////////////////////////////////////////////////////////////////////////
int LDbField::DataSize()
{
	switch (Type)
	{
		case GV_BOOL:
			return 1;
		case GV_INT32:
			return 4;
		case GV_INT64:
			return 8;
		case GV_DATETIME:
			return DB_DATE_SZ;
		default:
			LgiAssert(!"Impl me.");
			break;
	}

	return 0;
}

size_t LDbField::Sizeof()
{
	return	8 +
			sizeof(Id) +
			2 + // Type
			sizeof(Offset);
}

bool LDbField::Serialize(GPointer &p, bool Write)
{
	OBJ_HEAD(FieldMagic);
		
	SERIALIZE(s32, Id);
	SERIALIZE(s32, Offset);
	SERIALIZE_CAST(GVariantType, u16, Type);

	OBJ_TAIL();
}

///////////////////////////////////////////////////////////////////////////////////
int LDbRow::HeaderSz = sizeof(uint32) << 1; // Magic + Size

LDbRow::LDbRow(DbTablePriv *priv)
{
	d = priv;
	Next = NULL;
	Prev = NULL;
	Pos = -1;
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

struct VarBlock
{
	int Index;
	size_t Start;
	size_t Len;
	int End() { return Start + Len; }
};

int VarCmp(VarBlock *a, VarBlock *b)
{
	return a->Start - b->Start;
}

size_t LDbRow::GetInitialSize()
{
	return HeaderSz + d->FixedSz + (d->Variable * sizeof(uint32));
}

bool LDbRow::Compact()
{
	if (Edit.Length())
	{
		// The variable sized fields can get fragmented, this function removes unused space.
		GArray<VarBlock> v;
		for (unsigned i=0; i<d->Variable; i++)
		{
			if (Offsets[VariableOff][i] >= 0)
			{
				VarBlock &b = v.New();
				b.Index = i;
				b.Start = Offsets[VariableOff][i];
				b.Len = strlen(Base.c + b.Start) + 1;
			}
		}
		v.Sort(VarCmp);
		size_t Pos = GetInitialSize();
		for (unsigned i=0; i<v.Length(); i++)
		{
			VarBlock &b = v[i];
			if (b.Start > Pos)
			{
				// Move block down
				memcpy(Base.c + Pos, Base.c + b.Start, b.Len);
				Offsets[VariableOff][b.Index] = Pos;
				b.Start = Pos;
			}
			Pos = b.Start + b.Len;
		}
		if (Base.u32[1] > Pos)
			Base.u32[1] = Pos;
	}

	return true;
}

GString LDbRow::ToString()
{
	GString::Array a;
	a.SetFixedLength(false);
	for (unsigned i=0; i<d->Fields.Length(); i++)
	{
		LDbField &f = d->Fields[i];
		switch (f.Type)
		{
			case GV_INT32:
			case GV_INT64:
				a.New().Printf(LGI_PrintfInt64, GetInt(f.Id));
				break;
			case GV_STRING:
			{
				GString s = GetStr(f.Id);
				if (s.Length() > 0)
					a.New() = s;
				else
					a.New() = "NULL";
				break;
			}
			case GV_DATETIME:
			{
				LDateTime *dt = GetDate(f.Id);
				if (dt)
					a.New() = dt->Get();
				else
					a.New() = "NULL";
				break;
			}
			default:
				LgiAssert(0);
				break;
		}
	}

	GString Sep(", ");
	return Sep.Join(a);
}

uint32 LDbRow::Size(uint32 Set)
{
	if (Base.c)
	{
		if (Set)
			Base.u32[1] = Set;

		return Base.u32[1];
	}

	LgiAssert(0);
	return 0;
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
		if (Base.c)
		{
			if (!Edit.Length(Base.u32[1]))
				return false;

			memcpy(Edit.AddressOf(), Base.c, Base.u32[1]);
		}
		else
		{
			int InitialSize = GetInitialSize();
			if (!Edit.Length(InitialSize))
				return false;

			Base.c = Edit.AddressOf();
			Base.u32[0] = RowMagic;
			Base.u32[1] = InitialSize;

			// Initialize fixed fields to zero
			memset(Base.c + 8, 0, d->FixedSz);
			if (d->Variable > 0)
			{
				// And the variable offset table to -1
				memset(Base.c + 8 + d->FixedSz, 0xff, InitialSize - d->FixedSz);
			}
		}

		PostEdit();
	}

	return true;
}

void LDbRow::PostEdit()
{
	Base.c = Edit.AddressOf();
	if (Base.c)
	{
		Size(Edit.Length());
		Offsets[VariableOff] = (int32*) Edit.AddressOf(8 + d->FixedSz);
	}
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
		Edit.Add((char*)"", 1);

	PostEdit();
	d->SetDirty();

	return Store3Success;
}

int64 LDbRow::GetInt(int id)
{
	Info i = d->Map.Find(id);
	if (i.Index < 0 || !Base.c)
		return -1;
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
	Info i = d->Map.Find(id);
	if (i.Index < 0 || !Base.c)
		return NULL;
	GPointer p = { Base.s8 + Offsets[FixedOff][i.Index] };
	if (i.Type != GV_DATETIME)
		return NULL;

	LDbDate dd;
	dd.Serialize(p, Cache, false);	
	return &Cache;
}

Store3Status LDbRow::SetDate(int id, LDateTime *dt)
{
	Info i = d->Map.Find(id);
	if (i.Index < 0 || !dt)
		return Store3Error;
	if (!Base.c)
		StartEdit();
	GPointer p = { Base.s8 + Offsets[FixedOff][i.Index] };
	if (i.Type != GV_DATETIME)
		return Store3Error;

	LDbDate dd;
	dd.Serialize(p, *dt, true);
	d->SetDirty();
	
	return Store3Success;
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
	if (File)
		Serialize(File, false);
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

	for (unsigned i=0; i<d->Indexes.Length(); i++)
		d->Indexes[i]->OnNew(r);

	return r;
}

bool LDbTable::Empty()
{
	d->Indexes.DeleteObjects();

	LDbRow *n;
	for (LDbRow *r = d->First; r; r = n)
	{
		n = r->Next;
		delete r;
	}

	d->First = d->Last = NULL;
	d->Rows = 0;
	d->Fixed = 0;
	d->FixedSz = 0;
	d->Variable = 0;
	d->Fields.Empty();
	d->FixedOffsets.Empty();
	d->Map.Empty();
	d->Data.Empty();

	return true;
}

bool LDbTable::DeleteRow(LDbRow *r)
{
	if (!r || r->d != d)
		return false;

	for (unsigned i=0; i<d->Indexes.Length(); i++)
		d->Indexes[i]->OnDelete(r);

	return d->DeleteRow(r);
}

DbArrayIndex *LDbTable::Sort(int Id, bool Ascending)
{
	LDbField *f = d->FindField(Id);
	if (!f)
		return NULL;

	// Collect all the records
	DbArrayIndex *i = new DbArrayIndex(d);
	if (!i)
		return NULL;
	if (!i->Length(d->Rows))
	{
		delete i;
		return NULL;
	}
	int Idx = 0;
	for (LDbRow *r = d->First; r; r = r->Next)
		(*i)[Idx++] = r;

	LgiAssert(Idx == d->Rows);

	// Sort them
	i->Sort(f, Ascending);

	// Save the index
	d->Indexes.Add(i);

	return i;
}

bool LDbTable::Serialize(const char *Path, bool Write)
{
	GFile f;
	if (!f.Open(Path, Write ? O_WRITE : O_READ))
		return false;

	if (Write)
	{
		if (d->Fields.Length() > 0)
		{
			size_t Sz = sizeof(uint32) +
				d->Fields.Length() * d->Fields[0].Sizeof();
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
		}

		for (LDbRow *r = d->First; r; r = r->Next)
		{
			// Fix the size before we write
			LgiAssert(r->Base.u32[0] == RowMagic);
			if (r->Edit.Length())
				r->Compact();
			if (f.Write(r->Base.c, r->Size()) != r->Size())
				return false;
		}
	}
	else
	{
		Empty();

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
		char *End = p.c + d->Data.Length();
		d->Fixed = 0;
		d->FixedSz = 0;
		d->Variable = 0;
		d->Map.Empty();
		d->FixedOffsets.Empty();
		while (p.c < End - 8 && *p.u32 == FieldMagic)
		{
			LDbField &f = d->Fields.New();
			if (!f.Serialize(p, false))
				return false;
			if (IsFixed(f.Type))
			{
				d->FixedSz += f.DataSize();
				d->FixedOffsets[d->Fixed] = f.Offset;
				d->Map.Add(f.Id, Info(f.Type, d->Fixed++));
			}
			else
			{
				d->Map.Add(f.Id, Info(f.Type, d->Variable++));
			}
		}

		// Create the rows
		d->Rows = 0;
		while (p.c < End - 8 && *p.u32 == RowMagic)
		{
			LDbRow *r = new LDbRow(d);
			if (!r)
				return false;
			r->Base = p;
			r->Offsets[FixedOff] = d->FixedOffsets.AddressOf();
			r->Offsets[VariableOff] = (int32*) (r->Base.c + 8 + d->FixedSz);
			if (d->Last)
			{
				r->Prev = d->Last;
				d->Last->Next = r;
				d->Last = r;
				d->Rows++;
			}
			else
			{
				d->First = d->Last = r;
				d->Rows = 1;
			}
			p.c += p.u32[1];
		}
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

GString LDbTable::ToString()
{
	GString::Array a;
	a.SetFixedLength(false);

	for (LDbRow *r = NULL; Iterate(r); )
	{
		GString s = r->ToString();
		a.Add(s);
	}

	GString Sep("\n");
	return Sep.Join(a);
}

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

	LgiTrace("Export table:\n");
	for (LDbRow *r = NULL; t.Iterate(r); )
	{
		int64 Id = r->GetInt(TestUid);
		int64 Int = r->GetInt(TestInt32);
		char *s = r->GetStr(TestString);
		char *s2 = r->GetStr(TestString2);
		LgiTrace("\t%i: %i, %s, %s\n", (int)Id, (int)Int, s, s2);
	}

	const char *File = "test.db";
	t.Serialize(File, true);

	LDbTable in;
	if (!in.Serialize(File, false))
		return false;

	LgiTrace("Import table:\n");
	LDbRow *Nine = NULL;
	for (LDbRow *r = NULL; in.Iterate(r); )
	{
		int64 Start = LgiMicroTime();
		int64 Id = r->GetInt(TestUid);
		int64 Int = r->GetInt(TestInt32);
		char *s = r->GetStr(TestString);
		char *s2 = r->GetStr(TestString2);
		int64 Time = LgiMicroTime()-Start;
		LgiTrace("\t%i: %i, %s, %s (%.4fms)\n", (int)Id, (int)Int, s, s2, (double)Time/1000.0);

		if (Int == 9)
			Nine = r;
		else if (Int == 12)
			r->SetStr(TestString2, "This is a new string.");
		else if (Int == 15)
			r->SetStr(TestString, NULL);
		else if (Int == 6)
			r->SetInt(TestInt32, 66);
	}

	if (!Nine)
		return false;
	in.DeleteRow(Nine);
	LgiTrace("Post delete:\n");
	for (LDbRow *r = NULL; in.Iterate(r); )
	{
		int64 Id = r->GetInt(TestUid);
		int64 Int = r->GetInt(TestInt32);
		char *s = r->GetStr(TestString);
		char *s2 = r->GetStr(TestString2);
		LgiTrace("\t%i: %i, %s, %s\n", (int)Id, (int)Int, s, s2);
	}

	if (!in.Serialize(File, true))
		return false;
	if (!t.Serialize(File, false))
		return false;

	LgiTrace("Reread:\n");
	for (LDbRow *r = NULL; t.Iterate(r); )
	{
		int64 Id = r->GetInt(TestUid);
		int64 Int = r->GetInt(TestInt32);
		char *s = r->GetStr(TestString);
		char *s2 = r->GetStr(TestString2);
		LgiTrace("\t%i: %i, %s, %s\n", (int)Id, (int)Int, s, s2);
	}

	return true;
}