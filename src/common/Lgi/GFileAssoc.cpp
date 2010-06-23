#include <stdio.h>

#include "Lgi.h"
#include "GFileAssoc.h"

/////////////////////////////////////////////////////////////////////
GFileAssocAction::GFileAssocAction()
{
	App = 0;
	Action = 0;
}

GFileAssocAction::~GFileAssocAction()
{
	DeleteArray(App);
	DeleteArray(Action);
}

/////////////////////////////////////////////////////////////////////
class GFileAssocPrivate
{
public:
	char *Mime;
	char *Ext;

	#ifdef WIN32
	char *Class;
	#endif

	GFileAssocPrivate(char *mime, char *ext)
	{
		Mime = NewStr(mime);
		Ext = NewStr(ext);

		#ifdef WIN32
		Class = 0;

		if (Mime AND !Ext)
		{
			char *d = strrchr(Mime, '/');
			if (d)
			{
				if (d[1] == '.')
				{
					Ext = NewStr(d + 1);
				}
				else
				{
					GRegKey k("HKEY_CLASSES_ROOT\\MIME\\Database\\Content Type\\%s", Mime);
					char *e = k.GetStr("Extension");
					if (e)
					{
						Ext = NewStr(e);
					}
				}
			}

			GRegKey k("HKEY_CLASSES_ROOT\\%s", Ext);
			Class = NewStr(k.GetStr());
		}
		else if (Ext AND !Mime)
		{
			GRegKey k("HKEY_CLASSES_ROOT\\%s", Ext);
			char *m = k.GetStr("Content Type");
			if (m)
			{
				Mime = NewStr(m);
			}
			else
			{
				char s[256];
				sprintf(s, "application/%s", Ext);
				Mime = NewStr(s);
			}
			Class = NewStr(k.GetStr());
		}
		#endif
	}

	~GFileAssocPrivate()
	{
		DeleteArray(Mime);
		DeleteArray(Ext);

		#ifdef WIN32
		DeleteArray(Class);
		#endif
	}
};

GFileAssoc::GFileAssoc(char *mime, char *ext)
{
	d = new GFileAssocPrivate(mime, ext);
}

GFileAssoc::~GFileAssoc()
{
	DeleteObj(d);
}

char *GFileAssoc::GetMimeType()
{
	return d->Mime;
}

char *GFileAssoc::GetExtension()
{
	return d->Ext;
}

bool GFileAssoc::GetExtensions(GArray<char*> &Ext)
{
	bool Status = false;

	#ifdef WIN32

	if (d->Ext)
	{
		Ext.Add(NewStr(d->Ext + 1));
		Status = true;
	}
	
	#endif

	return Status;
}

bool GFileAssoc::GetActions(GArray<GFileAssocAction*> &Actions)
{
	bool Status = false;

	#ifdef WIN32

	if (d->Class)
	{
		GRegKey k("HKEY_CLASSES_ROOT\\%s\\shell", d->Class);
		List<char> a;
		if (k.GetKeyNames(a))
		{
			for (char *s = a.First(); s; s = a.Next())
			{
				GFileAssocAction *Act = new GFileAssocAction;
				if (Act)
				{
					Actions.Add(Act);
					Act->Action = s;
					Status = true;

					GRegKey n("HKEY_CLASSES_ROOT\\%s\\shell\\%s\\command", d->Class, s);
					Act->App = NewStr(n.GetStr());
				}
			}
		}
	}
	
	#endif

	return Status;
}

bool GFileAssoc::SetAction(GFileAssocAction *Action)
{
	bool Status = false;

	#ifdef WIN32

	if (Action)
	{
		GRegKey k("HKEY_CLASSES_ROOT\\%s\\shell\\%s\\command", d->Class, Action->Action);
		if (!k.IsOk())
		{
			k.Create();
		}
		k.SetStr(0, Action->App);
		Status = true;
	}
	
	#endif

	return Status;
}

bool GFileAssoc::SetIcon(char *File, int Index)
{
	bool Status = false;

	#ifdef WIN32

	if (File)
	{
		GRegKey k("HKEY_CLASSES_ROOT\\%s\\DefaultIcon", d->Class);
		if (!k.IsOk())
		{
			k.Create();
		}

		char s[300];
		sprintf(s, "%s,%i", File, Index);
		k.SetStr(0, s);
		Status = true;
	}
	
	#endif

	return Status;
}
