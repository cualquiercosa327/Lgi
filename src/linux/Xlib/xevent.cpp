#include <ctype.h>

#define XK_MISCELLANY 1
#include <keysymdef.h>

#include "LgiOsDefs.h"
#include "xevent.h"
#include "xwidget.h"
#include "xapplication.h"
typedef uint32 COLOUR;
#include "GFile.h"
#include "GFont.h"

static bool KeymapsInit = false;
static LgiKey ODD_keymap[256];
static LgiKey MISC_keymap[256];

/////////////////////////////////////////////////////////////////////////////////////////////
/* Get the translated SDL virtual keysym */
LgiKey X11_TranslateKeycode(Display *display, KeyCode kc)
{
	if (NOT KeymapsInit)
	{
		KeymapsInit = true;
		memset(MISC_keymap, 0, sizeof(MISC_keymap));

		/* These X keysyms have 0xFF as the high byte */
		MISC_keymap[XK_BackSpace&0xFF] = VK_BACKSPACE;
		MISC_keymap[XK_Tab&0xFF] = VK_TAB;
		MISC_keymap[XK_Clear&0xFF] = VK_CLEAR;
		MISC_keymap[XK_Return&0xFF] = VK_RETURN;
		MISC_keymap[XK_Pause&0xFF] = VK_PAUSE;
		MISC_keymap[XK_Escape&0xFF] = VK_ESCAPE;
		MISC_keymap[XK_Delete&0xFF] = VK_DELETE;

		MISC_keymap[XK_KP_0&0xFF] = VK_KP0;		/* Keypad 0-9 */
		MISC_keymap[XK_KP_1&0xFF] = VK_KP1;
		MISC_keymap[XK_KP_2&0xFF] = VK_KP2;
		MISC_keymap[XK_KP_3&0xFF] = VK_KP3;
		MISC_keymap[XK_KP_4&0xFF] = VK_KP4;
		MISC_keymap[XK_KP_5&0xFF] = VK_KP5;
		MISC_keymap[XK_KP_6&0xFF] = VK_KP6;
		MISC_keymap[XK_KP_7&0xFF] = VK_KP7;
		MISC_keymap[XK_KP_8&0xFF] = VK_KP8;
		MISC_keymap[XK_KP_9&0xFF] = VK_KP9;
		MISC_keymap[XK_KP_Insert&0xFF] = VK_KP0;
		MISC_keymap[XK_KP_End&0xFF] = VK_KP1;	
		MISC_keymap[XK_KP_Down&0xFF] = VK_KP2;
		MISC_keymap[XK_KP_Page_Down&0xFF] = VK_KP3;
		MISC_keymap[XK_KP_Left&0xFF] = VK_KP4;
		MISC_keymap[XK_KP_Begin&0xFF] = VK_KP5;
		MISC_keymap[XK_KP_Right&0xFF] = VK_KP6;
		MISC_keymap[XK_KP_Home&0xFF] = VK_KP7;
		MISC_keymap[XK_KP_Up&0xFF] = VK_KP8;
		MISC_keymap[XK_KP_Page_Up&0xFF] = VK_KP9;
		MISC_keymap[XK_KP_Delete&0xFF] = VK_KP_PERIOD;
		MISC_keymap[XK_KP_Decimal&0xFF] = VK_KP_PERIOD;
		MISC_keymap[XK_KP_Divide&0xFF] = VK_KP_DIVIDE;
		MISC_keymap[XK_KP_Multiply&0xFF] = VK_KP_MULTIPLY;
		MISC_keymap[XK_KP_Subtract&0xFF] = VK_KP_MINUS;
		MISC_keymap[XK_KP_Add&0xFF] = VK_KP_PLUS;
		MISC_keymap[XK_KP_Enter&0xFF] = VK_KP_ENTER;
		MISC_keymap[XK_KP_Equal&0xFF] = VK_KP_EQUALS;

		MISC_keymap[XK_Up&0xFF] = VK_UP;
		MISC_keymap[XK_Down&0xFF] = VK_DOWN;
		MISC_keymap[XK_Right&0xFF] = VK_RIGHT;
		MISC_keymap[XK_Left&0xFF] = VK_LEFT;
		MISC_keymap[XK_Insert&0xFF] = VK_INSERT;
		MISC_keymap[XK_Home&0xFF] = VK_HOME;
		MISC_keymap[XK_End&0xFF] = VK_END;
		MISC_keymap[XK_Page_Up&0xFF] = VK_PAGEUP;
		MISC_keymap[XK_Page_Down&0xFF] = VK_PAGEDOWN;

		MISC_keymap[XK_F1&0xFF] = VK_F1;
		MISC_keymap[XK_F2&0xFF] = VK_F2;
		MISC_keymap[XK_F3&0xFF] = VK_F3;
		MISC_keymap[XK_F4&0xFF] = VK_F4;
		MISC_keymap[XK_F5&0xFF] = VK_F5;
		MISC_keymap[XK_F6&0xFF] = VK_F6;
		MISC_keymap[XK_F7&0xFF] = VK_F7;
		MISC_keymap[XK_F8&0xFF] = VK_F8;
		MISC_keymap[XK_F9&0xFF] = VK_F9;
		MISC_keymap[XK_F10&0xFF] = VK_F10;
		MISC_keymap[XK_F11&0xFF] = VK_F11;
		MISC_keymap[XK_F12&0xFF] = VK_F12;
		MISC_keymap[XK_F13&0xFF] = VK_F13;
		MISC_keymap[XK_F14&0xFF] = VK_F14;
		MISC_keymap[XK_F15&0xFF] = VK_F15;

		MISC_keymap[XK_Num_Lock&0xFF] = VK_NUMLOCK;
		MISC_keymap[XK_Caps_Lock&0xFF] = VK_CAPSLOCK;
		MISC_keymap[XK_Scroll_Lock&0xFF] = VK_SCROLLOCK;
		MISC_keymap[XK_Shift_R&0xFF] = VK_RSHIFT;
		MISC_keymap[XK_Shift_L&0xFF] = VK_LSHIFT;
		MISC_keymap[XK_Control_R&0xFF] = VK_RCTRL;
		MISC_keymap[XK_Control_L&0xFF] = VK_LCTRL;
		MISC_keymap[XK_Alt_R&0xFF] = VK_RALT;
		MISC_keymap[XK_Alt_L&0xFF] = VK_LALT;
		MISC_keymap[XK_Meta_R&0xFF] = VK_RMETA;
		MISC_keymap[XK_Meta_L&0xFF] = VK_LMETA;
		MISC_keymap[XK_Super_L&0xFF] = VK_LSUPER; /* Left "Windows" */
		MISC_keymap[XK_Super_R&0xFF] = VK_RSUPER; /* Right "Windows */
		MISC_keymap[XK_Mode_switch&0xFF] = VK_MODE; /* "Alt Gr" key */
		MISC_keymap[XK_Multi_key&0xFF] = VK_COMPOSE; /* Multi-key compose */

		MISC_keymap[XK_Help&0xFF] = VK_HELP;
		MISC_keymap[XK_Print&0xFF] = VK_PRINT;
		MISC_keymap[XK_Sys_Req&0xFF] = VK_SYSREQ;
		MISC_keymap[XK_Break&0xFF] = VK_BREAK;
		MISC_keymap[XK_Menu&0xFF] = VK_MENU;
		MISC_keymap[XK_Hyper_R&0xFF] = VK_MENU;   /* Windows "Menu" key */
	}

	KeySym xsym = XKeycodeToKeysym(display, kc, 0);
	LgiKey key = VK_UNKNOWN;

	if (xsym)
	{
		switch (xsym>>8)
		{
		    case 0x1005FF:
				#ifdef SunXK_F36
				if ( xsym == SunXK_F36 )
					key = VK_F11;
				#endif
				#ifdef SunXK_F37
				if ( xsym == SunXK_F37 )
					key = VK_F12;
				#endif
				break;
		    case 0x00:	/* Latin 1 */
				key = (LgiKey)(xsym & 0xFF);
				break;
		    case 0x01:	/* Latin 2 */
		    case 0x02:	/* Latin 3 */
		    case 0x03:	/* Latin 4 */
		    case 0x04:	/* Katakana */
		    case 0x05:	/* Arabic */
		    case 0x06:	/* Cyrillic */
		    case 0x07:	/* Greek */
		    case 0x08:	/* Technical */
		    case 0x0A:	/* Publishing */
		    case 0x0C:	/* Hebrew */
		    case 0x0D:	/* Thai */
				/* These are wrong, but it's better than nothing */
				key = (LgiKey)(xsym & 0xFF);
				break;
		    case 0xFE:
				key = ODD_keymap[xsym&0xFF];
				break;
		    case 0xFF:
				key = MISC_keymap[xsym&0xFF];
				break;
			default:
				/*
				fprintf(stderr, "X11: Unhandled xsym, sym = 0x%04x\n",
						(unsigned int)xsym);
				*/
				break;
		}
	}
	else
	{
		/* X11 doesn't know how to translate the key! */
		switch (kc)
		{
		    /* Caution:
		       These keycodes are from the Microsoft Keyboard
		     */
		    case 115:
				key = VK_LSUPER;
				break;
		    case 116:
				key = VK_RSUPER;
				break;
		    case 117:
				key = VK_MENU;
				break;
		    	default:
				/*
			 	 * no point in an error message; happens for
			 	 * several keys when we get a keymap notify
			 	 */
				break;
		}
	}

	// printf("xsym=%i %x key=%i %x\n", xsym, xsym, key, key);

	return key;
}

