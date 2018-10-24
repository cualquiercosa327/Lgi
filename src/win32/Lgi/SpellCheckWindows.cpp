#include "Lgi.h"
#include "LgiSpellcheck.h"
#if _MSC_VER >= _MSC_VER_VS2013
#include "Spellcheck.h"
#endif

#ifdef __spellcheck_h__

#include <initguid.h>
DEFINE_GUID(CLSID_SpellCheckerFactory,0x7AB36653,0x1796,0x484B,0xBD,0xFA,0xE7,0x4F,0x1D,0xB7,0xC1,0xDC);
DEFINE_GUID(IID_ISpellCheckerFactory,0x8E018A9D,0x2415,0x4677,0xBF,0x08,0x79,0x4E,0xA6,0x1F,0x94,0xBB);

#include "GHashTable.h"

int LangCmp(GSpellCheck::LanguageId *a, GSpellCheck::LanguageId *b)
{
	return Stricmp(a->LangCode.Get(), b->LangCode.Get());
}

int DictionaryCmp(GSpellCheck::DictionaryId *a, GSpellCheck::DictionaryId *b)
{
	return Stricmp(a->Dict.Get(), b->Dict.Get());
}

class WindowsSpellCheck : public GSpellCheck
{
	bool Ok;
	ISpellCheckerFactory *Factory;
	ISpellChecker *Sc;

	struct Lang
	{
		GString Id;
		GString English;
		GString Native;
		GString::Array Dictionaries;
	};

	LHashTbl<ConstStrKey<char,false>, Lang*> Languages;

	bool ReadCsv()
	{
		GString File = __FILE__;
		File = File.Replace(".cpp", ".csv");
		if (!FileExists(File))
		{
			GAutoString f(LgiFindFile(LgiGetLeaf(File)));
			File = f.Get();
		}

		GFile f;
		if (!f.Open(File, O_READ))
			return false;
		GString::Array Lines = f.Read().SplitDelimit("\r\n");
		for (GString *Ln = NULL; Lines.Iterate(Ln);)
		{
			GString::Array v = Ln->Split(",");
			if (v.Length() > 1)
			{
				Lang *l = Languages.Find(v[0].Strip());
				if (l)
				{
					l->English = v[1].Strip();
					if (v.Length() > 2)
						l->Native = v[2].Strip();
				}
			}
		}

		return true;
	}

public:
	WindowsSpellCheck() :
		GSpellCheck("GSpellCheck")
	{
		Ok = false;
		Sc = NULL;
		Factory = NULL;
	}

	~WindowsSpellCheck()
	{
		if (Sc)
		{
			Sc->Release();
			Sc = NULL;
		}
		if (Factory)
		{
			Factory->Release();
			Factory = NULL;
		}
	}

	ISpellCheckerFactory *GetFactory()
	{
		if (!Factory)
		{
			HRESULT r = CoInitialize(NULL);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - CoInitialize failed.\n", _FL);
				return NULL;
			}

			r = CoCreateInstance(CLSID_SpellCheckerFactory, NULL, CLSCTX_INPROC_SERVER, IID_ISpellCheckerFactory, (LPVOID*)&Factory);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - CoCreateInstance(CLSID_SpellCheckerFactory) failed.\n", _FL);
				return NULL;
			}

