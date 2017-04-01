#include "LgiSpellcheck.h"

#include <initguid.h>
DEFINE_GUID(CLSID_SpellCheckerFactory,0x7AB36653,0x1796,0x484B,0xBD,0xFA,0xE7,0x4F,0x1D,0xB7,0xC1,0xDC);
DEFINE_GUID(IID_ISpellCheckerFactory,0x8E018A9D,0x2415,0x4677,0xBF,0x08,0x79,0x4E,0xA6,0x1F,0x94,0xBB);

struct GSpellCheckPriv
{
	bool Ok;
	ISpellCheckerFactory *Factory;
	ISpellChecker *Sc;

	GSpellCheckPriv()
	{
		Ok = false;
		Sc = NULL;
		Factory = NULL;
	}
	
	~GSpellCheckPriv()
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

	ISpellCheckerFactory *GSpellCheck::GetFactory()
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
		}
		return Factory;
	}

	ISpellChecker *GSpellCheck::GetSpellCheck(char *Lang)
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
};

GSpellCheck::GSpellCheck(GEventSinkI *owner) : GEventTargetThread("GSpellCheck")
{
	d = new GSpellCheckPriv;
	Owner = owner;
}

GSpellCheck::~GSpellCheck()
{
	delete d;
}

GMessage::Result GSpellCheck::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_SET_DICTIONARY:
		{
			GAutoPtr<GString> Lang((GString*)Msg->A());
			if (!Lang)
			{
				LgiTrace("%s:%i - No language specified.\n", _FL);
				break;
			}

			GetSpellCheck(*Lang);
			break;
		}
		case M_ENUMERATE_DICTIONARIES:
		{
			GAutoPtr<GString::Array> d(new GString::Array);

			if (!GetFactory())
			{
				LgiTrace("%s:%i - No factory.\n", _FL);
				break;
			}

			IEnumString *Value = NULL;
			HRESULT r = Factory->get_SupportedLanguages(&Value);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - get_SupportedLanguages failed: %i\n", _FL, r);
				break;
			}

			LPOLESTR Str;
			while (SUCCEEDED((r = Value->Next(1, &Str, NULL))) && Str)
			{
				d->Add(Str);
			}

			Value->Release();

			if (d && d->Length() > 0)
				Owner->PostEvent(M_ENUMERATE_DICTIONARIES, (GMessage::Param)d.Release());
			break;
		}
		case M_CHECK_TEXT:
		{
			GAutoPtr<GString> Txt((GString*)Msg->A());
			if (!Txt)
			{
				LgiTrace("%s:%i - No text specified.\n", _FL);
				break;
			}

			if (!Sc)
			{
				LgiTrace("%s:%i - No spell check loaded.\n", _FL);
				break;
			}

			GAutoWString WTxt(Utf8ToWide(*Txt));
			IEnumSpellingError *ErrEnum = NULL;
			HRESULT r = Sc->Check(WTxt, &ErrEnum);
			if (FAILED(r))
			{
				LgiTrace("%s:%i - Check failed: %i.\n", _FL, r);
				break;
			}

			ISpellingError *Err = NULL;
			while (SUCCEEDED((r = ErrEnum->Next(&Err))))
			{
				ULONG Start, Len;
				CORRECTIVE_ACTION Action;
				LPWSTR Replace = NULL;

				r = Err->get_StartIndex(&Start);
				r = Err->get_Length(&Len);
				r = Err->get_CorrectiveAction(&Action);
				r = Err->get_Replacement(&Replace);

				Err->Release();
			}

			ErrEnum->Release();
			break;
		}
	}

	return 0;
}