XlibEvent::XlibEvent(XEvent *e)
{
	Event = e;

	Scancode = 0;
	Sym = VK_UNKNOWN;
	Mod = 0;
	Unicode = 0;

	if (Event->type == KeyPress OR Event->type == KeyRelease)
	{
		Scancode = Event->xkey.keycode;
		Sym = X11_TranslateKeycode(XDisplay(), Scancode);
		
		// printf("Key down=%i Sym=%x\n", Event->type == KeyPress, Sym);
	}
}

int XlibEvent::type()
{
	return Event->type;
}

// Mouse
int XlibEvent::x()
{
	switch (Event->type)
	{
		case ButtonPress:
		case ButtonRelease:
		{
			return Event->xbutton.x - 1;
			break;
		}
		case MotionNotify:
		{
			return Event->xmotion.x - 1;
			break;
		}
	}

	return 0;
}

int XlibEvent::ScreenX()
{
	switch (Event->type)
	{
		case ButtonPress:
		case ButtonRelease:
		{
			return Event->xbutton.x_root - 1;
			break;
		}
		case MotionNotify:
		{
			return Event->xmotion.x_root - 1;
			break;
		}
	}

	return 0;
}

int XlibEvent::y()
{
	switch (Event->type)
	{
		case ButtonPress:
		case ButtonRelease:
		{
			return Event->xbutton.y - 1;
			break;
		}
		case MotionNotify:
		{
			return Event->xmotion.y - 1;
			break;
		}
	}

	return 0;
}

