// Mac Implementation of General LGI functions
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// #define _POSIX_TIMERS
#include <time.h>

#include "Lgi.h"
#include "GProcess.h"
#include "GTextLabel.h"
#include "GButton.h"
#include <Carbon/Carbon.h>
#include "INet.h"

#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>


#import <Cocoa/Cocoa.h>

////////////////////////////////////////////////////////////////
// Local helper functions
bool _lgi_check_file(char *Path)
{
	if (Path)
	{
		if (FileExists(Path))
		{
			// file is there
			return true;
		}
		else
		{
			// shortcut?
			char *e = Path + strlen(Path);
			strcpy(e, ".lnk");
			if (FileExists(Path))
			{
				// resolve shortcut
				char Link[256];
				if (ResolveShortcut(Path, Link, sizeof(Link)))
				{
					// check destination of link
					if (FileExists(Link))
					{
						strcpy(Path, Link);
						return true;
					}
				}
			}
			*e = 0;
		}
	}

	return false;
}

void LgiSleep(uint32 i)
{
	struct timespec request, remain;

	ZeroObj(request);
	ZeroObj(remain);
	
	request.tv_sec = i / 1000;
	request.tv_nsec = (i % 1000) * 1000000;

	while (nanosleep(&request, &remain) == -1)
	{
	    request = remain;
	}
}

char *p2c(unsigned char *s)
{
	if (s)
	{
		s[1+s[0]] = 0;
		return (char*)s + 1;
	}
	return 0;
}

void c2p255(Str255 &d, const char *s)
{
	if (s)
	{
		int Len = strlen(s);
		if (Len > 255) Len = 255;
		d[0] = Len;
		for (int i=0; i<Len; i++)
			d[1+i] = s[i];
	}
}


/*
Boolean AssertProc(DialogRef theDialog, EventRecord *theEvent, DialogItemIndex *itemHit)
{
	printf("what=%i\n", theEvent->what);
	switch (theEvent->what)
	{
		case mouseDown:
		{
			int asd=0;
			break;
		}
	}
	
	return true;
}
*/

void _lgi_assert(bool b, const char *test, const char *file, int line)
{
	static bool Asserting = false;

	if (!b && !Asserting)
	{
		Asserting = true;
		printf("%s:%i - Assert failed:\n%s\n", file, line, test);

		#ifdef _DEBUG

		GString p;
		p.Printf("Assert failed, file: %s, line: %i\n%s", file, line, test);

		int Result = 0;
		
		#if 0
		if (LgiApp->AppWnd)
		{
			LgiApp->AppWnd->PostEvent(	M_ASSERT_DLG,
										(GMessage::Param)&Result,
										(GMessage::Param)new GString(p));
			while (Result == 0)
				LgiSleep(10);
		}
		#else

		SInt16 r;
		Str255 t;
		Str255 s;
		Str63 sAbort = {5, 'A', 'b', 'o', 'r', 't'},
			  sBreak = {5, 'B', 'r', 'e', 'a', 'k'},
			  sIgnore = {6, 'I', 'g', 'n', 'o', 'r', 'e'};
		AlertStdAlertParamRec Params;

		c2p255(t, "Assert");
		c2p255(s, p.Get());
		Params.movable = true;
		Params.helpButton = false;
		Params.filterProc = 0;

		Params.defaultButton = kAlertStdAlertOKButton;
		Params.defaultText = sAbort;

		Params.cancelButton = kAlertStdAlertCancelButton;
		Params.cancelText = sIgnore;

		Params.otherText = sBreak;

		Params.position = kWindowDefaultPosition;
		
		// Params.filterProc = AssertProc;
		
		StandardAlert(kAlertStopAlert, t, s, &Params, &r);

		if (r == kAlertStdAlertOKButton)
			Result = 1;
		else if (r == kAlertStdAlertOtherButton)
			Result = 2;
		else
			Result = 3;
		
		#endif

		switch (Result)
		{
			default:
			{
				exit(-1);
				break;
			}
			case 2:
			{
				// Crash here to bring up the debugger...
				int *p = 0;
				*p = 0;
				break;
			}
			case 3:
			{
				break;
			}
		}

		#endif

		Asserting = false;
	}
}

////////////////////////////////////////////////////////////////////////
// Implementations
GMessage CreateMsg(int m, GMessage::Param a, GMessage::Param b)
{
	static class GMessage Msg(0);
	Msg.Set(m, a, b);
	return Msg;
}

OsView DefaultOsView(GView *v)
{
	return 0;
}

GString LGetFileMimeType(const char *File)
{
	GAutoString m = LgiApp->GetFileMimeType(File);
	if (!m)
		return GString();

	return m.Get();
}

