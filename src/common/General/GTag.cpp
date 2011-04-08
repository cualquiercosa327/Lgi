#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "Lgi.h"
#include "GTag.h"
#include "Base64.h"
#include "GCombo.h"
#include "GCheckBox.h"
#include "GRadioGroup.h"

GTag::GTag(char *e)
{
	ObscurePasswords = true;
	Element = NewStr(e);
}

GTag::~GTag()
{
	Empty();
	DeleteArray(Element);
}

void GTag::Empty()
{
	DeleteObjects();
}

bool GTag::operator ==(char *s)
{
	return stricmp(Element, s) == 0;
}

GTag &GTag::operator =(GTag &t)
{
	DeleteObjects();
	DeleteArray(Element);

	Element = NewStr(t.Element);
	for (GNamedVariant *v=t.First(); v; v=t.Next())
	{
		for (GNamedVariant *v=t.First(); v; v=t.Next())
		{
			GNamedVariant *n = new GNamedVariant(v->Name());
			if (n)
			{
				Insert(n);
				(GVariant&)*n = *v;
			}
		}
	}
	
	return *this;
}

bool GTag::IsNumber(char *s)
{
	for (; s AND *s; s++)
	{
		if (NOT isdigit(*s) OR strchr("e.-", *s))
		{
			return false;
		}
	}

	return true;
}
	
bool GTag::Read(XmlTag *t)
{
	if (*this == t->Name)
	{
		for (XmlValue *v=t->Values.First(); v; v=t->Values.Next())
		{
			if (ObscurePasswords AND stristr(v->Name, "Password"))
			{
				char Clear[256];
				ZeroObj(Clear);
				ConvertBase64ToBinary((uchar*)Clear, sizeof(Clear), v->Value, strlen(v->Value));
				GVariant i(Clear);
				SetVariant(v->Name, i);
			}
			else
			{
				GVariantType Type = TypeOf(v->Name);
				if (Type == GV_NULL)
				{
					Type = IsNumber(v->Value) ? GV_INT32 : GV_STRING;
				}

				switch (Type)
				{
					case GV_INT32:
					{
						GVariant i = atoi(v->Value);
						SetVariant(v->Name, i);
						break;
					}
					default: // string
					{
						GVariant i = v->Value;
						SetVariant(v->Name, i);;
						break;
					}
				}
			}
		}

		return true;
	}

	return false;
}

void GTag::Write(GFile &f)
{
	f.Print("<%s ", Element);
	
	int n = 0;
	for (GNamedVariant *v=First(); v; v=Next(), n++)
	{
		if (n) f.Print("\n\t");

		if (ObscurePasswords AND stristr(v->Name(), "Password"))
		{
			char Hidden[256];
			ZeroObj(Hidden);
			ConvertBinaryToBase64(Hidden, sizeof(Hidden), (uchar*)v->Str(), strlen(v->Str()));
			f.Print("%s=\"%s\"", v->Name(), Hidden);
		}
		else
		{
			if (v->IsInt())
			{
				f.Print("%s=%i", v->Name(), v->Value.Int);
			}
			else if (v->IsString())
			{
				f.Print("%s=\"%s\"", v->Name(), v->Value.String);
			}
			else LgiAssert(0);
		}
	}

	f.Print(" />\n");
}

GNamedVariant *GTag::GetNamed(char *Name)
{
	if (Name)
	{
		for (GNamedVariant *v=First(); v; v=Next())
		{
			if (stricmp(v->Name(), Name) == 0)
			{
				return v;
			}
		}
	}

	return 0;
}

GVariantType GTag::TypeOf(char *Name)
{
	GNamedVariant *v = GetNamed(Name);
	if (v)
	{
		return v->Type;
	}
	return GV_NULL;
}

bool GTag::GetVariant(const char *Name, GVariant &Value, char *Array)
{
	GNamedVariant *v = GetNamed(Name);
	if (v)
	{
		Value = *v;
		return true;
	}

	return false;
}

bool GTag::SetVariant(const char *Name, GVariant &Value, char *Array)
{
	GNamedVariant *v = GetNamed(Name);
	if (Value.IsNull())
	{
		if (v)
		{
			if (Delete(v))
			{
				DeleteObj(v);
			}
			else
			{
				printf("%s:%i - Delete failed.\n", __FILE__, __LINE__);
			}
		}
	}
	else
	{
		if (v)
		{
			*(GVariant*)v = Value;
			return true;
		}
		else
		{
			GNamedVariant *n = new GNamedVariant(Name);
			if (n)
			{
				(GVariant&)*n = Value;
				Insert(n);
				return true;
			}
		}
	}

	return false;
}

void GTag::SerializeUI(GView *Dlg, GMap<char*,int> &Fields, bool To)
{
	for (int i=0; i<Fields.Length(); i++)
	{
		char *Opt = Fields.NameAt(i);
		if (Opt)
		{
			GView *View = Dlg->FindControl(Fields[Opt]);
			GCombo *Cbo = dynamic_cast<GCombo*>(View);
			GCheckBox *Chk = dynamic_cast<GCheckBox*>(View);
			GRadioGroup *Grp = dynamic_cast<GRadioGroup*>(View);
			bool IsIntOnly = Cbo OR Chk OR Grp;

			if (To) // Mem -> UI
			{
				GVariant Value;
				if (GetVariant(Opt, Value))
				{
					if (Value.IsInt() OR IsIntOnly)
					{
						if (Value.Str())
						{
							Dlg->SetCtrlValue(Fields[Opt], atoi(Value.Str()));
						}
						else
						{
							Dlg->SetCtrlValue(Fields[Opt], Value.Value.Int);
						}
					}
					else
					{
						Dlg->SetCtrlName(Fields[Opt], Value.Str());
					}
				}
			}
			else // UI -> Mem
			{
				if (IsIntOnly)
				{
					GVariant Value(View->Value());
					SetVariant(Opt, Value);
				}
				else
				{
					char *v = Dlg->GetCtrlName(Fields[Opt]);
					if (v)
					{
						int Type = TypeOf(Opt);
						if (Type == GV_NULL) Type = IsNumber(v) ? GV_INT32 : GV_STRING;

						switch (Type)
						{
							case GV_INT32:
							{
								GVariant Value(atoi(v));
								SetVariant(Opt, Value);
								break;
							}
							default:
							case GV_STRING:
							{
								GVariant Value(v);
								SetVariant(Opt, Value);
								break;
							}
						}
					}
					else
					{
						GVariant v;
						SetVariant(Opt, v);
					}
				}
			}
		}
	}
}