int XlibEvent::ScreenY()
{
	switch (Event->type)
	{
		case ButtonPress:
		case ButtonRelease:
		{
			return Event->xbutton.y_root - 1;
			break;
		}
		case MotionNotify:
		{
			return Event->xmotion.y_root - 1;
			break;
		}
	}

	return 0;
}

int XlibEvent::button()
{
	int Status = 0;
	int State = state();

	switch (Event->type)
	{
		case ButtonPress:
		case ButtonRelease:
		{
			switch (Event->xbutton.button)
			{
				case 1:
					Status |= XWidget::LeftButton;
					break;
				case 2:
					Status |= XWidget::MidButton;
					break;
				case 3:
					Status |= XWidget::RightButton;
					break;
			}
			break;
		}
		case MotionNotify:
		{
			if (State & Button1MotionMask)
			{
				Status |= XWidget::LeftButton;
			}
			else if (State & Button2MotionMask)
			{
				Status |= XWidget::MidButton;
			}
			else if (State & Button3MotionMask)
			{
				Status |= XWidget::RightButton;
			}
			break;
		}
	}

	if (State & 0x8)
	{
		Status |= XWidget::AltButton;
	}

	if (State & ControlMask)
	{
		Status |= XWidget::ControlButton;
	}

	if (State & ShiftMask)
	{
		Status |= XWidget::ShiftButton;
	}

	return Status;
}

