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
		
		DictionaryId *i = new DictionaryId;
		if (!i)
			return false;
		i->Lang = Lang;
		i->Dict = Dictionary;
		
		return PostEvent(M_SET_DICTIONARY,
						(GMessage::Param)ResponseHnd,
						(GMessage::Param)i);
	}

	bool Check(int ResponseHnd, GString s)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_CHECK_TEXT, (GMessage::Param)ResponseHnd, (GMessage::Param)new GString(s));
	}
	
	bool InstallDictionary()
	{
		LgiAssert(!"Impl.");
		return false;
	}
};

// These are the various implementations of the this object. You have to include the
// correct C++ source to get this to link.
extern GAutoPtr<GSpellCheck> CreateWindowsSpellCheck();		// Available on Windows 8.0 and greater
extern GAutoPtr<GSpellCheck> CreateAppleSpellCheck();		// Available on Mac OS X
extern GAutoPtr<GSpellCheck> CreateAspellObject();			// Available anywhere.

#endif