			IEnumString *Value = NULL;
			r = Factory->get_SupportedLanguages(&Value);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - get_SupportedLanguages failed: %i\n", _FL, r);
			}
			else
			{
				LPOLESTR Str;
				while (SUCCEEDED((r = Value->Next(1, &Str, NULL))) && Str)
				{
					GString s = Str;			
					GString::Array a = s.Split("-", 1);
					if (a.Length() == 2)
					{
						Lang *l = Languages.Find(a[0]);
						if (!l)
						{
							Languages.Add(a[0], l = new Lang);
							l->Id = a[0];
						}
						if (l)
						{
							l->Dictionaries.New() = s;
						}
					}

					CoTaskMemFree(Str);
				}

				Value->Release();

				ReadCsv();
			}
		}

		return Factory;
	}

	ISpellChecker *GetSpellCheck(char *Lang)
	{
		if (!GetFactory())
		{
			LgiTrace("%s:%i - No factory.\n", _FL);
			return NULL;
		}
		
		GAutoWString WLang(Utf8ToWide(Lang));
		HRESULT r = Factory->CreateSpellChecker(WLang, &Sc);
		if (FAILED(r))
		{
			LgiTrace("%s:%i - CreateSpellChecker failed: %i.\n", _FL, r);
			return NULL;
		}

		return Sc;
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_ENUMERATE_LANGUAGES:
			{
				if (!GetFactory())
				{
					LgiTrace("%s:%i - No factory.\n", _FL);
					break;
				}

				int ResponseHnd = (int)Msg->A();
				GAutoPtr< GArray<LanguageId> > Langs(new GArray<LanguageId>);

				// const char *Id;
				// for (Lang *l = Languages.First(&Id); l; l = Languages.Next(&Id))
				for (auto l : Languages)
				{
					LanguageId &i = Langs->New();
					i.LangCode = l.value->Id;
					i.EnglishName = l.value->English ? l.value->English : l.value->Id;
					i.NativeName = l.value->Native;
				}

				if (Langs && Langs->Length() > 0)
				{
					Langs->Sort(LangCmp);
					PostObject(ResponseHnd, Msg->Msg(), Langs);
				}
				break;
			}
			case M_ENUMERATE_DICTIONARIES:
			{
				int ResponseHnd = (int)Msg->A();
				GAutoPtr<GString> Param((GString*)Msg->B());

				if (!GetFactory())
				{
					LgiTrace("%s:%i - No factory.\n", _FL);
					break;
				}

				GAutoPtr< GArray<DictionaryId> > Out(new GArray<DictionaryId>);
				Lang *l = Languages.Find(*Param);
				if (!l)
				{
					LgiTrace("%s:%i - Language '%s' not found.\n", _FL, Param->Get());
					break;
				}
				
				for (unsigned i=0; i<l->Dictionaries.Length(); i++)
				{
					GString &d = l->Dictionaries[i];
					GString::Array a = d.Split("-", 1);
					if (a.Length() == 2)
					{
						DictionaryId &id = Out->New();
						id.Lang = l->Id.Get();
						id.Dict = a[1].Get();
					}
				}

				if (Out && Out->Length() > 0)
				{
					Out->Sort(DictionaryCmp);
					PostObject(ResponseHnd, Msg->Msg(), Out);
				}
				break;
			}
			case M_SET_DICTIONARY:
			{
				int ResponseHnd = (int)Msg->A();
				GAutoPtr<DictionaryId> Dict((DictionaryId*)Msg->B());
				bool Success = false;
				if (Dict)
				{
					GString Lang;
					Lang.Printf("%s-%s", Dict->Lang.Get(), Dict->Dict.Get());
					Success = GetSpellCheck(Lang) != NULL;
				}

				PostThreadEvent(ResponseHnd, M_SET_DICTIONARY, (GMessage::Param)Success);
				break;
			}
			case M_CHECK_TEXT:
			{
				int ResponseHnd = (int)Msg->A();
				GAutoPtr<CheckText> Ct((CheckText*)Msg->B());
				if (!Ct)
				{
					LgiTrace("%s:%i - No text specified.\n", _FL);
					break;
				}
				if (!Sc)
				{
					LgiTrace("%s:%i - No spell check loaded.\n", _FL);
					break;
				}

				GAutoWString WTxt(Utf8ToWide(Ct->Text));
				IEnumSpellingError *ErrEnum = NULL;
				HRESULT r = Sc->Check(WTxt, &ErrEnum);
				if (FAILED(r))
				{
					LgiTrace("%s:%i - Check failed: %i.\n", _FL, r);
					break;
				}

				ISpellingError *Err = NULL;
				while (SUCCEEDED((r = ErrEnum->Next(&Err))) && Err)
				{
					ULONG Start, Len;
					CORRECTIVE_ACTION Action;
					LPWSTR Replace = NULL;

					r = Err->get_StartIndex(&Start);
					r = Err->get_Length(&Len);
					r = Err->get_CorrectiveAction(&Action);
					r = Err->get_Replacement(&Replace);

					Err->Release();

					SpellingError &Se = Ct->Errors.New();
					Se.Start = Start;
					Se.Len = Len;
					if (Action == CORRECTIVE_ACTION_GET_SUGGESTIONS)
					{
						GAutoWString Word(NewStrW(WTxt + Start, Len));
						IEnumString *Strs = NULL;
						r = Sc->Suggest(Word, &Strs);
						if (SUCCEEDED(r))
						{
							LPOLESTR Str;
							while (SUCCEEDED((r = Strs->Next(1, &Str, NULL))) && Str)
							{
								Se.Suggestions.Add(Str);
								CoTaskMemFree(Str);
							}
							Strs->Release();
						}
					}
				}

				ErrEnum->Release();

				PostObject(ResponseHnd, Msg->Msg(), Ct);
				break;
			}
		}

		return 0;
	}
};

#endif

GAutoPtr<GSpellCheck> CreateWindowsSpellCheck()
{
	GAutoPtr<GSpellCheck> p;
	#ifdef __spellcheck_h__
	p.Reset(new WindowsSpellCheck);
	#endif
	return p;
}

