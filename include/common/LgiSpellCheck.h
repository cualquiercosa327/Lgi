#ifndef _SPELL_CHECK_H_
#define _SPELL_CHECK_H_

#ifdef WINDOWS
#include "Spellcheck.h"
#endif
#include "GEventTargetThread.h"

enum SPELL_MSGS
{
	M_CHECK_TEXT = M_USER + 100,
	M_ENUMERATE_DICTIONARIES,
	M_SET_DICTIONARY,
};

class GSpellCheck : public GEventTargetThread
{
	GEventSinkI *Owner;
	bool Ok;
	#ifdef WINDOWS
	ISpellCheckerFactory *Factory;
	ISpellChecker *Sc;
	#endif

public:
	GSpellCheck(GEventSinkI *owner) : GEventTargetThread("GSpellCheck")
	{
		Owner = owner;
		Ok = false;
		#ifdef WINDOWS
		Sc = NULL;
		Factory = NULL;
		#endif
	}

	~GSpellCheck()
	{
		#ifdef WINDOWS
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
		#endif
	}

	#ifdef WINDOWS
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
	#endif

	GMessage::Result OnEvent(GMessage *Msg)
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

				#ifdef WINDOWS
				GetSpellCheck(*Lang);
				#endif
				break;
			}
			case M_ENUMERATE_DICTIONARIES:
			{
				GAutoPtr<GString::Array> d(new GString::Array);

				#ifdef WINDOWS
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
				#endif

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
				#ifdef WINDOWS
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
				#endif				
				break;
			}
		}

		return 0;
	}

	bool EnumDictionaries()
	{
		return PostEvent(M_ENUMERATE_DICTIONARIES);
	}

	bool SetDictionary(const char *lang)
	{
		return PostEvent(M_SET_DICTIONARY, (GMessage::Param)new GString(lang));
	}

	bool Check(GString s)
	{
		return PostEvent(M_CHECK_TEXT, (GMessage::Param)new GString(s));
	}
};

#endif