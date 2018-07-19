// Clipboard Implementation
#include "Lgi.h"
#include "GVariant.h"
#include "GClipBoard.h"

#define DEBUG_CLIPBOARD					0
#define VAR_COUNT						16
#define LGI_CLIP_BINARY					"lgi.binary"
#define LGI_RECEIVE_CLIPBOARD_TIMEOUT	4000

using namespace Gtk;

struct ClipData : public LMutex
{
	::GVariant v[VAR_COUNT];
	
	ClipData() : LMutex("ClipData")
	{
	}
	
}	Data;

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
	if (d->c)
		Open = true;
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
	GAutoString u(WideToUtf8(Str));
	return Text(u, AutoEmpty);
}

char16 *GClipBoard::TextW()
{
	GAutoString u(Text());
	return Utf8ToWide(u);
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC && d->c)
	{
		GMemDC *Mem = dynamic_cast<GMemDC*>(pDC);
		if (Mem)
		{
			/*
			GdkPixbuf *pb = gdk_pixbuf_new_from_data (	const guchar *data,
														GDK_COLORSPACE_RGB,
														gboolean has_alpha,
														int bits_per_sample,
														int width,
														int height,
														int rowstride,
														GdkPixbufDestroyNotify destroy_fn,
														gpointer destroy_fn_data);
			// gtk_clipboard_set_image(d->c, pb);
			*/
		}
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	return pDC;
}

void LgiClipboardGetFunc(GtkClipboard *clipboard,
                        GtkSelectionData *data,
                        guint info,
                        gpointer user_data)
{
	if (Data.Lock(_FL))
	{
		::GVariant *p = (::GVariant*)user_data;
		#if DEBUG_CLIPBOARD
		printf("%s:%i - LgiClipboardGetFunc: %p, %i\n", _FL, p, info);
		#endif

		switch (info)
		{
			case GV_BINARY:
			{
				if (p->Type == info)
				{
					data->data = p->Value.Binary.Data;
					data->length = p->Value.Binary.Length;
				}
				else LgiTrace("%s:%i - Variant is the wrong type: %i\n", _FL, p->Type);
				break;
			}
			default:
			{
				LgiTrace("%s:%i - Undefined data type: %i\n", _FL, info);
				break;
			}
		}
		
		Data.Unlock();
	}
}

void LgiClipboardClearFunc(GtkClipboard *clipboard,
                          gpointer user_data)
{
	if (Data.Lock(_FL))
	{
		::GVariant *p = (::GVariant*)user_data;
		#if DEBUG_CLIPBOARD
		printf("%s:%i - LgiClipboardClearFunc: %i\n", _FL, p->Type);
		#endif
		p->Empty();
		Data.Unlock();
	}
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	if (!Ptr || Len <= 0)
		return false;

	::GVariant *p = NULL;
	if (Data.Lock(_FL))
	{
		for (int i=0; i<VAR_COUNT; i++)
		{
			if (Data.v[i].Type == GV_NULL)
			{
				p = Data.v + i;
				p->SetBinary(Len, Ptr);
				break;
			}
		}
		Data.Unlock();
	}
	
	GtkTargetEntry te;
	te.target = LGI_CLIP_BINARY;
	te.flags = 0; // GTK_TARGET_SAME_APP?
	te.info = GV_BINARY; // App defined data type ID
	Gtk::gboolean r = gtk_clipboard_set_with_data(d->c,
					                             &te,
					                             1,
					                             LgiClipboardGetFunc,
					                             LgiClipboardClearFunc,
					                             p);


	#if DEBUG_CLIPBOARD
	printf("%s:%i - gtk_clipboard_set_with_data = %i\n", _FL, r);
	#endif
	
	return r;
}

struct ReceiveData
{
	GAutoPtr<uint8> *Ptr;
	ssize_t *Len;
};

void LgiClipboardReceivedFunc(GtkClipboard *clipboard,
                             GtkSelectionData *data,
                             gpointer user_data)
{
	ReceiveData *r = (ReceiveData*)	user_data;
	if (data && r)
	{
		uint8 *d = new uint8[data->length];
		if (d)
		{
			memcpy(d, data->data, data->length);
			if (r->Len)
				*r->Len = data->length;
			r->Ptr->Reset(d);

			#if DEBUG_CLIPBOARD
			printf("%s:%i - LgiClipboardReceivedFunc\n", _FL);
			#endif
		}
		else LgiTrace("%s:%i - Alloc failed %i\n", _FL, data->length);
	}
	else LgiTrace("%s:%i - Missing ptr: %p %p\n", _FL, data, r);
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, ssize_t *Len)
{
	ReceiveData r = {&Ptr, Len};

	gtk_clipboard_request_contents(	d->c,
	                                gdk_atom_intern(LGI_CLIP_BINARY, false),
	                                LgiClipboardReceivedFunc,
	                                &r);
                                

	uint64 Start = LgiCurrentTime();
	do
	{
		if (r.Ptr->Get())
			break;
		LgiYield();
		LgiSleep(1);
	}
	while (LgiCurrentTime() - Start > LGI_RECEIVE_CLIPBOARD_TIMEOUT);

	#if DEBUG_CLIPBOARD
	printf("%s:%i - GClipBoard::Binary %p, %i\n", _FL, r.Ptr->Get(), Len ? *Len : -1);
	#endif

	return r.Ptr->Get() != NULL;
}

