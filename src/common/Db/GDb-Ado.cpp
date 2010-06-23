/*hdr
**      FILE:           GDb-Ado.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/2/2000
**      DESCRIPTION:    Ado implemetation of the GDb API
**
**      Copyright (C) 2000 Matthew Allen
**						fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GDb.h"

#import <msado15.dll> no_namespace rename("EOF", "adoEOF") implementation_only

class GAdoField : public GDbField
{
	char *Value;
	char *FldName;
	ADODB::Field *Fld;

public:
	GAdoField(ADODB::_RecordsetPtr Rs1, int i)
	{
		Value = 0;
		FldName = 0;
		_variant_t n((long) i);
		Fld = (Rs1) ? Rs1->Fields->GetItem(n) : 0;
		if (Fld)
		{
			FldName = NewStr(Fld->GetName());
		}
	}

	GAdoField(ADODB::_RecordsetPtr Rs1, char *FieldName)
	{
		Value = 0;
		FldName = 0;
		_variant_t n(FieldName);
		Fld = (Rs1) ? Rs1->Fields->GetItem(n) : 0;
		if (Fld)
		{
			FldName = NewStr(Fld->GetName());
		}
	}

	~GAdoField()
	{
		DeleteArray(Value);
		DeleteArray(FldName);
	}

	char *Name()
	{
		return FldName;
	}

	bool Name(char *Str)
	{
		return false;
	}

	int Type()
	{
		if (Fld)
		{
			switch ((long) Fld->Type)
			{
				case ADODB::adTinyInt:
				case ADODB::adSmallInt:
				case ADODB::adUnsignedTinyInt:
				case ADODB::adUnsignedSmallInt:
				case ADODB::adInteger:
				case ADODB::adUnsignedInt:
				{
					return GDbInt;
					break;
				}
				case ADODB::adSingle:
				case ADODB::adDouble:
				{
					return GDbFloat;
					break;
				}
				case ADODB::adBoolean:
				{
					return GDbBoolean;
					break;
				}
				case ADODB::adVarChar:
				{
					return GDbString;
					break;
				}
				case ADODB::adLongVarChar:
				{
					return GDbMemo;
					break;
				}
			}
		}

		return GDbVoid;
	}

	bool Type(int NewType)
	{
		return false;
	}

	int Length()
	{
		if (Fld)
		{
			switch ((long) Fld->Type)
			{
				case ADODB::adTinyInt:
				case ADODB::adUnsignedTinyInt:
				{
					return sizeof(char);
				}
				case ADODB::adSmallInt:
				case ADODB::adUnsignedSmallInt:
				{
					return sizeof(short);
				}
				case ADODB::adInteger:
				case ADODB::adUnsignedInt:
				{
					return sizeof(int);
					break;
				}
				case ADODB::adSingle:
				case ADODB::adDouble:
				{
					return sizeof(double);
					break;
				}
				case ADODB::adBoolean:
				{
					return sizeof(bool);
					break;
				}
				case ADODB::adVarChar:
				{
					return Fld->GetDefinedSize();
					break;
				}
				case ADODB::adLongVarChar:
				{
					return -1; // variable
					break;
				}
			}
		}

		return 0;
	}

	bool Length(int NewLength)
	{
		return false;
	}

	char *Description()
	{
		return "";
	}

	bool Description(char *NewDesc)
	{
		return false;
	}

	bool Set(char *Str)
	{
		if (Fld)
		{
			char *s = NewStr(Str);
			if (s)
			{
				long SLen = strlen(s);
				long DLen = Fld->GetDefinedSize();
				if (SLen > DLen) s[DLen] = 0;
				_variant_t D(s);
				Fld->PutValue(D);

				DeleteArray(s);
				return true;
			}
		}

		return false;
	}

	bool Set(int Int)
	{
		if (Fld)
		{
			_variant_t D((long) Int);
			Fld->PutValue(D);
			return true;
		}
		return false;
	}

	bool Set(double Dbl)
	{
		if (Fld)
		{
			_variant_t D(Dbl);
			Fld->PutValue(D);
			return true;
		}
		return false;
	}

	bool Set(void *Data, int Length)
	{
		if (Fld)
		{
			SAFEARRAY sa;
			VARIANT vd;
			SAFEARRAY *psa = &sa;

			ZeroObj(sa);
			sa.cbElements = 1;
			sa.pvData = Data;
			sa.cDims = 1;
			sa.rgsabound[0].cElements = Length;
			sa.rgsabound[0].lLbound = 0;
			vd.vt = VT_ARRAY | VT_UI1;
			vd.pparray = &psa;

			_variant_t D(vd);
			Fld->PutValue(D);
			return true;
		}
		return false;
	}

	bool Get(char *&Str)
	{
		if (Fld)
		{
			_variant_t v = Fld->GetValue();
			if (v.vt == VT_BSTR OR
				v.vt == (VT_BSTR | VT_BYREF))
			{
				_bstr_t bstr((v.vt & VT_BYREF) ? *v.pbstrVal : v.bstrVal, false);
				DeleteArray(Value);
				Value = NEW(char[bstr.length()+1]);
				if (Value)
				{
					for (int i=0; i<bstr.length(); i++)
					{
						Value[i] = v.bstrVal[i];
					}
					Value[bstr.length()] = 0;
				}
				Str = Value;
				return true;
			}
		}
		return false;
	}

	bool Get(int &Int)
	{
		if (Fld)
		{
			_variant_t var = Fld->GetValue();
			VARIANT v = var.Detach();
			switch (v.vt)
			{
				case VT_UI1: Int=v.bVal; return true;
				case VT_I2: Int=v.iVal; return true;
				case VT_I4: Int=v.lVal; return true;
				case VT_BYREF|VT_UI1: Int=*v.pbVal; return true;
				case VT_BYREF|VT_I2: Int=*v.piVal; return true;
				case VT_BYREF|VT_I4: Int=*v.plVal; return true;
        	}
		}

		return false;
	}

	bool Get(double &Dbl)
	{
		if (Fld)
		{
			_variant_t var = Fld->GetValue();
			VARIANT v = var.Detach();
			switch (v.vt)
			{
				case VT_R4: Dbl=v.fltVal; return true;
				case VT_R8: Dbl=v.dblVal; return true;
				case VT_BYREF|VT_R4: Dbl=*v.pfltVal; return true;
				case VT_BYREF|VT_R8: Dbl=*v.pdblVal; return true;
        	}
		}

		return false;
	}

	bool Get(void *&Data, int &Length)
	{
		return false;
	}
};

/////////////////////////////////////////////////////////////////
class GAdoRecordset : public GDbRecordset
{
	ADODB::_RecordsetPtr Rs1;
	List<GAdoField> Fields;
	GAdoField *NullField;

public:
	GAdoRecordset(ADODB::_ConnectionPtr &Con, char *Sql)
	{
		NullField = NEW(GAdoField(0, 0));
		if (Sql)
		{
			Rs1.CreateInstance(__uuidof(ADODB::Recordset));
			if (Rs1)
			{
				_bstr_t bstrSource(Sql);
				_variant_t vtCon((IDispatch*)Con);
				if (Rs1->Open(bstrSource, vtCon, ADODB::adOpenDynamic, ADODB::adLockOptimistic, -1) == S_OK)
				{
					for (int i=0; i<CountFields(); i++)
					{
						Fields.Insert(NEW(GAdoField(Rs1, i)));
					}
				}
				else
				{
					// error
					Rs1->Close();
					Rs1 = 0;
				}
			}
		}
	}

	~GAdoRecordset()
	{
		if (Rs1)
		{
			Rs1->Close();
			Rs1 = 0;
		}
	}

	GDbField *Field(int Index)
	{
		GAdoField *f = Fields.ItemAt(Index);
		return (f) ? f : NullField;
	}

	GDbField *Field(char *Name)
	{
		if (Name)
		{
			for (GAdoField *f = Fields.First(); f; f = Fields.Next())
			{
				if (stricmp(Name, f->Name()) == 0)
				{
					return f;
				}
			}
		}

		return NullField;
	}

	bool InsertField(GDbField *Fld, int Index)
	{
		return false;
	}

	bool DeleteField(GDbField *Fld)
	{
		return false;
	}

	int CountFields()
	{
		if (Rs1)
		{
			return Rs1->Fields->GetCount();
		}
		return 0;
	}

	bool AddNew()
	{
		if (Rs1)
		{
			return Rs1->AddNew() == S_OK;
		}

		return false;
	}

	bool Edit()
	{
		return true; // not needed
	}

	bool Update()
	{
		if (Rs1)
		{
			return Rs1->Update() == S_OK;
		}

		return false;
	}

	bool Bof()
	{
		return (Rs1) ? Rs1->GetBOF() : true;
	}

	bool Eof()
	{
		return (Rs1) ? Rs1->GetAdoEOF() : true;
	}

	bool MoveFirst()
	{
		if (Rs1)
		{
			return Rs1->MoveFirst() == S_OK;
		}

		return false;
	}

	bool MoveNext()
	{
		if (Rs1)
		{
			return Rs1->MoveNext() == S_OK;
		}
		
		return false;
	}

	bool MovePrev()
	{
		if (Rs1)
		{
			return Rs1->MovePrevious() == S_OK;
		}

		return false;
	}

	bool MoveLast()
	{
		if (Rs1)
		{
			return Rs1->MoveLast() == S_OK;
		}

		return false;
	}

	int SeekRecord(int i)
	{
		if (Rs1)
		{
			return Rs1->Move(i) == S_OK;
		}

		return false;
	}

	int RecordIndex()
	{
		if (Rs1)
		{
			return Rs1->GetAbsolutePosition() - 1;
		}

		return false;
	}

	int CountRecords()
	{
		if (Rs1)
		{
			return Rs1->GetRecordCount();
		}

		return false;
	}

	bool DeleteRecord()
	{
		if (Rs1)
		{
			return Rs1->Delete(ADODB::adAffectCurrent) == S_OK;
		}

		return false;
	}
};

/////////////////////////////////////////////////////////////////
class GAdoDb : public GDb
{
	ADODB::_ConnectionPtr Con;
	char *ErrorStr;

public:
	GAdoDb()
	{
		ErrorStr = 0;
	}

	~GAdoDb()
	{
		DeleteArray(ErrorStr);
		Disconnect();
	}

	// Attaching to data
	bool Connect(char *Init)
	{
		_bstr_t bstrEmpty(L"");
		Con.CreateInstance(__uuidof(ADODB::Connection));
		if (Con)
		{
			try
			{
				Con->ConnectionString = Init;
				Con->Open(bstrEmpty, bstrEmpty, bstrEmpty, -1);
			}
			catch (_com_error e)
			{
				_bstr_t d = e.Description();
				ErrorStr = NewStr((char*)d);
				return false;
			}
			
			return true;
		}

		return false;
	}

	bool Disconnect()
	{
		if (Con)
		{
			Con->Close();
			Con = 0;
		}

		return true;
	}

	// Retrieving data
	GDbRecordset *OpenRecordset(char *Sql)
	{
		return NEW(GAdoRecordset(Con, Sql));
	}

	GDbRecordset *GetTables()
	{
		return 0;
	}

	GDbRecordset *GetFields(char *Table)
	{
		return 0;
	}
};

/////////////////////////////////////////////////////////////////
GDb *OpenAdoDatabase(char *Name)
{
	GAdoDb *Db = NEW(GAdoDb);
	if (Db AND NOT Db->Connect(Name))
	{
		DeleteObj(Db);
	}

	return Db;
}

