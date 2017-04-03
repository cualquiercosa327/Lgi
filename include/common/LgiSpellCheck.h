#ifndef _LGI_SPELL_CHECK_H_
#define _LGI_SPELL_CHECK_H_

#include "GEventTargetThread.h"

enum SPELL_MSGS
{
	M_CHECK_TEXT = M_USER + 100,
	M_ENUMERATE_DICTIONARIES,
	M_SET_DICTIONARY,
};

// Spell check interface
class GSpellCheck : public GEventTargetThread
{
public:
	virtual ~GSpellCheck() {}

	// Impl this:
	// GMessage::Result OnEvent(GMessage *Msg);

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
	
	bool InstallDictionary()
	{
		LgiAssert(!"Impl.");
		return false;
	}
};

#endif