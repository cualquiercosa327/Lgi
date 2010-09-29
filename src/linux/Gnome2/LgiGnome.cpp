#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>	
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <gtk/gtksettings.h>
#include <gtk/gtkmain.h>

#define USE_GNOME	1
#define TRACE		1

char *NewStr(char *s)
{
	char *r = 0;

	if (s)
	{
		int Len = strlen(s);
		r = new char[Len+1];
		if (r)
		{
			memcpy(r, s, Len);
			r[Len] = 0;
		}
	}
	
	return r;
}

namespace Lgi
{
	#include "LgiWinManGlue.h"
};

extern "C"
{
	bool LgiWmInit(Lgi::WmInitParams *Params);
	bool LgiWmExit();

	bool LgiWmFileToMime(char *Mime, char *File);
	bool LgiWmMimeToApps(char *Mime, Lgi::GArray<Lgi::GAppInfo*> &Apps, int Limit);

	bool LgiWmGetSysFont(char *Type, char *Font, int FontLen, int &PtSize);
	bool LgiWmGetLanguage(char *Lang);
	bool LgiWmGetColour(int Which, Lgi::WmColour *Colour);
	bool LgiWmGetPath(int Which, char *Buf, int BufLen);
}

Lgi::Display *Dsp = 0;

bool LgiWmInit(Lgi::WmInitParams *Params)
{
	printf("Lgi Gnome2 add-on loaded.\n");

	if (Params)
	{
		Dsp = Params->Dsp;
	}
	
	#if USE_GNOME
	gnome_program_init
	(
		"lgi", "2.0.0", LIBGNOMEUI_MODULE,
		Params->Args, Params->Arg,
		NULL, NULL
	);
	gdk_threads_enter();
	#endif

	return true;
}

bool LgiWmExit()
{
	#if USE_GNOME
	gdk_threads_leave();
	gnome_vfs_shutdown();
	#endif
	
	// We can't let the host application unload the shared object
	// because Gnome registers handlers that run on exit, which
	// crash if this library is unloaded before exit.
	return false;
}

bool LgiWmFileToMime(char *Mime, char *File)
{
	bool Status = false;

	#if USE_GNOME
	
	#if TRACE
	printf("%s:%i - LgiWmFileToMime(%s, %s)\n", _FL, Mime, File);
	#endif
	
	if (Mime AND File)
	{
		char Full[300];
		sprintf(Full, "file://%s", Mime);
		
		char *m = gnome_vfs_get_mime_type(Full);
		if (m)
		{
			strcpy(Mime, m);
			Status = true;
		}
		else printf("%s:%i - gnome_vfs_get_mime_type failed\n", __FILE__, __LINE__);
	}
	else printf("%s:%i - error\n", __FILE__, __LINE__);
	#endif

	return Status;
}

bool LgiWmMimeToApps(char *Mime, Lgi::GArray<Lgi::GAppInfo*> &Apps, int Limit)
{
	bool Status = false;

	#if USE_GNOME

	#if TRACE
	printf("%s:%i - LgiWmMimeToApps(%s)\n", _FL, Mime);
	#endif
	
	if (Mime)
	{
		GList *lst = gnome_vfs_mime_get_all_applications(Mime);
		if (lst)
		{
			for (lst = g_list_first(lst); lst; lst = g_list_next(lst))
			{
				GnomeVFSMimeApplication *a = (GnomeVFSMimeApplication*)lst->data;
				
				Lgi::GAppInfo *i = new Lgi::GAppInfo;
				if (i)
				{
					i->Path = NewStr((char*)gnome_vfs_mime_application_get_exec(a));
					if (i->Path)
					{
						i->Name = NewStr((char*)gnome_vfs_mime_application_get_name(a));
						i->Icon = NewStr((char*)gnome_vfs_mime_application_get_icon(a));
						Apps.Add(i);
					}
					else
					{
						DeleteObj(i);
					}
				}
				
				gnome_vfs_mime_application_free(a);
			}
			
			g_list_free(lst);
			
			Status = Apps.Length() > 0;
		}
		else
		{
			printf("%s:%i - gnome_vfs_mime_get_all_applications(%s) failed\n", __FILE__, __LINE__, Mime);
		}
	}
	#endif

	return Status;
}

