#ifndef _LGI_SPELL_CHECK_H_
#define _LGI_SPELL_CHECK_H_

#include "GEventTargetThread.h"
#include "GOptionsFile.h"

enum SPELL_MSGS
{
	M_CHECK_TEXT = M_USER + 100,
	M_ENUMERATE_LANGUAGES,
	M_ENUMERATE_DICTIONARIES,
	M_SET_DICTIONARY,
	M_SET_PARAMS,
	M_ADD_WORD,
	M_INSTALL_DICTIONARY,
};

#define SPELL_CHK_VALID_HND(hnd) \
	if (hnd < 0)				\
	{							\
		LgiAssert(0);			\
		return false;			\
	}

// Spell check interface
class GSpellCheck : public GEventTargetThread
{
public:
	static const char Delimiters[];

	struct LanguageId
	{
		GString LangCode;
		GString EnglishName;
		GString NativeName;
	};
	
	struct DictionaryId
	{
		GString Lang;
		GString Dict;
	};

	struct Params
	{
		GOptionsFile::PortableType IsPortable;
		GString OptionsPath;
		GString Lang, Dict;
		GCapabilityTarget *CapTarget;
		
		Params()
		{
			CapTarget = NULL;
		}
	};
	
	struct SpellingError
	{
		int Start, Len;
		GString::Array Suggestions;
		
		int End() { return Start + Len; }
	};
	
	struct CheckText
	{
		GString Text;
		int Len;
		GArray<SpellingError> Errors;

		// Application specific data
		void *UserPtr;
		int64 UserInt;
		
		CheckText()
		{
			Len = 0;
			UserPtr = NULL;
			UserInt = 0;
		}
	};


	GSpellCheck(GString Name) : GEventTargetThread(Name) {}
	virtual ~GSpellCheck() {}

	// Impl OnEvent in your subclass:
	// GMessage::Result OnEvent(GMessage *Msg);

	/// Sends a M_ENUMERATE_LANGUAGES event to 'ResponseHnd' with a heap
	/// allocated GArray<LanguageId>.
	bool EnumLanguages(int ResponseHnd)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_ENUMERATE_LANGUAGES,
						(GMessage::Param)ResponseHnd);
	}

	/// Sends a M_ENUMERATE_DICTIONARIES event to 'ResponseHnd' with a heap
	/// allocated GArray<DictionaryId>.
	bool EnumDictionaries(int ResponseHnd, const char *Lang)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_ENUMERATE_DICTIONARIES,
						(GMessage::Param)ResponseHnd,
						(GMessage::Param)new GString(Lang));
	}

	bool SetParams(GAutoPtr<Params> p)
	{
		return PostObject(GetHandle(), M_SET_PARAMS, p);
	}

	bool SetDictionary(int ResponseHnd, const char *Lang, const char *Dictionary = NULL)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		
		GAutoPtr<DictionaryId> i(new DictionaryId);
		if (!i)
			return false;
		i->Lang = Lang;
		i->Dict = Dictionary;
		
		return PostObject(	GetHandle(),
							M_SET_DICTIONARY,
							(GMessage::Param)ResponseHnd,
							i);
	}

	bool Check(int ResponseHnd, GString s, int64 UserInt = 0, void *UserPtr = NULL)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		
		if (s.Length() == 0)
			return false;

		GAutoPtr<CheckText> c(new CheckText);
		GUtf8Str Utf(s);
		c->Text = s;
		c->Len = Utf.GetChars();
		c->UserInt = UserInt;
		c->UserPtr = UserPtr;
		
		return PostObject(GetHandle(), M_CHECK_TEXT, (GMessage::Param)ResponseHnd, c);
	}
	
	bool InstallDictionary()
	{
		return PostEvent(M_INSTALL_DICTIONARY);
	}
};

// These are the various implementations of the this object. You have to include the
// correct C++ source to get this to link.
extern GAutoPtr<GSpellCheck> CreateWindowsSpellCheck();		// Available on Windows 8.0 and greater
extern GAutoPtr<GSpellCheck> CreateAppleSpellCheck();		// Available on Mac OS X
extern GAutoPtr<GSpellCheck> CreateAspellObject();			// Available anywhere.

#endif