/// \file
/// \author Matthew Allen
/// \brief Native mac file open dialog.
#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GToken.h"

//////////////////////////////////////////////////////////////////////////
// This is just a private data container to make it easier to change the
// implementation of this class without effecting headers and applications.
class GFileSelectPrivate
{
	friend class GFileSelect;
	friend class GFileSelectDlg;
	friend class GFolderList;

	GView *Parent;
	GFileSelect *Select;

	char *Title;
	char *DefExt;
	bool MultiSelect;
	List<char> Files;
	int CurrentType;
	List<GFileType> Types;
	List<char> History;
	bool ShowReadOnly;
	bool ReadOnly;
	
	bool EatClose;

public:
	static char *InitPath;
	static bool InitShowHiddenFiles;
	static GRect InitSize;

	GFileSelectPrivate(GFileSelect *select)
	{
		ShowReadOnly = false;
		ReadOnly = false;
		EatClose = false;
		Select = select;
		Title = 0;
		DefExt = 0;
		MultiSelect = false;
		Parent = 0;
		CurrentType = -1;
	}

	virtual ~GFileSelectPrivate()
	{
		DeleteArray(Title);
		DeleteArray(DefExt);

		Types.DeleteObjects();
		Files.DeleteArrays();
		History.DeleteArrays();
	}

	Boolean FilterProc(AEDesc * theItem,
						NavFilterModes filterMode)
	{
		FSRef fs;
		if (!AEGetDescData(theItem, &fs, sizeof(fs)))
		{
			UInt8 path[300];
			FSRefMakePath(&fs, path, sizeof(path));
			
			if (DirExists((char*)path))
				return true;
				
			char *Dir = strrchr((char*)path, '/');
			if (Dir)
			{
				Dir++;
				
				for (GFileType *t = Types.First(); t; t = Types.Next())
				{
					char *e = t->Extension();
					if (e)
					{
						if (MatchStr(e, Dir))
							return true;
					}
				}
				
				return false;
			}
		}
		
		return true;
	}

	void DoTypes(NavDialogRef Dlg)
	{
		if (Types.Length())
		{
			int Len = 0;
			for (GFileType *t = Types.First(); t; t = Types.Next())
			{
				if (t->Extension())
				{
					if (stricmp(t->Extension(), LGI_ALL_FILES))
						Len++;
				}
			}

			if (Len)
			{
				CFMutableArrayRef identifiers = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				if (identifiers)
				{
					for (GFileType *t = Types.First(); t; t = Types.Next())
					{
						char *Ext = t->Extension();
						GToken e(Ext, ";");
						for (int i=0; i<e.Length(); i++)
						{
							char *Dot = strrchr(e[i], '.');
							if (Dot)
							{
								Dot++;
														
								CFStringRef s = CFStringCreateWithBytes(NULL, (UInt8*)Dot, strlen(Dot), kCFStringEncodingUTF8, false);
								CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
																						s,
																						kUTTypeData);
								if (uti)
								{
									CFArrayAppendValue(identifiers, uti);
								}
								else printf("%s:%i - UTTypeCreatePreferredIdentifierForTag failed\n");
								
								CFRelease(s);
							}
						}
					}

					OSStatus e = NavDialogSetFilterTypeIdentifiers(Dlg, identifiers);
					if (e) printf("%s:%i - NavDialogSetFilterTypeIdentifiers failed with %i\n", _FL, e);
				}
				else printf("%s:%i - CFArrayCreateMutable failed\n");
			}
		}
	}

	bool DoReply(NavDialogRef Dlg, bool Save = false)
	{
		bool Status = false;
		
		NavReplyRecord r;
		OSStatus e = NavDialogGetReply(Dlg, &r);
		if (e) printf("%s:%i - NavDialogGetReply failed with %i\n", _FL, e);
		else if (Status = r.validRecord)
		{
			long len = 0;
			e = AECountItems(&r.selection, &len);
			if (!e)
			{
				for (int i=0; i<len; i++)
				{
					FSRef fs;
					AEKeyword theAEKeyword;
					DescType typeCode;
					Size size;
					e = AEGetNthPtr(&r.selection, i+1, typeFSRef, &theAEKeyword, &typeCode, &fs, sizeof(fs), &size);
					if (e) printf("%s:%i - AEGetNthPtr(%i) failed with %i\n", _FL, i, e);
					else
					{
						UInt8 path[300];
						e = FSRefMakePath(&fs, path, sizeof(path));
						
						if (Save)
						{
							char *Buffer = CFStringToUtf8(r.saveFileName);
							if (Buffer)
							{
								LgiMakePath((char*)path,
											sizeof(path),
											(char*)path,
											(char*)Buffer);
								DeleteArray(Buffer);
							}
						}
						
						if (!e)
						{
							Files.DeleteArrays();
							Files.Insert(NewStr((char*)path));
						}
					}
				}
			}						
		}
		
		return Status;
	}
};