bool XlibEvent::down()
{
	switch (Event->type)
	{
		case ButtonPress:
		case KeyPress:
		{
			return true;
			break;
		}
		case MotionNotify:
		{
			return (button() & (XWidget::LeftButton | XWidget::MidButton | XWidget::RightButton)) != 0;
		}
	}

	return 0;
}

bool XlibEvent::doubleclick()
{
	if (Event->type == ButtonPress AND
		Event->xbutton.button >= 0 AND
		Event->xbutton.button < 4)
	{
		uint64 Now = LgiCurrentTime();
		uint64 Last = XApp()->GetLastButtonPressTime
			(
				Event->xbutton.button-1,
				Event->xbutton.x_root,
				Event->xbutton.y_root
			);
			
		return (Now - Last) < DOUBLE_CLICK_TIME;
	}

	return false;
}

// Keyboard
char16 XlibEvent::unicode(XIC Ic)
{
	if (NOT Unicode AND
		(Event->type == KeyPress OR Event->type == KeyRelease))
	{
		Scancode = Event->xkey.keycode;
		Sym = X11_TranslateKeycode(XDisplay(), Scancode);
		
		if (Ic)
		{
			/* A UTF-8 character can be at most 6 bytes */
			char keybuf[6];
			static int LookupStringState = 0;
			
			// Turn off ctrl, it wrecks up what char this is
			int OldState = Event->xkey.state;
			Event->xkey.state &= ~ControlMask;

			// Find the utf representation of the string
			uint8 *i = (uint8*)keybuf;
			int Len = Xutf8LookupString(Ic,
										&Event->xkey,
										keybuf,
										sizeof(keybuf),
										NULL,
										&LookupStringState);
			if (Len)
			{
				// Convert to utf32
				Unicode = LgiUtf8To32(i, Len);
			}
			else
			{
				// No unicode representation, but usually that means there
				// is an ascii representation... e.g. tab (0x9)
				Unicode = (int)Sym;
			}
			
			// Turn control back on if needed
			Event->xkey.state = OldState;
		}

	
		/*
		// Do this here, so that it runs in the GUI thread and
		// can't get stuck in a deadlock getting the mutex
		//
		// Also it's nice that it's only called once too.

		KeySym k;
		char c[256] = "";
		int Chars = 0;
		bool IsShift = (Event->xkey.state & ShiftMask) != 0; // ShiftMask == 0x01
		bool IsCtrl = button() & XWidget::ControlButton;

		int State = Event->xkey.state;
		Event->xkey.state &= ~0x4;
		Chars = XLookupString(&Event->xkey, c, sizeof(c), &k, 0);
		Event->xkey.state = State;

		if (Chars > 0)
		{
			c[Chars] = 0;
			Char = c[0];
		}

		if (Char >= 0x0e AND Char <= 0x1a)
		{
			// This range is assigned to virtual keys, X11 shouldn't generate these
			// directly. We assign this range of values in the if (k == 0x####) code.
			Char = 0;
		}


		if (k == 65505) Char = VK_RSHIFT;
		else if (k == 65307) Char = VK_ESCAPE;
		else if (k == 65361) Char = VK_LEFT;
		else if (k == 65362) Char = VK_UP;
		else if (k == 65363) Char = VK_RIGHT;
		else if (k == 65364) Char = VK_DOWN;
		else if (k == 65365) Char = VK_PAGEUP;
		else if (k == 65366) Char = VK_PAGEDOWN;
		else if (k == 65360) Char = VK_HOME;
		else if (k == 65367) Char = VK_END;
		else if (k == 65379) Char = VK_INSERT;
		else if (k == 65535) Char = VK_DELETE;
		else if (k == 65289 OR k == 65056) Char = '\t';
		else if (k == 65293 OR k == 65421) Char = VK_RETURN;
		else if (k == 65288) Char = VK_BACKSPACE;
		else if (k == 65470) Char = VK_F1;
		else if (k == 65471) Char = VK_F2;
		else if (k == 65472) Char = VK_F3;
		else if (k == 65473) Char = VK_F4;
		else if (k == 65474) Char = VK_F5;
		else if (k == 65475) Char = VK_F6;
		else if (k == 65476) Char = VK_F7;
		else if (k == 65477) Char = VK_F8;
		else if (k == 65478) Char = VK_F9;
		else if (k == 65479) Char = VK_F10;
		else if (k == 65480) Char = VK_F11;
		else if (k == 65481) Char = VK_F12;
		else if (k == 65508) Char = VK_LCTRL;

		switch (Char)
		{
			case VK_F1:
			case VK_F2:
			case VK_F3:
			case VK_F4:
			case VK_F5:
			case VK_F6:
			case VK_F7:
			case VK_F8:
			case VK_F9:
			case VK_F10:
			case VK_F11:
			case VK_F12:
			{
				if (IsCtrl) Event->type = KeyPress;
				break;
			}
		}

		#if 0
		printf("::unicode Char=%i,0x%x,'%c' chars=%i keycode=%i keysym=%i (0x%x) state=%x (%s%s%s%s)\n",
			Char, Char, Char<' '?'.':Char, Chars,
			Event->xkey.keycode, k, k, Event->xkey.state,
			(char*)(button() & XWidget::ShiftButton ? " Shift" : ""),
			(char*)(button() & XWidget::AltButton ? " Alt" : ""),
			(char*)(button() & XWidget::ControlButton ? " Ctrl" : ""),
			(char*)(down() ?" Down":"")
			);
		#endif
		
		*/
	}

	return Unicode;
}

