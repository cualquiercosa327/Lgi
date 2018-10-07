/*hdr
**      FILE:           GDb-Dbf.h
**      AUTHOR:         Matthew Allen
**      DATE:           8/2/2000
**      DESCRIPTION:    Dbf implementation of the GDb API
**
**      Copyright (C) 2000 Matthew Allen
**						fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GDb.h"

///////////////////////////////////////////////////////////////////
// Forward decl
class GDbfField;
class GDbfRecord;
class GDbfRecordset;

// Classes
class GDbfDb : public GDb
{
	char *Dir;

public:
	GDbfDb();
	~GDbfDb();

	// Attaching to data
	bool Connect(char *Init);
	bool Disconnect() { return true; }

	// Retrieving data
	GDbRecordset *Open(char *Name);
	GDbRecordset *TableAt(int i) { return 0; }
};

class GDbfRecord
{
	GDbfRecordset *Recordset;
	char *Data;

public:
	GDbfRecord(GDbfRecordset *rs);
	virtual ~GDbfRecord();

	char *GetData();
};

class GDbfField : public GDbField
{
	friend class GDbfRecord;
	friend class GDbfRecordset;

	GDbfRecordset *Recordset;

	char *FldName;
	int FldType;
	int FldLength;
	int MemLength;
	int DiskOffset; // into record data
	int MemOffset; // into record data
	short Decimal; // position of decimal point

	char *GetPtr();

public:
	GDbfField(GDbfRecordset *rs);
	~GDbfField();

	// Properties
	char *Name();
	bool Name(char *Str);
	int Type();
	bool Type(int NewType);
	int Length();
	bool Length(int NewLength);

	// Data access
	bool Set(GVariant &v);
	bool Get(GVariant &v);

	// Not implemented
	char *Description() { return 0; }
	bool Description(char *NewDesc) { return false; }
	bool Set(void *Data, int Length) { return false; }
	bool Get(void *&Data, int &Length) { return false; }
};

class GDbfRecordset : public GDbRecordset
{
	friend class GDbfRecord;
	friend class GDbfField;

	char *FileName;				// Name of DBF file
	GFile *File;					// Open file
	
	int MemLength; // memory record length
	ushort DiskLength; // disk record length
	ushort HeaderLength;
	uint RecordCount;
	
	List<GDbfField> F;
	List<GDbfRecord> R;
	GDbfRecord *Current;
	GDbfField *NullField;

public:
	GDbfRecordset(char *file);
	~GDbfRecordset();

	// State
	bool IsOpen()
	{
		return (File!=0) AND (FileName!=0) AND (F.GetItems()>0);
	}

	// Fields
	GDbField *operator [](int Index);
	GDbField *operator [](char *Name);
	bool InsertField(GDbField *Fld, int Index = -1);
	bool DeleteField(GDbField *Fld);
	int Fields();

	bool AddNew() { return false; }
	bool Edit() { return false; }
	bool Update() { return false; }
	void Cancel() { }

	// Markers
	bool Bof();
	bool Eof();

	// Moving around the records
	bool MoveFirst();
	bool MoveNext();
	bool MovePrev();
	bool MoveLast();
	int SeekRecord(int i);

	// Numbers/Counts
	int RecordIndex();
	int CountRecords();
	
	// Deleting
	bool DeleteRecord();
};

///////////////////////////////////////////////////////////////////
GDbfDb::GDbfDb()
{
	Dir = 0;
}

GDbfDb::~GDbfDb()
{
	DeleteArray(Dir);
}

bool GDbfDb::Connect(char *Init)
{
	DeleteArray(Dir);
	Dir = NewStr(Init);
	return Dir != 0;
}

GDbRecordset *GDbfDb::Open(char *Name)
{
	if (Dir AND Name)
	{
		char d[256];
		char *LastChar = Dir + (strlen(Dir)-1);
		if (*LastChar == DIR_CHAR) *LastChar = 0;

		sprintf(d, "%s%c%s", Dir, DIR_CHAR, Name);
		if (FileExists(d))
		{
			return new GDbfRecordset(d);
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////
GDbfRecord::GDbfRecord(GDbfRecordset *rs)
{
	Recordset = rs;
	Data = 0;
}

GDbfRecord::~GDbfRecord()
{
	DeleteArray(Data);
}

char *GDbfRecord::GetData()
{
	if (!Data AND
		Recordset AND
		Recordset->IsOpen())
	{
		// Get my index
		int Index = Recordset->R.IndexOf(this);
		if (Index >= 0)
		{
			// Allocate space
			Data = new char[Recordset->MemLength];
			if (Data)
			{
				// Seek to start of the record
				Recordset->File->Seek(	Recordset->HeaderLength +
										(Recordset->DiskLength * Index) + 1,
										SEEK_SET);
				
				// Read the record in
				if (Recordset->File->Read(Data, Recordset->DiskLength-1) == (Recordset->DiskLength-1))
				{
					for (GDbfField *f = Recordset->F.Last();
						f; f = Recordset->F.Prev())
					{
						// move the data into place
						memmove(Data+f->MemOffset, Data+f->DiskOffset, f->FldLength);
						if (f->MemLength > f->FldLength)
						{
							// zero out any space after the field
							// (for null-terminated strings)
							memset(Data+f->MemOffset+f->FldLength, 0, f->MemLength-f->FldLength);
						}

						// truncate trailing spaces in string data
						if (f->FldType == GV_STRING)
						{
							char *Start = Data + f->MemOffset;
							for (char *End = Start + f->FldLength - 1; End >= Start; End--)
							{
								if (*End == ' ')
								{
									*End = 0;
								}
								else break;
							}
						}
					}
				}
				else
				{
					// file is trucated
					DeleteArray(Data);
				}
			}
		}
	}

	return Data;
}

/////////////////////////////////////////////////////////////////////////
GDbfField::GDbfField(GDbfRecordset *rs)
{
	Recordset = rs;
	FldName = 0;
	FldType = 0;
	FldLength = 0;
	MemLength = 0;
	DiskOffset = 0;
	MemOffset = 0;
}

GDbfField::~GDbfField()
{
	DeleteArray(FldName);
}

char *GDbfField::Name()
{
	return FldName;
}

bool GDbfField::Name(char *Str)
{
	if (Str)
	{
		DeleteArray(FldName);
		FldName = NewStr(Str);
		return FldName != 0;
	}

	return false;
}

int GDbfField::Type()
{
	return FldType;
}

bool GDbfField::Type(int NewType)
{
	// TODO: implement field conversion
	return NewType == FldType;
}

int GDbfField::Length()
{
	switch (FldType)
	{
		case GV_INT32:
			return sizeof(int);
		case GV_DOUBLE:
			return sizeof(double);
		case GV_STRING:
			return FldLength;
	}

	return 0;
}

bool GDbfField::Length(int NewLength)
{
	// TODO: implement length conversion
	return false;
}

char *GDbfField::GetPtr()
{
	if (Recordset AND Recordset->Current)
	{
		char *d = Recordset->Current->GetData();
		if (d)
		{
			return d + MemOffset;
		}
	}

	return 0;
}

bool GDbfField::Set(GVariant &v)
{
	char *Data = GetPtr();

	switch (v.Type)
	{
		case GV_STRING:
		{
			char *Str = v.Str();
			int Len = strlen(Str);
			if (Len <= FldLength)
			{
				strcpy(Data, Str);
			}
			else
			{
				memcpy(Data, Str, FldLength);
				// should already have a NULL at the end of the field
			}
			return true;
		}
		case GV_INT32:
		{
			char *Str = GetPtr();
			if (Str)
			{
				char Buf[32];
				sprintf(Buf, "%i", v.Value.Int);
				int Len = strlen(Buf);
				int White = Len - FldLength;
				memset(Str, ' ', White);
				memcpy(Str+White, Buf, Len);
			}
			return false;
		}
		case GV_DOUBLE:
		{
			char *Str = GetPtr();
			if (Str)
			{
				char Format[8];
				char Buf[32];
				sprintf(Format, "%%.%if", Decimal);
				sprintf(Buf, Format, v.Value.Dbl);
				int Len = strlen(Buf);
				int White = Len - FldLength;
				memset(Str, ' ', White);
				memcpy(Str+White, Buf, Len);
			}
		}
		return false;
	}

	return false;
}

bool GDbfField::Get(GVariant &v)
{
	char *Data = GetPtr();

	switch (FldType)
	{
		case GV_STRING:
		{
			v = GetPtr();
			return true;
		}
		case GV_INT32:
		{
			v = atoi(Data);
			return true;
		}
		case GV_DOUBLE:
		{
			v = atof(Data);
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
GDbfRecordset::GDbfRecordset(char *file)
{
	FileName = NewStr(file);
	File = (FileName) ? new GFile : 0;
	DiskLength = 0;
	MemLength = 0;
	HeaderLength = 0;
	RecordCount = 0;
	Current = 0;
	NullField = new GDbfField(0);

	int MemOffset = 0;
	if (File AND File->Open(FileName, O_READ))
	{
		// Read header
		uchar Type;
		char LastUpdate[3];
		
		*File >> Type;
		File->Read(LastUpdate, 3);
		*File >> RecordCount;
		*File >> HeaderLength;
		*File >> DiskLength;
		File->Seek(32, SEEK_SET);

		// Read fields
		while (1)
		{
			char Name[16];
			ZeroObj(Name);
			File->Read(Name, 11);
			if (Name[0] != 0x0d)
			{
				GDbfField *Fld = new GDbfField(this);
				if (Fld)
				{
					Fld->FldName = NewStr(Name);
					
					uchar Type;
					*File >> Type;
					File->Seek(4, SEEK_CUR);
					uchar Length;
					*File >> Length;
					Fld->FldLength = Length;
					uchar Dec;
					*File >> Dec;
					Fld->Decimal = Dec;
					File->Seek(14, SEEK_CUR);

					switch (Type)
					{
						case 'C':
						case 'c':
						{
							Fld->FldType = GV_STRING;
							break;
						}
						case 'D':
						case 'd':
						{
							Fld->FldType = GV_DATETIME;
							break;
						}
						case 'F':
						case 'f':
						{
							Fld->FldType = GV_DOUBLE;
							break;
						}
						case 'L':
						case 'l':
						{
							Fld->FldType = GV_BINARY;
							break;
						}
						case 'M':
						case 'm':
						{
							Fld->FldType = GV_BINARY;
							break;
						}
						case 'N':
						case 'n':
						{
							Fld->FldType = (Dec>0) ? GV_DOUBLE : GV_INT32;
							break;
						}
						default:
						{
							// unknown field type
							break;
						}
					}

					Fld->DiskOffset = MemOffset;
					MemOffset += Fld->FldLength;
					Fld->MemOffset = MemLength;
					// add a NULL char for strings
					Fld->MemLength = Fld->FldLength + 1;
					MemLength += Fld->MemLength;

					// add field
					F.Insert(Fld);
				}
				else
				{
					// memory allocation error
					LgiAssert(0);
					break;
				}
			}
			else
			{
				// end of field list...
				break;
			}
		} // loop to next field

		// Create record objects
		for (uint i=0; i<RecordCount; i++)
		{
			R.Insert(new GDbfRecord(this));
		}
	}
}

GDbfRecordset::~GDbfRecordset()
{
	F.DeleteObjects();
	R.DeleteObjects();
	Current = 0;
}

GDbField *GDbfRecordset::operator [](int Index)
{
	GDbfField *f = F.ItemAt(Index);
	return (f) ? f : NullField;
}

GDbField *GDbfRecordset::operator [](char *Name)
{
	if (Name)
	{
		for (GDbfField *f = F.First(); f; f = F.Next())
		{
			if (f->Name() AND
				stricmp(f->Name(), Name) == 0)
			{
				return f;
			}
		}
	}

	return NullField;
}

bool GDbfRecordset::InsertField(GDbField *Fld, int Index)
{
	// TODO
	return false;
}

bool GDbfRecordset::DeleteField(GDbField *Fld)
{
	// TODO
	return false;
}

int GDbfRecordset::Fields()
{
	return F.GetItems();
}

bool GDbfRecordset::Bof()
{
	return Current == 0;
}

bool GDbfRecordset::Eof()
{
	return Current == 0;
}

bool GDbfRecordset::MoveFirst()
{
	Current = R.First();
	return Current != 0;
}

bool GDbfRecordset::MoveNext()
{
	Current = R.Next();
	return Current != 0;
}

bool GDbfRecordset::MovePrev()
{
	Current = R.Prev();
	return Current != 0;
}

bool GDbfRecordset::MoveLast()
{
	Current = R.Last();
	return Current != 0;
}

int GDbfRecordset::SeekRecord(int i)
{
	Current = R.ItemAt(i);
	return Current != 0;
}

int GDbfRecordset::RecordIndex()
{
	return (Current) ? R.IndexOf(Current) : -1;
}

int GDbfRecordset::CountRecords()
{
	return R.GetItems();
}

bool GDbfRecordset::DeleteRecord()
{
	// TODO
	return false;
}

///////////////////////////////////////////////////////////////////
GDb *OpenDbfDatabase(char *Dir)
{
	GDbfDb *Dbf = new GDbfDb;
	if (Dbf AND Dbf->Connect(Dir))
	{
		return Dbf;
	}

	DeleteObj(Dbf);
	return 0;
}

