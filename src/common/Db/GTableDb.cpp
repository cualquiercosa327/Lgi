#include "Lgi.h"
#include "GTableDb.h"

/////////////////////////////////////////////////////////////////////////////////////////////
struct GTableDbPriv
{
	GArray<GTableDb::Table*> Tables;
};

/////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::Row::Row(GTableDb::Table *tbl, uint64 id)
{
}

GTableDb::Row::~Row()
{
}

int32 GTableDb::Row::GetInt(int FieldId)
{
	return 0;
}

int64 GTableDb::Row::GetInt64(int FieldId)
{
	return 0;
}

const char *GTableDb::Row::GetStr(int FieldId)
{
	return 0;
}

bool GTableDb::Row::SetInt(int FieldId, int32 value)
{
	return 0;
}

bool GTableDb::Row::SetInt64(int FieldId, int64 value)
{
	return 0;
}

bool GTableDb::Row::GetStr(int FieldId, const char *value)
{
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::Table::Table(const char *Name)
{
}

GTableDb::Table::~Table()
{
}

bool GTableDb::Table::DeleteTable()
{
	return false;
}

bool GTableDb::Table::AddField(char *Name, int FieldId, GVariantType Type)
{
	return false;
}

bool GTableDb::Table::DeleteField(int FieldId)
{
	return false;
}

GTableDb::Field *GTableDb::Table::GetField(int FieldId)
{
	return false;
}

GTableDb::Field *GTableDb::Table::GetFieldAt(int Index)
{
	return false;
}

GTableDb::Row *GTableDb::Table::NewRow(GVariant *Key)
{
	return false;
}

bool GTableDb::Table::DeleteRow(int Index)
{
	return false;
}

GTableDb::Row *GTableDb::Table::operator[](int index)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
GTableDb::GTableDb(const char *BaseFolder, int BlockSize)
{
}

GTableDb::~GTableDb()
{
}

GArray<GTableDb::Table*> &GTableDb::Tables()
{
	return d->Tables;
}

GTableDb::Table *GTableDb::CreateTable(const char *Name)
{
	return 0;
}

bool GTableDb::Commit()
{
	return false;
}
