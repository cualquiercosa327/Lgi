// http://src.chromium.org/svn/trunk/src/chrome/browser/spellchecker_mac.mm
#include "Lgi.h"
#include "ScribePlugin.h"
#include "LgiSpellCheck.h"
#include "ScribeDefs.h"

#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>

enum MacSpellProps
{
	SPROP_NONE,
	SPROP_DICTIONARY,
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

static int GVariantCmp(GVariant *a, GVariant *b, NativeInt Data)
{
	return stricmp(a->Str(), b->Str());
}

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
	
	GMessage::Result OnEvent(GMessage *m)
	{
		switch (m->Msg())
		{
			case M_ENUMERATE_LANGUAGES:
			{
				int ResponseHnd = (int)m->A();
				if (ResponseHnd < 0)
					break;
				
				NSArray *availableLanguages = [[NSSpellChecker sharedSpellChecker] availableLanguages];
				NSEnumerator *e = [availableLanguages objectEnumerator];

				NSString *lang_code;
				GHashTbl<char*,bool> Langs;
				while (lang_code = [e nextObject])
				{
					GString lang = ConvertLanguageCodeFromMac(lang_code);
					if (lang)
					{
						int p = lang.Find("-");
						if (p > 0)
							lang.Length(p);
						Langs.Add(lang, true);
					}
				}
				
				char *k;
				GAutoPtr< GArray<LanguageId> > a(new GArray<LanguageId>);
				if (!a)
					break;
				
				for (bool b = Langs.First(&k); b; b = Langs.Next(&k))
				{
					LanguageId &i = a->New();
					i.LangCode = k;
					i.EnglishName = k;
				}

				a->Sort(LangCmp);
				
				PostThreadEvent(ResponseHnd, M_ENUMERATE_LANGUAGES, (GMessage::Param) a.Release() );
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
	}
	*/
};

GAutoPtr<GSpellCheck> CreateAppleSpellCheck()
{
	GAutoPtr<GSpellCheck> a(new AppleSpellChecker());
	return a;
}