char *GFileSelectPrivate::InitPath = 0;

//////////////////////////////////////////////////////////////////////////
GFileSelect::GFileSelect()
{
	d = new GFileSelectPrivate(this);
}

GFileSelect::~GFileSelect()
{
	DeleteObj(d);
}

void GFileSelect::ShowReadOnly(bool b)
{
	d->ShowReadOnly = b;;
}

bool GFileSelect::ReadOnly()
{
	return d->ReadOnly;
}

char *GFileSelect::Name()
{
	return d->Files.First();
}

bool GFileSelect::Name(char *n)
{
	d->Files.DeleteArrays();
	if (n)
	{
		d->Files.Insert(NewStr(n));
	}

	return true;
}

char *GFileSelect::operator [](int i)
{
	return d->Files.ItemAt(i);
}

int GFileSelect::Length()
{
	return d->Files.Length();
}

int GFileSelect::Types()
{
	return d->Types.Length();
}

void GFileSelect::ClearTypes()
{
	d->Types.DeleteObjects();
}

GFileType *GFileSelect::TypeAt(int n)
{
	return d->Types.ItemAt(n);
}

bool GFileSelect::Type(char *Description, char *Extension, int Data)
{
	GFileType *Type = new GFileType;
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		d->Types.Insert(Type);
	}

	return Type != 0;
}

int GFileSelect::SelectedType()
{
	return d->CurrentType;
}

GViewI *GFileSelect::Parent()
{
	return d->Parent;
}

void GFileSelect::Parent(GViewI *Window)
{
	d->Parent = dynamic_cast<GView*>(Window);
}

bool GFileSelect::MultiSelect()
{
	return d->MultiSelect;
}

void GFileSelect::MultiSelect(bool Multi)
{
	d->MultiSelect = Multi;
}

#define CharPropImpl(Func, Var)				\
	char *GFileSelect::Func()				\
	{										\
		return Var;							\
	}										\
	void GFileSelect::Func(char *i)			\
	{										\
		DeleteArray(Var);					\
		if (i)								\
		{									\
			Var = NewStr(i);				\
		}									\
	}

CharPropImpl(InitialDir, d->InitPath);
CharPropImpl(Title, d->Title);
CharPropImpl(DefaultExtension, d->DefExt);

void Lgi_NavEventProc(	NavEventCallbackMessage callBackSelector,
						NavCBRecPtr callBackParms,
						void * callBackUD)
{
	GFileSelectPrivate *d = (GFileSelectPrivate*) callBackUD;
	switch (callBackSelector)
	{
		case kNavCBStart:
		{
			if (d->InitPath)
			{
				OSStatus e = noErr;
				AEDesc theLocation = {typeNull, NULL};
				FSRef Ref;
				e = FSPathMakeRef((uchar*) d->InitPath, &Ref, 0);
				if (e) printf("%s:%i - FSPathMakeRef failed.\n", _FL);
				
				FSSpec Spec;
				FSGetCatalogInfo(&Ref, kFSCatInfoNone, 0, 0, &Spec, 0);

				e = AECreateDesc(typeFSS, &Spec, sizeof(FSSpec), &theLocation);
				if (e) printf("%s:%i - FSMakeFSSpec failed.\n", _FL);
				e = NavCustomControl(callBackParms->context,
									kNavCtlSetLocation, (void*)&theLocation);
				if (e) printf("%s:%i - FSMakeFSSpec failed.\n", _FL);
			}
			break;
		}
	}
}

Boolean Lgi_NavObjectFilterProc(AEDesc * theItem,
								void *info,
								void *callBackUD,
								NavFilterModes filterMode)
{
	GFileSelectPrivate *d = (GFileSelectPrivate*) callBackUD;
	return d->FilterProc(theItem, filterMode);
}
   
