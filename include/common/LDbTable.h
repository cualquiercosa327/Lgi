#ifndef _DB_TABLE_H_
#define _DB_TABLE_H_

#include "GVariant.h"
#include "Store3.h"

struct DbTablePriv;
class LDbTable;

struct LDbDate
{
	size_t Sizeof();
	bool Serialize(GPointer &p, LDateTime &dt, bool Write);
};

struct LDbField
{
	int Id;
	GVariantType Type;
	int Offset;

	int DataSize();
	size_t Sizeof();
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

	// Date cache
	LDateTime Cache;
	
	LDbRow(struct DbTablePriv *priv);
	bool StartEdit();
	void PostEdit();
	bool Compact();
	uint32 GetInitialSize();

public:
	static int HeaderSz;

	~LDbRow();

	// Fields
	size_t GetFields();
	LDbField &GetField(size_t Idx);

	// Row level op
	bool Delete();
	uint32 Size(uint32 Set = 0);
	GString ToString();

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

class DbIndex
{
	DbTablePriv *d;

public:
	DbIndex(DbTablePriv *priv);
	virtual ~DbIndex();
	virtual bool OnNew(LDbRow *r) = 0;
	virtual bool OnDelete(LDbRow *r) = 0;
};

class DbArrayIndex : public DbIndex, public GArray<LDbRow*>
{
	friend class LDbTable;

	LDbField Fld;
	bool Ascend;

	DbArrayIndex(DbTablePriv *priv);
	bool Sort(LDbField *fld, bool ascend);

public:
	bool OnNew(LDbRow *r);
	bool OnDelete(LDbRow *r);
	bool Resort();
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
	
	/// This returns a sorted array of rows according to the specified
	/// id and sort direction. The array remains the property of the 
	/// table. When done just free the index object.
	DbArrayIndex *Sort(int Id, bool Ascending = true);

	// IO
	bool Serialize(const char *Path, bool Write);

	// Testing
	GString ToString();
	static bool UnitTests();
};


#endif
