// http://src.chromium.org/svn/trunk/src/chrome/browser/spellchecker_mac.mm
#include "Lgi.h"
#include "LgiSpellCheck.h"

#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>

enum MacSpellProps
{
	SPROP_NONE,
	SPROP_DICTIONARY,
};

static char Delim[] =
{
	' ', '\t', '\r', '\n', ',', ',', '.', ':', ';',
	'{', '}', '[', ']', '!', '@', '#', '$', '%', '^', '&', '*',
	'(', ')', '_', '-', '+', '=', '|', '\\', '/', '?', '\"',
	0
};

/*
static int PropId[] = { SPROP_DICTIONARY };
static int PropType[] = { GV_LIST };
static const char *PropName[] = { "Default Dictionary" };
*/

static OSStatus LoadFrameworkBundle(CFStringRef framework, CFBundleRef *bundlePtr)
{
    OSStatus    err;
    FSRef       frameworksFolderRef;
    CFURLRef    baseURL;
    CFURLRef    bundleURL;

    if (!bundlePtr)
		return -1;
    *bundlePtr = nil;
    baseURL = nil;
    bundleURL = nil;
    err = FSFindFolder(kOnAppropriateDisk, kFrameworksFolderType, true, &frameworksFolderRef);
    if (err == noErr) {
        baseURL = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &frameworksFolderRef);
        if (baseURL == nil)
            err = coreFoundationUnknownErr;
    }

    if (err == noErr)
	{
        bundleURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, baseURL, framework, false);
        if (bundleURL == nil)
            err = coreFoundationUnknownErr;
    }

    if (err == noErr)
	{
        *bundlePtr = CFBundleCreate(kCFAllocatorSystemDefault, bundleURL);
        if (*bundlePtr == nil)
            err = coreFoundationUnknownErr;
    }

    if (err == noErr)
	{
        if ( ! CFBundleLoadExecutable( *bundlePtr ) )
            err = coreFoundationUnknownErr;
    }

    if (err != noErr && *bundlePtr != nil)
	{
        CFRelease(*bundlePtr);
        *bundlePtr = nil;
    }

    if (bundleURL != nil)
        CFRelease(bundleURL);

    if (baseURL != nil)
        CFRelease(baseURL);

	return err;
}

typedef BOOL (*NSApplicationLoadFuncPtr)();
static BOOL InitCocoa = false;
void InitializeCocoa()
{
	
    CFBundleRef                 appKitBundleRef;
    NSApplicationLoadFuncPtr    myNSApplicationLoad;
    OSStatus                    err;

	if (InitCocoa)
		return;
	InitCocoa = true;

    err = LoadFrameworkBundle(CFSTR("AppKit.framework"), &appKitBundleRef);
    if (err != noErr)
		goto FallbackMethod;

    myNSApplicationLoad = (NSApplicationLoadFuncPtr) CFBundleGetFunctionPointerForName(appKitBundleRef, CFSTR("NSApplicationLoad"));
    if (!myNSApplicationLoad)
		goto FallbackMethod;

    (void) myNSApplicationLoad();
    return;

FallbackMethod:
    {
		NSApplication *NSApp = [NSApplication sharedApplication];
	}
}

static const unsigned int kShortLanguageCodeSize = 2;

/*
static int GVariantCmp(GVariant *a, GVariant *b, NativeInt Data)
{
	return stricmp(a->Str(), b->Str());
}
*/

int LangCmp(GSpellCheck::LanguageId *a, GSpellCheck::LanguageId *b)
{
	return stricmp(a->LangCode, b->LangCode);
}

