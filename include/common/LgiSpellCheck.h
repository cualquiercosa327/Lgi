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
	struct GSpellCheckPriv *d;
	GEventSinkI *Owner;

public:
	GSpellCheck(GEventSinkI *owner);
	~GSpellCheck();

	GMessage::Result OnEvent(GMessage *Msg);

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