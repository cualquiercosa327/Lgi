/*
**	FILE:			GuiDataDlg.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			30/10/98
**	DESCRIPTION:	Data serialisation classes
**
**	Copyright (C) 1998, Matthew Allen
**		fret@memecode.com
*/


#include <stdio.h>

#include "Lgi.h"
#include "GPassword.h"
#include "GDataDlg.h"

////////////////////////////////////////////////////////////////////////////////////////////////
DataDlgField::DataDlgField(int type, int ctrlid, char *opt, char *desc)
{
	Type = type;
	CtrlId = ctrlid;
	Option = NewStr(opt);
	Desc = NewStr(desc);
}

DataDlgField::~DataDlgField()
{
	DeleteArray(Option);
	DeleteArray(Desc);
}

/////////////////////////////////////////////////////////////////////////////////
DataDlgTools::DataDlgTools()
{
	DeleteEmptyStrings = true;
	Dlg = 0;
	Options = 0;
}

void DataDlgTools::Set(GView *dlg, ObjProperties *options)
{
	Dlg = dlg;
	Options = options;
}

ObjProperties *DataDlgTools::GetOptions()
{
	return Options;
}

bool DataDlgTools::ProcessField(DataDlgField *f, bool Write, char *OptionOverride)
{
	char *Opt = (OptionOverride) ? OptionOverride : f->Option;
	GViewI *v = Dlg->FindControl(f->CtrlId);
	if (Options && Dlg && Opt && v)
	{
		char OsOpt[256] = "";
		switch (f->Type)
		{
			case DATA_PASSWORD:
			{
				GPassword g;
				if (Write) // Ctrl -> Opts
				{
					if (Dlg->GetCtrlEnabled(f->GetCtrl()))
					{
						char *Pass = v->Name();
						if (Pass)
						{
							g.Set(Pass);
							// g.Serialize(Options, f->GetOption(), Write);
							LgiAssert(!"Not impl");
						}
					}
					else
					{
						Options->DeleteKey(f->GetOption());
					}
				}
				else // Opts -> Ctrl
				{
					// bool HasPass = g.Serialize(Options, f->GetOption(), Write);
					LgiAssert(!"Not impl");
					// Dlg->SetCtrlEnabled(f->GetCtrl(), HasPass);
				}
				break;
			}
			case DATA_STR_SYSTEM:
			{
				sprintf(OsOpt, "%s-%s", Opt, LgiGetOsName());
				Opt = OsOpt;
				// fall through
			}
			case DATA_STR:
			case DATA_FILENAME:
			{
				if (Write) // Ctrl -> Opts
				{
					char *s = v->Name();
					if (DeleteEmptyStrings && !ValidStr(s))
					{
						Options->DeleteKey(Opt);
					}
					else
					{
						Options->Set(Opt, ValidStr(s) ? s : (char*)"");
					}
				}
				else // Opts -> Ctrl
				{
					char *n = 0;
					Options->Get(Opt, n);
					v->Name(n);
				}
				break;
			}
			case DATA_BOOL:
			case DATA_INT:
			{
				if (Write) // Ctrl -> Opts
				{
					Options->Set(Opt, (int)v->Value());
				}
				else // Opts -> Ctrl
				{
					int n = 0;
					Options->Get(Opt, n);
					v->Value(n);
				}
				break;
			}
			default:
			{
				LgiAssert(0);
				return false;
				break;
			}
		}

		return true;
	}

	return false;
}

bool DataDlgTools::ProcessFields(DataDlgFieldList &Field, bool Write)
{
	int Processed = 0;
	
	for (DataDlgField *f = Field.First(); f; f = Field.Next())
	{
		if (ProcessField(f, Write))
		{
			Processed++;
		}
	}

	return Processed == Field.Length();
}

bool DataDlgTools::LoadFields(DataDlgFieldList &Field)
{
	return ProcessFields(Field, false);
}

bool DataDlgTools::SaveFields(DataDlgFieldList &Field)
{
	return ProcessFields(Field, true);
}