class AppleSpellChecker :
	public GSpellCheck
{
	GViewI *Wnd;
	GString Dictionary;
	GString CurLang;
	GString::Array Dictionaries;

	GString ConvertLanguageCodeFromMac(NSString* lang_code)
	{
		// TODO(pwicks):figure out what to do about Multilingual
		// Guards for strange cases.
		// if ([lang_code isEqualToString:@"en"]) return NewStr("en-US");
		// if ([lang_code isEqualToString:@"pt"]) return NewStr("pt-PT");
		
		// NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
		
		GString s;
		if ([lang_code length] > kShortLanguageCodeSize && [lang_code characterAtIndex:kShortLanguageCodeSize] == '_')
		{
			NSString *tmp = [NSString stringWithFormat:@"%@-%@",
				[lang_code substringToIndex:kShortLanguageCodeSize],
				[lang_code substringFromIndex:(kShortLanguageCodeSize + 1)]];
			s = (char*)[tmp UTF8String];
		}
		else
		{
			s = (char*)[lang_code UTF8String];
		}
		
		// [pool release];
		
		return s;
	}

	bool ListDictionaries(List<char> &d)
	{
		// NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
		NSArray *availableLanguages = [[NSSpellChecker sharedSpellChecker] availableLanguages];
		NSEnumerator *e = [availableLanguages objectEnumerator];

		#if 1
		NSString *lang_code;
		while (lang_code = [e nextObject])
		#else
		for (NSString *lang_code in availableLanguages)
		#endif
		{
			GString lang = ConvertLanguageCodeFromMac(lang_code);
			/*if (lang)
				d.Add(lang.Release());*/
		}

		// [pool release];
		return d.Length();
	}

public:
	AppleSpellChecker() : GSpellCheck("AppleSpellChecker")
	{
		InitializeCocoa();
	}
	
	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_ENUMERATE_LANGUAGES:
			{
				int ResponseHnd = (int)Msg->A();
				GAutoPtr< GArray<LanguageId> > Langs(new GArray<LanguageId>);
				
				NSArray *availableLanguages = [[NSSpellChecker sharedSpellChecker] availableLanguages];
				NSEnumerator *e = [availableLanguages objectEnumerator];

				NSString *lang_code;
				GHashTbl<char*,bool> Map;
				while (lang_code = [e nextObject])
				{
					GString &lang = Dictionaries.New();
					lang = ConvertLanguageCodeFromMac(lang_code);
					if (lang)
					{
						ssize_t p = lang.Find("-");
						GString l = p > 0 ? lang(0, p) : lang;
						Map.Add(l, true);
					}
				}
				
				char *k;
				GAutoPtr< GArray<LanguageId> > a(new GArray<LanguageId>);
				if (!a)
					break;
				
				for (bool b = Map.First(&k); b; b = Map.Next(&k))
				{
					LanguageId &i = Langs->New();
					i.LangCode = k;
					i.EnglishName = k;
				}

				a->Sort(LangCmp);
				
				if (Langs && Langs->Length() > 0)
					PostObject(ResponseHnd, Msg->Msg(), Langs);
				break;
			}
			case M_ENUMERATE_DICTIONARIES:
			{
				int ResponseHnd = (int)Msg->A();
				GAutoPtr<GString> Lang((GString*)Msg->B());
				if (!Lang)
					break;

				GAutoPtr< GArray<DictionaryId> > Out(new GArray<DictionaryId>);
				for (unsigned i=0; i<Dictionaries.Length(); i++)
				{
					GString Dict = Dictionaries[i];
					GString::Array a = Dict.Split("-", 1);
					if (a.Length() >= 1 && a[0].Equals(*Lang))
					{
						DictionaryId &id = Out->New();
						id.Lang = *Lang;
						id.Dict = Dict.Get();
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
					char *d = Dict->Dict ? Dict->Dict : Dict->Lang;
					NSString *ln = [NSString stringWithUTF8String:d];
					Success = [[NSSpellChecker sharedSpellChecker] setLanguage:ln];
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

				for (char *s = Ct->Text; s; )
				{
					while (*s && strchr(Delim, *s))
						s++;
					char *e = s;
					while (*e && !strchr(Delim, *e))
						e++;
					if (e == s)
						break;
					GString Word(s, e - s);
					
					NSString *In = [NSString stringWithUTF8String:Word.Get()];
					NSRange spell_range = [[NSSpellChecker sharedSpellChecker]
											checkSpellingOfString:In startingAt:0
											language:nil wrap:NO inSpellDocumentWithTag:1
											wordCount:NULL];
					bool Status = (spell_range.length == 0);
					if (!Status)
					{
						SpellingError &Err = Ct->Errors.New();
						Err.Start = s - Ct->Text.Get();
						Err.Len = e - s;
						
						NSArray *guesses = [[NSSpellChecker sharedSpellChecker] guessesForWordRange:spell_range
																				inString:In
																				language:nil
																				inSpellDocumentWithTag:0];
						if (guesses)
						{
							for (NSString *g in guesses)
								Err.Suggestions.New() = [g UTF8String];
						}
					}
					
					s = *e ? e + 1 : e;
				}
				
				PostObject(ResponseHnd, Msg->Msg(), Ct);
				break;
			}
		}
		
		return 0;
	}

	/*
	bool GetValue(const char *Var, GVariant &Value)
	{
		switch (StrToDom(Var))
		{
			case SdLanguage:
			{
				// NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
				NSString *ln = [[NSSpellChecker sharedSpellChecker] language];
				if (ln)
				{
					CurLang = (char*)[ln UTF8String];
					Value = (char*)[ln UTF8String];
				}
				// [pool release];
				return true;
				break;
			}
			default:
			{
				LgiAssert(0);
				break;
			}
		}
		
		return false;
	}
	
	bool SetValue(const char *Var, GVariant &Value)
	{
		switch (StrToDom(Var))
		{
			case SdLanguage:
			{
				char *Lang = Value.Str();
				if (Lang)
				{
					// NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
					NSString *ln = [NSString stringWithUTF8String:Lang];
					[[NSSpellChecker sharedSpellChecker] setLanguage:ln];
					// [pool release];
					
					return true;
				}
				break;
			}
			default:
			{
				LgiAssert(0);
				break;
			}
		}
		
		return false;
	}
	
	bool CallMethod(const char *MethodName, GVariant *ReturnValue, GArray<GVariant*> &Args)
	{
		switch (StrToDom(MethodName))
		{
			case SdCheckWord:
			{
				if (Args.Length() > 2 && ReturnValue)
				{
					char *Word = Args[0]->CastString();
					// bool Ask = Args[1]->CastBool();
					// GView *AskWndParent = Args[2]->CastView();
					// GViewI *Parent = AskWndParent ? AskWndParent : Wnd;
					
					if (ValidStr(Word))
					{
						bool Status = false;

						// NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

						NSString *In = [NSString stringWithUTF8String:Word];
						NSRange spell_range = [[NSSpellChecker sharedSpellChecker]
												checkSpellingOfString:In startingAt:0
												language:nil wrap:NO inSpellDocumentWithTag:1
												wordCount:NULL];
						Status = (spell_range.length == 0);
						if (Status)
						{
							*ReturnValue = true;
						}
						else if (ReturnValue->SetList())
						{
							NSArray *guesses = [[NSSpellChecker sharedSpellChecker] guessesForWordRange:spell_range
																					inString:In
																					language:nil
																					inSpellDocumentWithTag:0];
							if (guesses)
							{
								for (NSString *g in guesses)
									ReturnValue->Value.Lst->Insert(new GVariant((char*)[g UTF8String]));
							}
						}

						// [pool release];

						return true;
					}
				}
				break;
			}
			case SdEnumLanguages:
			{
				if (ReturnValue && ReturnValue->SetList())
				{
					// NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
					NSArray *availableLanguages = [[NSSpellChecker sharedSpellChecker] availableLanguages];
					NSEnumerator *e = [availableLanguages objectEnumerator];

					NSString *lang_code;
					GHashTbl<char*,bool> Langs;
					while (lang_code = [e nextObject])
					{
						GAutoString lang = ConvertLanguageCodeFromMac(lang_code);
						if (lang)
						{
							char *s = strchr(lang, '-');
							if (s)
								*s = 0;
							Langs.Add(lang, true);
						}
					}
					
					char *k;
					for (bool b = Langs.First(&k); b; b = Langs.Next(&k))
					{
						char s[256];
						sprintf_s(s, sizeof(s), "%s,%s", k, k);
						ReturnValue->Value.Lst->Insert(new GVariant(s));
					}
					ReturnValue->Value.Lst->Sort(GVariantCmp, NULL);

					// [pool release];
					return true;
				}
				break;
			}
			case SdEnumDictionaries:
			{
				List<char> d;
				bool Status = false;
				if (ReturnValue && ListDictionaries(d))
				{
					if (ReturnValue->SetList())
					{
						for (char *l = d.First(); l; l = d.Next())
						{
							ReturnValue->Value.Lst->Insert(new GVariant(l));
						}
						
						Status = true;
					}
				}
				d.DeleteArrays();
				return Status;
				break;
			}
			case SdAddToDict:
			{
				LgiAssert(0);
				break;
			}
			default:
			{
				LgiAssert(0);
				break;
			}
		}
		
		return false;
	}*/
};

GAutoPtr<GSpellCheck> CreateAppleSpellCheck()
{
	GAutoPtr<GSpellCheck> a(new AppleSpellChecker());
	return a;
}
