/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief Tools for getting data into and out of the UI

#ifndef __DATA_DLG_TOOLS
#define __DATA_DLG_TOOLS

#include "GProperties.h"
#include "GScrollBar.h"
#include "GTextLabel.h"

enum DataDlgType
{
	DATA_STR = 1,
	DATA_BOOL,
	DATA_INT,
	DATA_FLOAT,
	DATA_PASSWORD,
	DATA_STR_SYSTEM		// Operating system specific string. Has the OS
						// tag automatically appended to the option name
						// e.g. SomeProperty-Win32
};

class DataDlgTools;

/// UI <-> properties field
class DataDlgField
{
	friend class DataDlgTools;

protected:
	int Type;
	int CtrlId;
	char *Option;
	char *Desc;

public:
	DataDlgField(int type, int ctrlid, char *opt, char *desc = 0);
	~DataDlgField();

	int GetType() { return Type; }
	int GetCtrl() { return CtrlId; }
	char *GetOption() { return Option; }
	char *GetDesc() { return Desc; }

	void SetOption(char *o) { char *Opt = NewStr(o); DeleteArray(Option); Option = Opt; }
};

typedef List<DataDlgField> DataDlgFieldList;

/// UI <-> properties mapping class
class DataDlgTools
{
protected:
	bool DeleteEmptyStrings;
	GView *Dlg;
	ObjProperties *Options;

public:
	DataDlgTools();

	void Set(GView *Dlg, ObjProperties *Options);
	ObjProperties *GetOptions();

	bool ProcessField(DataDlgField *f, bool Write, char *OptionOverride = 0);
	bool ProcessFields(DataDlgFieldList &Field, bool Write);
	bool LoadFields(DataDlgFieldList &Field);
	bool SaveFields(DataDlgFieldList &Field);
};

class DRecordSetObj
{
public:
	virtual bool Serialize(ObjProperties &f, bool Write)
	{
		return false;
	}
};

template <class Record, class Lst>
class DRecordSetCtrls : public DataDlgTools
{
	// Controls
	GText *Description;
	GScrollBar *Scroll;
	int NewRecordId;
	int DeleteRecordId;

	DataDlgFieldList *Fields;
	
protected:
	Record *Current;
	Lst *Records;

public:
	char *RecordStringTemplate;

	DRecordSetCtrls(	GView *window,
						DataDlgFieldList *fields,
						Lst *records,
						int DescId,
						int ScrollId,
						int NewId,
						int DelId,
						char *Template)
	{
		Current = 0;
		RecordStringTemplate = NewStr(Template);
		Dlg = window;
		Fields = fields;
		Records = records;
		NewRecordId = NewId;
		DeleteRecordId = DelId;

		if (Dlg)
		{
			Description = dynamic_cast<GText*>(Dlg->FindControl(DescId));
			Scroll = dynamic_cast<GScrollBar*>(Dlg->FindControl(ScrollId));
			OnMoveRecord(dynamic_cast<Record*>(Records->First()));
		}
	}
	~DRecordSetCtrls()
	{
		DeleteArray(RecordStringTemplate);
	}

	virtual Record *NewObj() { return 0; }
	virtual void DelObj(Record *Obj) {}

	int GetCurrentIndex()
	{
		return (Records AND Current) ? Records->IndexOf(Current) : -1;
	}

	void Serialize(bool Write)
	{
		if (Fields AND Dlg AND Current)
		{
			ObjProperties Temp;
			Options = &Temp;

			if (Write)
			{
				// Move data from controls to propertiy list
				SaveFields(*Fields);

				// Move data from property list to record
				Current->Serialize(Temp, false);
			}
			else
			{
				// Move data from record to property list
				Current->Serialize(Temp, true);

				// Load the new data
				LoadFields(*Fields);
			}

			Options = 0;
		}
	}

	void OnMoveRecord(Record *r)
	{
		if (Dlg AND Fields)
		{
			Serialize(true);
			Current = r;
			Serialize(false);

			// Set controls enabled flag correctly
			for (DataDlgField *f = Fields->First(); f; f = Fields->Next())
			{
				GView *Ctrl = dynamic_cast<GView*>(Dlg->FindControl(f->GetCtrl()));
				if (Ctrl)
				{
					if (!Current)
					{
						// clear controls
					}

					Ctrl->Enabled(Current != 0);
				}
			}

			// Set the scrollbar
			if (Scroll)
			{
				Scroll->SetLimits(0, Records->Length() - 1);
				Scroll->Value((Current) ? Records->IndexOf(Current) : 0);
				Scroll->SetPage(1);
			}

			// Set the description
			if (Description)
			{
				char Str[256];
				sprintf(Str, RecordStringTemplate ? RecordStringTemplate : (char*)"Record: %i of %i", GetCurrentIndex()+1, (Records) ? Records->Length() : 0);
				Description->Name(Str);
			}
		}
	}

	int OnNotify(GViewI *Col, int Flags)
	{
		if (Fields AND Dlg AND Records)
		{
			if (Col == Scroll)
			{
				int CurrentIndex = (Current) ? Records->IndexOf(Current) : -1;
				int NewIndex = Col->Value();
				if (CurrentIndex != NewIndex)
				{
					OnMoveRecord(dynamic_cast<Record*>((*Records)[NewIndex]));
				}
			}

			if (Col->GetId() == NewRecordId)
			{
				Record *r = NewObj();
				if (r)
				{
					Records->Insert(r);
					OnMoveRecord(r);
				}
			}

			if (Col->GetId() == DeleteRecordId)
			{
				int Index = GetCurrentIndex();
				LgiAssert(Index >= 0);

				Record *r = dynamic_cast<Record*>((*Records)[Index]);
				if (r)
				{
					Records->Delete(r);

					Index = limit(Index, 0, Records->Length()-1);
					OnMoveRecord(Index >= 0 ? dynamic_cast<Record*>((*Records)[Index]) : 0);
					DelObj(r);
				}
			}
		}

		return 0;
	}
};

template <class Record, class Lst>
class DRecordSet : public DRecordSetCtrls<Record, Lst>
{
public:
	typedef Record *(*Allocator)(GView *);

	DRecordSet
	(
		GView *window,
		DataDlgFieldList *fields,
		Lst *records,
		int DescId,
		int ScrollId,
		int NewId,
		int DelId,
		char *Template,
		Allocator a
	) : DRecordSetCtrls<Record, Lst>
		(
			window,
			fields,
			records,
			DescId,
			ScrollId,
			NewId,
			DelId,
			Template
		)
	{
		Alloc = a;
	}

	Record *NewObj()
	{
		return Alloc ? Alloc(DRecordSetCtrls<Record, Lst>::Dlg) : 0;
	}

	void DelObj(Record *Obj)
	{
		if (Obj)
		{
			DeleteObj(Obj);
		}
	}

	Record *ItemAt(int i=-1)
	{
			return (i<0 OR !DRecordSetCtrls<Record, Lst>::Records)
			?
			dynamic_cast<Record*>(DRecordSetCtrls<Record, Lst>::Current)
			:
			dynamic_cast<Record*>( (*DRecordSetCtrls<Record, Lst>::Records)[i] );
	}

	void OnMoveRecord(Record *r)
	{
		DRecordSetCtrls<Record, Lst>::OnMoveRecord(r);
	}

private:
	Allocator Alloc;
};

#endif

