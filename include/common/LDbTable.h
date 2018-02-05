#ifndef _DB_TABLE_H_
#define _DB_TABLE_H_

#include "GVariant.h"
#include "Store3.h"

struct DbTablePriv;
class LDbTable;

struct LDateStore
{
	uint16 Year;
	uint16 TimeZone;
	uint8 Day, Month, Hour, Minute, Second, Flags;
};

struct LDbField
{
	int Id;
	GVariantType Type;
	int Offset;

	int Size()
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
				return sizeof(LDateStore);
			default:
				LgiAssert(!"Impl me.");
				break;
		}

		return 0;
	}

	bool Serialize(GPointer &p, bool Write);
};

class LDbRow : public GDataPropI
{
	friend class LDbTable;
	friend struct DbTablePriv;

	// Global table specific data
	DbTablePriv *d;

	// The doubly linked list of rows.
	LDbRow *Next, *Prev;
	
	// This is the position in the tables read-only data
	// for this row.
	ssize_t Pos;

	// This is the total size of the row
	ssize_t Size;

	// When editing a record, it can grow in size, so we copy the
	// Read-only data in the table into an edit buffer own by this
	// record.
	GArray<char> Edit;

	// This pointers to the record data.
	// Format
	//		uint32 Magic;
	//		char FixedSizeData[d->FixedSz]
	//		uint32 VariableOffsets[d->Variable]
	//		char VariableData[??]
	GPointer Base;

	// This points to the offset data:
	// [0] -> Variable offset table (part of this record)
	// [1] -> Fixed offset table (owned by 'd')
	int32 *Offsets[2];
	
	LDbRow(struct DbTablePriv *priv);
	bool StartEdit();
	void PostEdit();

public:
	static int HeaderSz;

	~LDbRow();

	// Fields
	int GetFields();
	LDbField &GetField(int Idx);

	// Row level op
	bool Delete();

	// Data access
	bool CopyProps(GDataPropI &p);
	char *GetStr(int id);
	Store3Status SetStr(int id, const char *str);
	int64 GetInt(int id);
	Store3Status SetInt(int id, int64 i);
	LDateTime *GetDate(int id);
	Store3Status SetDate(int id, LDateTime *i);
	GVariant *GetVar(int id);
	Store3Status SetVar(int id, GVariant *i);
	GDataPropI *GetObj(int id);
	Store3Status SetObj(int id, GDataPropI *i);
	GDataIt GetList(int id);
	Store3Status SetRfc822(GStreamI *Rfc822Msg);
};

class LDbTable
{
	struct DbTablePriv *d;

public:
	LDbTable(const char *File = NULL);
	~LDbTable();

	// Fields
	bool AddField(int Id, GVariantType Type);
	bool DeleteField(int Id);
	int GetFields();
	LDbField &GetField(int Idx);

	// Rows
	bool Empty();
	bool Iterate(LDbRow *&Ptr);
	int GetRows();
	LDbRow *NewRow();
	bool DeleteRow(LDbRow *r);

	// IO
	bool Serialize(const char *Path, bool Write);

	// Testing
	static bool UnitTests();
};


#endif