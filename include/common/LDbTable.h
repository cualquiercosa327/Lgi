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
};

class LDbRow : public GDataPropI
{
	friend class LDbTable;
	friend struct DbTablePriv;

	DbTablePriv *d;
	LDbRow *Next, *Prev;
	
	ssize_t Pos;
	GArray<char> Edit;
	char *Fix;
	int32 *Var;
	
	LDbRow(struct DbTablePriv *priv);

public:
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
};


#endif