bool GFileSelect::Open()
{
	bool Status = false;
	
	NavDialogCreationOptions o;
	ZeroObj(o);
	OSStatus e = NavGetDefaultDialogCreationOptions(&o);
	if (e) printf("%s:%i - NavGetDefaultDialogCreationOptions failed with %i\n", _FL, e);
	else
	{
		o.modality = kWindowModalityAppModal;
		if (MultiSelect())
			o.optionFlags |= kNavAllowMultipleFiles;
		
		NavEventUPP EventUPP = NewNavEventUPP(Lgi_NavEventProc);
		NavObjectFilterUPP ObjectUPP = NewNavObjectFilterUPP(Lgi_NavObjectFilterProc);
		NavDialogRef Dlg = 0;
		e = NavCreateGetFileDialog(	&o,
									0, // NavTypeListHandle inTypeList,
									EventUPP,
									0, // NavPreviewUPP inPreviewProc,
									0, // ObjectUPP,
									d,
									&Dlg);
		if (e) printf("%s:%i - NavCreateGetFileDialog failed with %i\n", _FL, e);
		else
		{
			d->DoTypes(Dlg);
			
			e = NavDialogRun(Dlg);
			if (e) printf("%s:%i - NavDialogRun failed with %i\n", _FL, e);
			else
			{
				Status = d->DoReply(Dlg);
			}
			
			NavDialogDispose(Dlg);
		}

		DisposeNavEventUPP(EventUPP);
		DisposeNavObjectFilterUPP(ObjectUPP);
	}

	return Status;
}

bool GFileSelect::OpenFolder()
{
	bool Status = false;
	
	NavDialogCreationOptions o;
	ZeroObj(o);
	OSStatus e = NavGetDefaultDialogCreationOptions(&o);
	if (e) printf("%s:%i - NavGetDefaultDialogCreationOptions failed with %i\n", _FL, e);
	else
	{
		o.modality = kWindowModalityAppModal;
		
		NavEventUPP EventUPP = NewNavEventUPP(Lgi_NavEventProc);
		NavObjectFilterUPP ObjectUPP = NewNavObjectFilterUPP(Lgi_NavObjectFilterProc);
		NavDialogRef Dlg = 0;
		e = NavCreateChooseFolderDialog(&o,
										EventUPP,
										0, // ObjectUPP,
										d,
										&Dlg);
		if (e) printf("%s:%i - NavCreateGetFileDialog failed with %i\n", _FL, e);
		else
		{
			d->DoTypes(Dlg);
			
			e = NavDialogRun(Dlg);
			if (e) printf("%s:%i - NavDialogRun failed with %i\n", _FL, e);
			else
			{
				Status = d->DoReply(Dlg);
			}
			
			NavDialogDispose(Dlg);
		}

		DisposeNavEventUPP(EventUPP);
		DisposeNavObjectFilterUPP(ObjectUPP);
	}

	return Status;
}

bool GFileSelect::Save()
{
	bool Status = false;
	
	NavDialogCreationOptions o;
	ZeroObj(o);
	OSStatus e = NavGetDefaultDialogCreationOptions(&o);
	if (e) printf("%s:%i - NavGetDefaultDialogCreationOptions failed with %i\n", _FL, e);
	else
	{
		o.modality = kWindowModalityAppModal;
		
		NavEventUPP EventUPP = NewNavEventUPP(Lgi_NavEventProc);
		NavObjectFilterUPP ObjectUPP = NewNavObjectFilterUPP(Lgi_NavObjectFilterProc);
		NavDialogRef Dlg = 0;
		e = NavCreatePutFileDialog(	&o,
									'type',
									'lgi ',
									EventUPP,
									d,
									&Dlg);
		if (e) printf("%s:%i - NavCreatePutFileDialog failed with %i\n", _FL, e);
		else
		{
			d->DoTypes(Dlg);
			
			e = NavDialogRun(Dlg);
			if (e) printf("%s:%i - NavDialogRun failed with %i\n", _FL, e);
			else
			{
				Status = d->DoReply(Dlg, true);
			}
			
			NavDialogDispose(Dlg);
		}

		DisposeNavEventUPP(EventUPP);
		DisposeNavObjectFilterUPP(ObjectUPP);
	}

	return Status;
}

