// Clipboard Implementation
#include "Lgi.h"
#include "GVariant.h"
#include "GClipBoard.h"

#define DEBUG_CLIPBOARD		0

using namespace Gtk;

class GClipBoardPriv
{
public:
    GtkClipboard *c;
};

///////////////////////////////////////////////////////////////////////////////////////////////
GClipBoard::GClipBoard(GView *o)
{
	d = new GClipBoardPriv;
	Owner = o;
	Open = false;
	pDC = 0;
	d->c = gtk_clipboard_get(GDK_NONE); // gdk_atom_intern("CLIPBOARD", false)
	#if DEBUG_CLIPBOARD
	printf("d->c = %i\n", d->c);
	#endif
}

GClipBoard::~GClipBoard()
{
	d->c = 0;
	DeleteObj(d);
}

bool GClipBoard::Empty()
{
    if (d->c)
    {
        gtk_clipboard_clear(d->c);
        #if DEBUG_CLIPBOARD
        printf("gtk_clipboard_clear(%i)\n", d->c);
        #endif
        return true;
    }

	return false;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	
	if (Str && d->c)
	{
		gtk_clipboard_set_text(d->c, Str, strlen(Str));
		#if DEBUG_CLIPBOARD
        printf("gtk_clipboard_set_text(%i,%s,%i)\n", d->c, Str, strlen(Str));
        #endif
		Status = true;
	}

	return Status;
}

char *GClipBoard::Text()
{
    char *t = 0;
    
    if (d->c)
    {
        gchar *txt = gtk_clipboard_wait_for_text(d->c);
        #if DEBUG_CLIPBOARD
        printf("gtk_clipboard_wait_for_text(%i)='%s'\n", d->c, txt);
        #endif
        if (txt)
        {
            t = NewStr(txt);
            g_free(txt);
        }
    }
	
	return t;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
    GAutoString u(LgiNewUtf16To8(Str));
    return Text(u, AutoEmpty);
}

char16 *GClipBoard::TextW()
{
    GAutoString u(Text());
    return LgiNewUtf8To16(u);
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC && d->c)
	{
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	return pDC;
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, int Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr && Len > 0)
	{
	}

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, int *Len)
{
	bool Status = false;

	if (Ptr && Len)
	{
	}

	return Status;
}