bool XlibEvent::ischar()
{
	return
		(
			(
				Unicode >= ' '
				AND
				Unicode <= 0xffff
				AND
				Unicode != VK_DELETE
			)
			OR
			Unicode == '\t'
			OR
			Unicode == VK_RETURN
			OR
			Unicode == 8
		)
		AND
		NOT TestFlag(button(), XWidget::AltButton)
		AND
		NOT TestFlag(button(), XWidget::ControlButton);
}

int XlibEvent::state()
{
	switch (Event->type)
	{
		case KeyPress:
		case KeyRelease:
		{
			return Event->xkey.state;
			break;
		}
		case MotionNotify:
		{
			return Event->xmotion.state;
			break;
		}
		case ButtonPress:
		case ButtonRelease:
		{
			return Event->xbutton.state;
			break;
		}
	}
	return 0;
}

// Wheel
int XlibEvent::delta()
{
	switch (Event->type)
	{
		case ButtonPress:
		{
			if (Event->xbutton.button == 4) return 120;
			if (Event->xbutton.button == 5) return -120;
			break;
		}
		case ButtonRelease:
		{
			break;
		}
	}

	return 0;
}

// Focus
bool XlibEvent::focus()
{
	return 0;
}

// Expose event
GRect &XlibEvent::exposed()
{
	static GRect r;

	switch (Event->type)
	{
		case Expose:
		{
			r.Set(	Event->xexpose.x,
					Event->xexpose.y,
					Event->xexpose.x + Event->xexpose.width,
					Event->xexpose.y + Event->xexpose.height);
			break;
		}
        default:
        {
        	r.ZOff(-1, -1);
            break;
        }
	}

	return r;
}

