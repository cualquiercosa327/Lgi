#ifndef _LGI_SPELL_CHECK_H_
#define _LGI_SPELL_CHECK_H_

#include "GEventTargetThread.h"

enum SPELL_MSGS
{
	M_CHECK_TEXT = M_USER + 100,
	M_ENUMERATE_DICTIONARIES,
	M_SET_DICTIONARY,
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
	GSpellCheck(GString Name) : GEventTargetThread(Name) {}
	virtual ~GSpellCheck() {}

	// Impl OnEvent in your subclass:
	// GMessage::Result OnEvent(GMessage *Msg);

	bool EnumDictionaries(int ResponseHnd)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_ENUMERATE_DICTIONARIES, (GMessage::Param)ResponseHnd);
	}

	bool SetDictionary(int ResponseHnd, const char *lang)
	{
		SPELL_CHK_VALID_HND(ResponseHnd);
		return PostEvent(M_SET_DICTIONARY, (GMessage::Param)ResponseHnd, (GMessage::Param)new GString(lang));
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