#include "GToken.h"
bool _GetIniField(char *Grp, char *Field, char *In, char *Out, int OutSize)
{
	if (ValidStr(In))
	{
		bool InGroup = false;
		GToken t(In, "\r\n");
		
		for (int i=0; i<t.Length(); i++)
		{
			char *Line = t[i];
			if (*Line == '[')
			{
				// Reset inside group at the start of each group
				InGroup = false;

				char *e = strchr(++Line, ']');
				if (e)
				{
					*e = 0;
					InGroup = stricmp(Grp, Line) == 0;
				}
			}
			else if (InGroup)
			{
				char *e = strchr(Line, '=');
				if (e)
				{
					// Point v to the data after the equals
					// Leave e pointing to the byte before
					char *v = e-- + 1;

					// Seek e through any whitespace between the equals and the
					// field name
					while (e > Line && strchr(" \t", *e)) e--;
					
					// Seek v through any whitespace after the equals
					while (*v && strchr(" \t", *v)) v++;
					
					// Calculate the length of the field
					int flen = (int)e-(int)Line;

					// Check the current field against the input field
					if (strnicmp(Field, Line, flen) == 0)
					{
						while (	*v &&
								strchr("\r\n", *v) == 0 &&
								OutSize > 1) // leave space for NULL
						{
							*Out++ = *v++;
							OutSize--;
						}

						// Add the NULL
						*Out++ = 0;
						
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

bool LgiGetAppsForMimeType(const char *Mime, GArray<GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	if (Mime)
	{
	}

	return Status;
}

GString LGetAppForMimeType(const char *Mime)
{
	GArray<GAppInfo*> Apps;
	if (LgiGetAppsForMimeType(Mime, Apps, 1))
		return Apps[0]->Path.Get();
	return GString();
}

int LgiRand(int Limit)
{
	return rand() % Limit;
}

bool LgiPlaySound(const char *FileName, int ASync)
{
	return LgiExecute(FileName);
}

OSErr FinderLaunch(long nTargets, FSRef *targetList)
{
	OSErr err;
	
	AppleEvent theAEvent, theReply;
	AEAddressDesc fndrAddress;
	AEDescList targetListDesc;
	OSType fndrCreator;
	AliasHandle targetAlias;
	long index;

	/* verify parameters */
	if ((nTargets == 0) || (targetList == NULL))
		return paramErr;

	/* set up locals  */
	AECreateDesc(typeNull, NULL, 0, &theAEvent);
	AECreateDesc(typeNull, NULL, 0, &fndrAddress);
	AECreateDesc(typeNull, NULL, 0, &theReply);
	AECreateDesc(typeNull, NULL, 0, &targetListDesc);
	targetAlias = NULL;
	fndrCreator = 'MACS';

	/* create an open documents event targeting the finder */
	err = AECreateDesc(typeApplSignature, (Ptr) &fndrCreator,
	sizeof(fndrCreator), &fndrAddress);

	if (err == noErr)
	{
		err = AECreateAppleEvent(kCoreEventClass,
								kAEOpenDocuments,
								&fndrAddress,
								kAutoGenerateReturnID,
								kAnyTransactionID,
								&theAEvent);
	}


	if (err == noErr)
	{
		/* create the list of files to open */
		err = AECreateList(NULL, 0, false, &targetListDesc);
	}

	if (err == noErr)
	{
		for (index=0; index < nTargets; index++)
		{
			if (targetAlias == NULL)
			{
				// err = NewAlias(NULL, (targetList + index), &targetAlias);
				err = FSNewAlias(NULL, (targetList + index), &targetAlias);
			}
			else
			{
				Boolean Changed;
				err = FSUpdateAlias(NULL, (targetList + index), targetAlias, &Changed);
			}

			if (err == noErr)
			{
				HLock((Handle) targetAlias);
				err = AEPutPtr(	&targetListDesc,
								(index + 1),
								typeAlias,
								*targetAlias, 
								GetHandleSize((Handle) targetAlias));
								HUnlock((Handle) targetAlias);
			}
		}
	}

	/* add the file list to the apple event */
	if( err == noErr )
	{
		err = AEPutParamDesc(&theAEvent, keyDirectObject, &targetListDesc);
	}
	
	if (err == noErr)
	{
		/* send the event to the Finder */
		err = AESend(&theAEvent,
					&theReply,
					kAENoReply,
					kAENormalPriority,
					kAEDefaultTimeout,
					NULL,
					NULL);
	}

	/* clean up and leave */
	if (targetAlias != NULL)
		DisposeHandle((Handle) targetAlias);
	AEDisposeDesc(&targetListDesc);
	AEDisposeDesc(&theAEvent);
	AEDisposeDesc(&fndrAddress);
	AEDisposeDesc(&theReply);

	return err;
}

GAutoString LgiErrorCodeToString(uint32 ErrorCode)
{
    const char *e = strerror(ErrorCode);
	static char tmp[32];
	if (!e)
	{
		sprintf_s(tmp, sizeof(tmp), "Error(%i)", ErrorCode);
		e = tmp;
	}
	
    return GAutoString(NewStr(e));
}

bool LgiExecute(const char *File, const char *Args, const char *Dir, GAutoString *ErrorMsg)
{
	bool Status = false;
	
	if (File)
	{
		GUri uri(File);
		
		if (uri.Protocol)
		{
			CFStringRef s = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)File, strlen(File), kCFStringEncodingUTF8, false);
			if (s)
			{
				CFURLRef u = CFURLCreateWithString(NULL, s, NULL);
				if (u)
				{
					Status = LSOpenCFURLRef(u, 0) == 0;
					CFRelease(u);
				}
				CFRelease(s);
			}
		}
		else
		{
			FSRef r;
			OSStatus e = FSPathMakeRef((UInt8*)File, &r, NULL);
			char Path[MAX_PATH];
			
			if (e)
			{
				// Check if this is an application
				snprintf(Path, sizeof(Path), "/Applications/%s", File);
				e = FSPathMakeRef((UInt8*)Path, &r, NULL);
				if (!e)
					File = Path;
			}
			
			if (e) printf("%s:%i - FSPathMakeRef failed with %i\n", _FL, (int)e);
			else
			{
				// Is this an app bundle?
				bool IsAppBundle = false;
				const char *Last = strrchr(File, '/');
				if (Last)
				{
					const char *Dot = strrchr(Last, '.');
					IsAppBundle = Dot && !stricmp(Dot, ".app");
					
					/* Ideally in OSX before 10.6 we'd convert to calling the executable in
					 the bundle rather the 'open' the app bundle, because open doesn't 
					 support arguments before 10.6
					 if (ValidStr(Args))
					 {
						 GArray<int> Ver;
						 LgiGetOs(Ver);
						 if (Ver.Length() > 1)
						 {
							 if (Ver[0] < 10 ||
								 Ver[1] < 6)
							 {
								 IsAppBundle = false;
							 }
						 }
					 }
					 */
				}
				
				struct stat s;
				int st = stat(File, &s);
				
				if (IsAppBundle)
				{
					char cmd[512];
					if (ValidStr((char*)Args))
						snprintf(cmd, sizeof(cmd), "open -a \"%s\" %s", File, Args);
					else
						snprintf(cmd, sizeof(cmd), "open -a \"%s\"", File);
					system(cmd);
				}
				else if (st == 0 &&
						 S_ISREG(s.st_mode) &&
						 (s.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
				{
					// This is an executable file
					if (!fork())
					{
						if (Dir)
							chdir(Dir);
						
						GArray<const char*> a;
						a.Add(File);
						
						char *p;
						while ((p = LgiTokStr(Args)))
						{
							a.Add(p);
						}
						a.Add(0);
						
						char *env[] = {0};
						execve(File, (char*const*)&a[0], env);
					}
					
					return true;
				}
				else
				{
					// Document
					e = FinderLaunch(1, &r);
					if (e) printf("%s:%i - FinderLaunch faied with %i\n", _FL, (int)e);
					else Status = true;
				}
			}
		}
	}
	
	return Status;
}

CFStringRef Utf8ToCFString(char *s, int len)
{
	if (s && len < 0)
		len = strlen(s);
	return CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)s, len, kCFStringEncodingUTF8, false);
}

char *CFStringToUtf8(CFStringRef r)
{
	if (r == NULL)
		return 0;

	char *Buffer = 0;
	CFRange g = { 0, CFStringGetLength(r) };
	CFIndex Used;

	if (CFStringGetBytes(r,
						g,
						kCFStringEncodingUTF8,
						0,
						false,
						0,
						0,
						&Used))
	{
		if ((Buffer = new char[Used+1]))
		{
			CFStringGetBytes(r,
							g,
							kCFStringEncodingUTF8,
							0,
							false,
							(UInt8*)Buffer,
							Used,
							&Used);

			Buffer[Used] = 0;
		}
	}
	
	return Buffer;
}

bool LgiGetMimeTypeExtensions(const char *Mime, GArray<GString> &Ext)
{
	int Start = Ext.Length();

	#define HardCodeExtention(chk, Ext1, Ext2) \
		else if (!stricmp(chk, Mime)) \
		{	if (Ext1) Ext.Add(Ext1); \
			if (Ext2) Ext.Add(Ext2); }

	if (!Mime);
	HardCodeExtention("text/calendar", "ics", 0)
	HardCodeExtention("text/x-vcard", "vcf", 0)
	HardCodeExtention("text/mbox", "mbx", "mbox")
	HardCodeExtention("application/pdf", "pdf", 0)
	HardCodeExtention("image/jpg", "jpg", 0)
	HardCodeExtention("image/jpeg", "jpg", 0)
	HardCodeExtention("image/png", "png", 0)
	else
		LgiTrace("%s:%i - Unknown mime type '%s'\n", _FL, Mime);

	return Ext.Length() > Start;
}

GString LgiCurrentUserName()
{
    struct passwd *pw = getpwuid(geteuid());
    if (pw)
        return pw->pw_name;
    
    return "";
}