bool LgiWmGetSysFont(char *Type, char *Font, int FontLen, int &PtSize)
{
	bool Status = false;
	
	#if USE_GNOME

	if (Type AND Font)
	{
		GtkSettings *s = gtk_settings_get_default();
		if (s)
		{
			gchararray font = 0;
			int delta = 0;

			if (strcasecmp(Type, "font") == 0 OR
				strcasecmp(Type, "menuFont") == 0)
			{
				g_object_get(G_OBJECT(s), "gtk-font-name", &font, NULL);
			}
			else if (strcasecmp(Type, "toolBarFont") == 0)
			{
				g_object_get(G_OBJECT(s), "gtk-font-name", &font, NULL);
				delta = -1;
			}
			else if (strcasecmp(Type, "fixed") == 0)
			{
			}
			else
			{
				printf("%s:%i - '%s' not a valid font type.\n", __FILE__, __LINE__, Type);
				return false;
			}
			
			if (font)
			{
				char *s = strrchr(font, ' ');
				if (s)
				{
					s++;
					strncpy(Font, font, FontLen-1);
					Font[FontLen-1] = 0;
					PtSize = atoi(s) + delta;
					Status = true;
				}
				else printf("%s:%i - no space in '%s' getting '%s'\n", __FILE__, __LINE__, font, Type);
				
				g_free(font);
			}
			else printf("%s:%i - no font for '%s'\n", __FILE__, __LINE__, Type);
		}
		else
		{
			printf("%s:%i - gtk_settings_get_for_screen failed.\n", __FILE__, __LINE__);
		}
	}
	else printf("%s:%i - args error\n", __FILE__, __LINE__);
	#endif
	
	return Status;
}

bool LgiWmGetLanguage(char *Lang)
{
	bool Status = false;
	
	#if USE_GNOME
	if (Lang)
	{
	}
	#endif
	
	return Status;
}

bool LgiWmGetColour(int Which, Lgi::WmColour *Colour)
{
	if (NOT Colour)
		return false;

	#if 0 // USE_GNOME

	GtkSettings *s = gtk_settings_get_default();
	if (NOT s)
	{
		printf("%s:%i - gtk_settings_get_for_screen failed.\n", __FILE__, __LINE__);
		return false;
	}
	
	char PropName[] = "gtk-color-scheme";
	gchararray Value = 0;
	g_object_get(G_OBJECT(s), PropName, &Value, 0);
	printf("Value='%s'\n", Value);

	GtkStyle *style = gtk_style_new();
	if (style)
	{
		GdkColor color;

		/*		
		GHashTable ht;
		g_object_get(G_OBJECT(s), "color-hash", &ht, NULL);
		*/
		
		#if 1
		FILE *f = fopen("colours.html", "w");
		if (f)
		{
			fprintf(f, "<table>\n");
			#define Dump(var) \
				fprintf(f, "<tr>\n"); \			
				{ for (int i=0; i<5; i++) { \
				color = style->var[i]; \
				fprintf(f, "<td style='background:#%02.2x%02.2x%02.2x;'>" #var "[%i]</td>\n", \
					color.red >> 8, color.green >> 8, color.blue >> 8, i); \
				}};
			Dump(fg);
			Dump(bg);
			Dump(light);
			Dump(dark);
			Dump(mid);
			Dump(text);
			Dump(base);
			Dump(text_aa);
			fprintf(f, "</table>\n");
			fclose(f);
		}
		#endif

		if (gtk_style_lookup_color(style, "DefaultTextColor", &color))
		{
		}
		// else printf("Name doesn't exist\n");
	}
	else printf("No style\n");

	switch (Which)
	{
		case 7: // LC_MED
		{
			break;
		}
		case 10: // LC_DIALOG
		{
			break;
		}
		case 12: // LC_TEXT
		{
			break;
		}
		case 13: // LC_SELECTION
		{
			break;
		}
		case 14: // LC_SEL_TEXT
		{
			break;
		}
		case 15: // LC_ACTIVE_TITLE
		{
			break;
		}
		case 16: // LC_ACTIVE_TITLE_TEXT
		{
			break;
		}
		case 17: // LC_INACTIVE_TITLE
		{
			break;
		}
		case 18: // LC_INACTIVE_TITLE_TEXT
		{
			break;
		}

		case 0: // LC_BLACK
		case 1: // LC_DKGREY
		case 2: // LC_MIDGREY
		
		case 3: // LC_LTGREY
		case 4: // LC_WHITE
		case 5: // LC_SHADOW
		case 6: // LC_LOW
		case 8: // LC_HIGH
		case 9: // LC_LIGHT
		case 11: // LC_WORKSPACE
		case 19: // LC_MENU_BACKGROUND
		case 20: // LC_MENU_TEXT
		default:
		{
			return false;
		}
	}
	#endif
	
	return false;
}

bool LgiWmGetPath(int Which, char *Buf, int BufLen)
{
	if (NOT Buf OR BufLen < 2)
		return false;
	
	#if USE_GNOME
	switch (Which)
	{
		case Lgi::LSP_DESKTOP:
		{
			break;
		}
		case Lgi::LSP_TRASH:
		{
			break;
		}
		case Lgi::LSP_LOCAL_APP_DATA:
		{
			break;
		}
		default:
		{
			return false;
		}
	}
	#endif
	
	return false;
}
