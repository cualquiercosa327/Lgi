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

class WindowsSpellCheck : public GSpellCheck
{
	bool Ok;
	ISpellCheckerFactory *Factory;
	ISpellChecker *Sc;

	GHashTbl<const char *, int> Languages;
	GString::Array Dictionaries;

public:
	WindowsSpellCheck() :
		GSpellCheck("GSpellCheck"),
		Languages(0, false, NULL, 0)
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
					GString &s = Dictionaries.New();
					s = Str;
			
					GString::Array a = s.Split("-", 1);
					if (a.Length() == 2)
					{
						int Cur = Languages.Find(a[0]);
						Languages.Add(a[0], Cur + 1);
					}

					CoTaskMemFree(Str);
				}

				Value->Release();
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

				const char *l;
				for (int c = Languages.First(&l); c; c = Languages.Next(&l))
				{
					LanguageId &i = Langs->New();
					i.LangCode = l;
				}

				if (Langs && Langs->Length() > 0)
					PostObject(ResponseHnd, Msg->Msg(), Langs);
				break;
			}
			case M_ENUMERATE_DICTIONARIES:
			{
				int ResponseHnd = (int)Msg->A();
				GAutoPtr<GString> Lang((GString*)Msg->B());

				if (!GetFactory())
				{
					LgiTrace("%s:%i - No factory.\n", _FL);
					break;
				}

				GAutoPtr< GArray<DictionaryId> > Out(new GArray<DictionaryId>);
				for (unsigned i=0; i<Dictionaries.Length(); i++)
				{
					GString &d = Dictionaries[i];
					GString::Array a = d.Split("-", 1);
					if (a.Length() == 1 || a[0].Equals(*Lang))
					{
						DictionaryId &id = Out->New();
						id.Lang = *Lang;
						id.Dict = a[1];
					}
				}

				if (Out && Out->Length() > 0)
					PostObject(ResponseHnd, Msg->Msg(), Out);
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

