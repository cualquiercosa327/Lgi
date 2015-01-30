#include "Lgi.h"
#include "GHtmlEdit.h"
#include "GHtmlPriv.h"
#include "GSkinEngine.h"
#include "GCombo.h"
#include "GScrollBar.h"
#include "GClipBoard.h"
#include "GDisplayString.h"

enum HtmlEditIds {

	// Controls
	IDC_STYLE = 300,
	IDC_FONT,
	IDC_POINTSIZE,
	IDC_BOLD,
	IDC_ITALIC,
	IDC_UNDERLINE,
	IDC_MAKE_LINK,
	IDC_FORE_COLOUR,
	IDC_BACK_COLOUR,
	IDC_DEBUG_WND,
	IDC_HTML_EDIT,
	
	// Messages
	IDM_COPY_ORIGINAL_SOURCE = 400
};

#define TOOLBAR_HT					18

#define	TAB_STOP					0

#define DEBUG_CURSOR_POS			0

#define SubtractPtr(a, b)			( (((int)(a))-((int)(b))) / sizeof(*a) )

using namespace Html1;

static char16 EmptyStr[] = {0};

uint32 IconData[] =
{
	/*
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x6FE16FE1, 0xFE616FE1, 0xFE61FE61, 0x0000FB61, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0x8C510861, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x0FF90FF9, 0x6FE16FE1, 0xFE616FE1, 0xFE61FE61, 0xFB61FB61, 0x0000FB61, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x52AAFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x0FF90FF9, 0x6FE16FE1, 0xFE616FE1, 0xFB61FE61, 0xFB61FB61, 0x0000F861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0x8C51FFFF, 0x8C510861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0x9CD3DEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0FF90000, 0x0FF90FF9, 0x6FE10FF9, 0xDEFB6FE1, 0xDEFBDEFB, 0xF861FB61, 0xF861F861, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDEFB632C, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0FF90000, 0x0FF90FF9, 0xDEFB0FF9, 0xDEFBDEFB, 0xDEFBDEFB, 0xF861DEFB, 0xF861F861, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF632C, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0B7F0000, 0x0B7F0B7F, 0xDEFBDEFB, 0xDEFBDEFB, 0x00000000, 0xDEFBDEFB, 0xF861F861, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08618C51, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CD3FFFF, 0xFFFFDEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0B7F0000, 0x0B7F0B7F, 0xDEFBDEFB, 0x0000DEFB, 0xFFFFFFFF, 0xDEFB0000, 0x0000DEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08618C51, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x52AAFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0B7F0000, 0x087F087F, 0xDEFB087F, 0x0000DEFB, 0xFFFFFFFF, 0xDEFB0000, 0x0000DEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x087F087F, 0xDEFB087F, 0xDEFBDEFB, 0x00000000, 0x0000DEFB, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0x08610861, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x087F087F, 0xC87FC87F, 0xDEFBC87F, 0xDEFBDEFB, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xC87FC87F, 0xDEFBC87F, 0x0000DEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x632CFFFF, 0x00000000, 0x00000000, 0x00000000, 
	0x632C0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	*/
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xCE790000, 0xFFFFCE79, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xCE79CE79, 0xAD55CE79, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0x8C510861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xCE790000, 0xCE790000, 0xAD55AD55, 0xFFFFAD55, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x0000FFFF, 0x00000000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x52AAFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 
	0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xCE790000, 0x0000CE79, 0xAD55AD55, 0xFFFFAD55, 0xFFFFFFFF, 0x0000FFFF, 0xEF7DEF7D, 0xEF7DEF7D, 0xFFFF0000, 0xEF7D0000, 0xEF7DEF7D, 0x0000EF7D, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0x8C51FFFF, 0x8C510861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CD3DEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x0000EF7D, 0xAD55AD55, 0xAD550000, 0x00008C51, 0xFFFFFFFF, 0xEF7D0000, 0x0000EF7D, 0xCE790000, 0x0000CE79, 0xAD55CE79, 0x00000000, 0xCE79CE79, 
	0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xDEFB632C, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xEF7D0000, 0xEF7DEF7D, 0xAD550000, 0x00008C51, 0xFFFF0000, 0xFFFFFFFF, 0xEF7D0000, 0xFFFF0000, 0x0000FFFF, 0xCE790000, 0x0000AD55, 0xFFFFFFFF, 0xAD550000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF632C, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xBDD7BDD7, 0xBDD7BDD7, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xEF7DEF7D, 0xEF7DEF7D, 0x0000DEFB, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xEF7D0000, 0xFFFF0000, 0x0000FFFF, 0xAD55CE79, 0x00000000, 0xFFFFFFFF, 0xAD550000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08618C51, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CD3FFFF, 0xFFFFDEFB, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xBDD7FFFF, 0x08610861, 0x08610861, 0x08610861, 0x08610861, 0xFFFFBDD7, 0xFFFFFFFF, 0xFFFFFFFF, 0xEF7D0000, 0xEF7DEF7D, 0xDEFBEF7D, 0x0000CE79, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xEF7D0000, 0x0000CE79, 0xCE790000, 0x0000AD55, 0xCE79CE79, 0x00000000, 0xAD55CE79, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0x08618C51, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x52AAFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xEF7D0000, 0xEF7DEF7D, 0xCE79DEFB, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0xAD55CE79, 0xAD55AD55, 0xFFFF0000, 0xCE790000, 0xAD55AD55, 0x0000AD55, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x0000FFFF, 0x00000000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0x08610861, 0x08610861, 0xFFFF8C51, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0861FFFF, 0x08610861, 0x08610861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x08610861, 0xFFFF0861, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CD3FFFF, 0x9CD39CD3, 0x9CD39CD3, 0x9CD39CD3, 0x9CD39CD3, 0xFFFF9CD3, 0xFFFFFFFF, 0xFFFFFFFF, 0x632CFFFF, 0x632C632C, 0x632C632C, 0x632C632C, 0x632C632C, 0xFFFF632C, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x632CFFFF, 0x00000000, 0x00000000, 0x00000000, 0x632C0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CD3FFFF, 0x9CD39CD3, 0x9CD39CD3, 0x9CD39CD3, 0x9CD39CD3, 0xFFFF9CD3, 0xFFFFFFFF, 0xFFFFFFFF, 0x632CFFFF, 0x632C632C, 0x632C632C, 0x632C632C, 0x632C632C, 0xFFFF632C, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
};
GInlineBmp Icons = { 96, 16, 16, IconData };

int XSort(GFlowRect **a, GFlowRect **b)
{
	return (*a)->x1 - (*b)->x1;
}

bool CanHaveText(HtmlTag TagId)
{
	switch (TagId)
	{
		default: break;
		case TAG_TABLE:
		case TAG_TR:
		case TAG_BR:
		case TAG_HR:
		case TAG_BODY:
		case TAG_HTML:
			return false;
	}

	return true;
}

int GFlowRect::Start()
{
	if (!Tag) return -1;
	char16 *t = Tag->Text();
	if (!t) return -1;
	char16 *e = t + StrlenW(t);
	if (Text < t || Text > e)
	{
		LgiAssert(!"Text should always point into the tag's buffer");
		return -1;
	}
	return Text - t;
}

class Btn : public GLayout
{
	int64 Val;
	int Icon;
	GImageList *Img;
	int HasLeft;
	int HasRight;
	bool Toggle;
	GAutoPtr<GDisplayString> Str;

public:
	int SpaceAfter;

	Btn(int cmd, int ico, bool toggle, GImageList *img, int wid)
	{
		Val = 0;
		SetId(cmd);
		Icon = ico;
		Img = img;
		HasLeft = -1;
		HasRight = -1;
		Toggle = toggle;
		SpaceAfter = 0;

		GRect r(0, 0, wid-1, TOOLBAR_HT);
		SetPos(r);
		SetTabStop(TAB_STOP);
	}

	Btn(int cmd, bool toggle, const char *Text)
	{
		Val = 0;
		SetId(cmd);
		Icon = -1;
		Img = 0;
		HasLeft = -1;
		HasRight = -1;
		Toggle = toggle;
		SpaceAfter = 0;
		
		Str.Reset(new GDisplayString(SysBold, Text));
		GRect r(0, 0, Str->X()+16, TOOLBAR_HT);
		SetPos(r);
		SetTabStop(TAB_STOP);
	}

	int64 Value()
	{
		return Val;
	}

	void Value(int64 v)
	{
		Val = v;
		Invalidate();
	}

	void OnPaint(GSurface *pDC)
	{
		if (HasLeft < 0 || HasRight < 0)
		{
			GViewIterator *i = GetParent()->IterateViews();
			if (i)
			{
				HasLeft = false;
				HasRight = false;

				GRect Pos = GetPos();

				for (GViewI *c = i->First(); c; c = i->Next())
				{
					GRect CPos = c->GetPos();
					if (CPos.x2 == Pos.x1 - 1)
					{
						HasLeft = true;
					}
					if (CPos.x1 == Pos.x2 + 1)
					{
						HasRight = true;
					}
				}

				DeleteObj(i);
			}
		}

		GRect Back = GetClient();
		if (HasLeft)
		{
			Back.x1 -= 40;
		}
		if (HasRight)
		{
			Back.x2 += 40;
		}
		
		GMemDC Mem(X(), Y(), System32BitColourSpace);
		GColour MemBackground(LC_MED, 24);
		Mem.Colour(MemBackground);
		Mem.Rectangle();

		GSkinEngine *s = LgiApp->SkinEngine;
		if (s)
		{
			s->DrawBtn(&Mem, Back, Val != 0, Enabled());
		}
		else
		{
			LgiWideBorder(&Mem, Back, Val ? DefaultSunkenEdge : DefaultRaisedEdge);
		}

		if (HasLeft && Val)
		{
			Mem.Colour(Rgb24(0x72, 0x72, 0x72), 24);
			Mem.Line(0, 0, 0, Y()-1);
			Mem.Colour(Rgb24(0xa0, 0xa0, 0xa0), 24);
			Mem.Line(1, 1, 1, Y()-2);
		}

		if (Str)
		{
			int dwn = Value() ? 1 : 0;
			Str->Draw(&Mem, (Mem.X()-Str->X())/2+dwn, (Mem.Y()-Str->Y())/2+dwn);
		}
		else if (Img)
		{
			GRect r(0, 0, Img->TileX()-1, Img->TileY()-1);
			r.Offset(((X()-Img->TileX())>>1)+(int)Val, ((Y()-Img->TileY())>>1)+(int)Val);
			Mem.Colour(Rgb24(255, 0, 0), 24);
			if (HasLeft && !HasRight)
			{
				// Right end
				r.Offset(-2, 0);
			}
			else if (!HasLeft && HasRight)
			{
				// Left end
				r.Offset(2, 0);
			}
			Img->Draw(&Mem, r.x1, r.y1, Icon, MemBackground);
		}

		pDC->Blt(0, 0, &Mem);
	}

	void OnMouseClick(GMouse &m)
	{
		if (m.Left())
		{
			Capture(m.Down());
			if (!Toggle)
			{
				Value(m.Down());
			}

			if (!m.Down() && GetClient().Overlap(m.x, m.y))
			{
				// Post msg.
				SendNotify();
			}
		}
	}

	void OnMouseMove(GMouse &m)
	{
		if (!Toggle && IsCapturing())
		{
			int v = GetClient().Overlap(m.x, m.y);
			if (v != Val)
			{
				Value(v);
			}
		}
	}
};

struct Block : public GRect
{
	enum Direction
	{
		Unknown,
		Left,
		Up,
		Right,
		Down
	};
	
	GTag *t;
	GFlowRect *fr;

	Block()
	{
		t = NULL;
		fr = NULL;
	}

	bool OverlapX(int x) { return x >= x1 && x <= x2; }
	bool OverlapY(int y) { return y >= y1 && y <= y2; }
	bool OverlapX(Block *b) { return !(b->x2 < x1 || b->x1 > x2); }
	bool OverlapY(Block *b) { return !(b->y2 < y1 || b->y1 > y2); }

	int StartOffset()
	{
		int Chars = 0;
		for (unsigned i = t->PreText() ? 1 : 0; i<t->TextPos.Length(); i++)
		{
			GFlowRect *r = t->TextPos[i];
			
			if (r == fr)
			{
				return Chars;
			}

			Chars += r->Len;
		}

		return 0;
	}

	int EndOffset()
	{
		int Chars = 0;
		for (unsigned i = t->PreText() ? 1 : 0; i<t->TextPos.Length(); i++)
		{
			GFlowRect *r = t->TextPos[i];
			if (r == fr)
			{
				return Chars + r->Len;
			}

			Chars += r->Len;
		}

		return 0;
	}
};

class HtmlEdit : public Html1::GHtml, public GDefaultDocumentEnv
{
	GTag *b;
	GHtmlEdit *Edit;
	GArray<GTag*> OptimizeTags;

	Block *FindBlock(Block::Direction Dir, int x, int y)
	{
		static Block Ret;
		
		// Get the tag at the given position...
		int Index;
		GdcPt2 LocalCoords;
		GTag *t = GetTagByPos(x, y, &Index, &LocalCoords, false);
		if (!t)
			return NULL;
		
		// Move up to block level element...
		while (	t->Display() != GCss::DispBlock &&
				t->Parent)
		{
			t = ToTag(t->Parent);
		}
		
		// Scan through all the children looking for suitable text rects...
		Ret.t = NULL;
		Ret.fr = NULL;
		for (unsigned i=0; i<t->Children.Length(); i++)
		{
			GTag *c = ToTag(t->Children[i]);
			if (!c) continue;
			
			for (unsigned n=0; n<c->TextPos.Length(); n++)
			{
				GFlowRect *r = c->TextPos[n];
				if (!r) continue;

				switch (Dir)
				{
					case Block::Left:
					{
						if (r->OverlapY(y) && r->x2 <= x)
						{
							// Left of current position... but is it nearest?
							if (!Ret.fr || Ret.fr->x2 < r->x2)
							{
								Ret.t = c;
								Ret.fr = r;
							}
						}
						break;
					}
					case Block::Right:
					{
						if (r->OverlapY(y) && r->x1 >= x)
						{
							// Left of current position... but is it nearest?
							if (!Ret.fr || Ret.fr->x1 > r->x1)
							{
								Ret.t = c;
								Ret.fr = r;
							}
						}
						break;
					}
					case Block::Up:
					{
						if (r->y2 <= y)
						{
							if (!Ret.fr ||
								r->y2 > Ret.fr->y2 ||
								(r->OverlapY(Ret.fr) && r->x1 > Ret.fr->x1))
							{
								Ret.t = c;
								Ret.fr = r;
							}
						}
						break;
					}
					case Block::Down:
					{
						if (r->y1 >= y)
						{
							if (!Ret.fr ||
								r->y1 < Ret.fr->y1 ||
								(r->OverlapY(Ret.fr) && r->x2 < Ret.fr->x2))
							{
								Ret.t = c;
								Ret.fr = r;
							}
						}
						break;
					}
				}
			}
		}
		
		return Ret.t ? &Ret : NULL;
	}

public:
	GAutoWString OriginalSrcW;
	
	HtmlEdit(GHtmlEdit *edit) : GHtml(100, 0, 0, 100, 100)
	{
		// Construct the basics
		SetId(IDC_HTML_EDIT);
		SetReadOnly(false);
		Edit = edit;
		b = 0;
		SetEnv(this);
		Sunken(true);
		SetTabStop(true);
		
		GAutoString n(NewStr("<html><body></body></html>"));
		Name(n);
	}

	const char *GetClass() { return "HtmlEdit"; }

	void OnDocumentChange()
	{
		SendNotify(GTVN_DOC_CHANGED);
	}

	bool OnContextMenuCreate(struct GTagHit &Hit, GSubMenu &RClick)
	{
		RClick.AppendItem("Copy Original Source", IDM_COPY_ORIGINAL_SOURCE);
		return true;
	}
	
	void OnContextMenuCommand(struct GTagHit &Hit, int Cmd)
	{
		if (Cmd == IDM_COPY_ORIGINAL_SOURCE)
		{
			GClipBoard c(this);
			if (OriginalSrcW)
				c.TextW(OriginalSrcW, true);
		}
	}

	// Draw a red box around the cursor for debugging.
	void OnPaint(GSurface *pDC)
	{
		GHtml::OnPaint(pDC);

		#if DEBUG_CURSOR_POS
		int LineY = GetFont()->GetHeight();
		int sx, sy;
		GetScrollPos(sx, sy);
		GRect c = GetCursorPos();
		c.Offset(0, -LineY*sy);
		c.Size(-1, -1);
		pDC->Colour(Rgb24(255, 0, 0), 24);
		pDC->Box(&c);
		#endif
	}

	bool IsSameStyle(GTag *a, GTag *b)
	{
		bool Status = false;

		if (a && b)
		{
			if (a->TagId == b->TagId)
			{
				if (a->Font && b->Font)
				{
					char *af = a->Font->Face();
					char *bf = b->Font->Face();
					if (af && bf)
					{
						Status = _stricmp(af, bf) == 0;
						Status &= a->Font->PointSize() == b->Font->PointSize();
						Status &= a->Font->Bold() == b->Font->Bold();
						Status &= a->Font->Underline() == b->Font->Underline();
						Status &= a->Font->Italic() == b->Font->Italic();
					}
				}
				else
				{
					Status = true;
				}
			}
		}

		return Status;
	}

	bool OptimizeLine(GArray<GTag*> &t, int &Deletes)
	{
		bool Changed = false;
		
		Deletes = 0;
		for (unsigned i=0; i<t.Length(); i++)
		{
			GTag *c = t[i];
			if (c->TagId == CONTENT &&
				!ValidStrW(c->Text()))
			{
				if (t.Length() > 1)
				{
					// Remove the blank tag
					c->Detach();
					Deletes++;
					DeleteObj(c);
				}
			}
		}
		
		return Changed;
	}

	void Optimize(GTag *Tag)
	{
		if (Tag)
		{
			bool Changed = false;

			// Look through direct children and remove unnecessary complexity
			GArray<GHtmlElement*> &t = Tag->Children;
			GArray<GTag*> Line;			
			int Deletes = 0;
			for (unsigned i=0; i<t.Length(); i++)
			{
				GTag *c = ToTag(t[i]);
				if (!c)
					break;
				
				if (c->Display() == GCss::DispBlock ||
					c->TagId == TAG_BR)
				{
					Changed |= OptimizeLine(Line, Deletes);
					Line.Length(0);
					i -= Deletes;
				}
				else
				{
					Line.Add(c);
				}
				
				/*
				if (IsSameStyle(a, b))
				{
					int LenA = StrlenW(a->Text());
					int LenB = StrlenW(b->Text());
					char16 *Txt = new char16[LenA + LenB + 1];
					if (Txt)
					{
						if (a->Text())
							StrcpyW(Txt, a->Text());
						if (b->Text())
							StrcpyW(Txt + LenA, b->Text());
						a->Text(Txt);

						if (b == Cursor &&
							b->Cursor >= 0)
						{
							// Ah! We're about to delete the tag that the cursor points at...
							// better move it:
							a->Cursor = LenA + b->Cursor;
							b->Cursor = -1;
							Cursor = a;
						}
						
						LgiAssert(b != Selection && b != Cursor);
						b->Detach();
						DeleteObj(b);
						
						t.DeleteAt(i + 1, true);
						i--;						
						Changed = true;
					}
				}
				*/
			}
			
			Changed |= OptimizeLine(Line, Deletes);

			if (Changed)
			{
				ViewWidth = -1;
			}
		}
	}

	void Optimize()
	{
		for (unsigned i=0; i<OptimizeTags.Length(); i++)
		{
			Optimize(OptimizeTags[i]);
		}
		OptimizeTags.Length(0);
	}

	GTag *GetStyleInsertPoint(GTag *Cur, int *Idx)
	{
		if (Cur)
		{
			GTag *Prev = 0;
			for (GTag *t = Cur; t; t = ToTag(t->Parent))
			{
				if (t->TagId == TAG_B ||
					t->TagId == TAG_U ||
					t->TagId == TAG_I ||
					t->TagId == TAG_TD ||
					t->Display() == GCss::DispBlock)
				{
					if (Idx)
					{
						*Idx = Prev ? t->Children.IndexOf(Prev) : 0;
					}

					return t;
				}

				Prev = t;
			}
		}

		return 0;
	}

	template <typename T>
	bool HasStyle(GCss::PropType s, GArray<T> &Values)
	{
		bool CursorFirst = IsCursorFirst();

		if (Cursor && Selection)
		{
			GTag *Min = CursorFirst ? Cursor : Selection;
			GTag *Max = CursorFirst ? Selection : Cursor;

			for (GTag *t = Min; t; t = NextTag(t))
			{
				T *v = (T*) t->PropAddress(s);
				if (v)
				{
					Values.Add(*v);
				}

				if (t == Max)
					break;
			}
		}
		else if (Cursor)
		{
			T *v = (T*) Cursor->PropAddress(s);
			if (v)
			{
				Values.Add(*v);
			}
		}

		return Values.Length() > 0;
	}

	bool HasElement(const char *e, GArray<GTag*> *out = 0)
	{
		bool CursorFirst = IsCursorFirst();

		if (Cursor && Selection)
		{
			GTag *Min = CursorFirst ? Cursor : Selection;
			GTag *Max = CursorFirst ? Selection : Cursor;

			for (GTag *t = Min; t; t = NextTag(t))
			{
				if (t->Tag && !_stricmp(t->Tag, e))
				{
					if (out)
						out->Add(t);
					else
						return true;
				}

				if (t == Max)
					break;
			}
		}
		else if (Cursor)
		{
			if (Cursor->Tag && !_stricmp(Cursor->Tag, e))
			{
				if (out)
					out->Add(Cursor);
				else
					return true;
			}
		}

		return out ? out->Length() > 0 : false;
	}

	bool StylizeSelection(GCss &Style, GArray<GTag*> *NewTags = 0, const char *NewElement = "span")
	{
		if (!Cursor || !Selection)
			return false;

		bool CursorFirst = IsCursorFirst();
		GTag *MinTag, *MaxTag;
		int MinIdx, MaxIdx;
		if (CursorFirst)
		{
			MinTag = Cursor;
			MinIdx = Cursor->Cursor;
			MaxTag = Selection;
			MaxIdx = Selection->Selection;
		}
		else
		{
			MaxTag = Cursor;
			MaxIdx = Cursor->Cursor;
			MinTag = Selection;
			MinIdx = Selection->Selection;
		}

		if (!MinTag->Parent)
		{
			LgiAssert(!"Huh?");
			return false;
		}

		if (MinTag == MaxTag)
		{
			// Single tag change
			int Idx = MinTag->Parent->Children.IndexOf(MinTag);
			int Start = MinTag->GetTextStart();
			int Len = StrlenW(MinTag->Text());

			if (MinIdx > Start)
			{
				// Starting partially into the tag...
				GTag *Edit = new GTag(this, 0);
				Edit->Text(NewStrW(MinTag->Text() + MinIdx + Start, MaxIdx - MinIdx));
				Edit->SetTag(NewElement);
				Edit->CopyStyle(Style);
				if (NewTags) NewTags->Add(Edit);

				MinTag->Parent->Attach(Edit, ++Idx);

				if (MaxIdx + Start < Len)
				{
					GTag *After = new GTag(this, 0);
					After->Text(NewStrW(MinTag->Text() + MaxIdx + Start));
					MinTag->Parent->Attach(After, ++Idx);
				}

				MinTag->Text()[MinIdx + Start] = 0;

				// Deselect...
				Cursor->Cursor = -1;
				Selection->Selection = -1;

				// Reselect new tag
				Cursor = Selection = Edit;
				Selection->Selection = 0;
				Cursor->Cursor = StrlenW(Selection->Text());
			}
			else if (MaxIdx < Len - Start)
			{
				// Starting from the start of the tag... but not the whole thing
				GTag *Edit = new GTag(this, 0);
				Edit->Text(NewStrW(MinTag->Text() + MinIdx + Start, MaxIdx - MinIdx));
				Edit->SetTag(NewElement);
				Edit->CopyStyle(Style);
				MinTag->Parent->Attach(Edit, Idx);
				if (NewTags) NewTags->Add(Edit);

				char16 *e = MinTag->Text() + Start + MaxIdx;
				memmove(MinTag->Text(), e, (StrlenW(e) + 1) * sizeof(*e));

				// Deselect...
				Cursor->Cursor = -1;
				Selection->Selection = -1;

				// Reselect new tag
				Cursor = Selection = Edit;
				Selection->Selection = 0;
				Cursor->Cursor = StrlenW(Selection->Text());
			}
			else
			{
				// The whole tag is changing...
				MinTag->CopyStyle(Style);
				if (!MinTag->Tag)
					MinTag->SetTag(NewElement);
				MinTag->Font = 0;
				if (NewTags) NewTags->Add(MinTag);
			}
		}
		else
		{
			// Do partial start tag change...


			// Do whole middle tags


			// Do partial end tag change...
		}

		return true;
	}

	bool HasBold()
	{
		GArray<GCss::FontWeightType> Types;
		HasStyle(GCss::PropFontWeight, Types);
		for (unsigned i=0; i<Types.Length(); i++)
		{
			if (Types[i] != GCss::FontWeightNormal &&
				Types[i] != GCss::FontWeightInherit &&
				Types[i] != GCss::FontWeightLighter)
			{
				return true;
			}
		}

		return false;
	}

	bool HasItalic()
	{
		GArray<GCss::FontStyleType> Types;
		HasStyle(GCss::PropFontStyle, Types);
		for (unsigned i=0; i<Types.Length(); i++)
		{
			if (Types[i] == GCss::FontStyleItalic)
				return true;
		}

		return false;
	}

	bool HasUnderline()
	{
		GArray<GCss::TextDecorType> Types;
		HasStyle(GCss::PropTextDecoration, Types);
		for (unsigned i=0; i<Types.Length(); i++)
		{
			if (Types[i] == GCss::TextDecorUnderline)
				return true;
		}

		return false;
	}

	bool MakeLink(char *Uri)
	{
		bool Status = false;

		GTag *c = GetCur();
		if (c)
		{
			bool DoUpdate = false;

			if (Selection)
			{
				GCss s;
				GArray<GTag*> Tags;
				DoUpdate = StylizeSelection(s, &Tags, "a");
				for (unsigned i=0; i<Tags.Length(); i++)
				{
					Tags[i]->Set("href", Uri);
					Tags[i]->SetStyle();
				}
			}

			if (DoUpdate)
			{
				Source.Reset();
				Update(true);
				OnDocumentChange();
			}
		}

		LgiAssert(IsOk());
		
		return false;
	}

	bool RemoveLink()
	{
		bool Status = false;

		GTag *c = GetCur();
		if (c)
		{
			bool DoUpdate = false;

			GArray<GTag*> Anchors;
			if (HasElement("a", &Anchors))
			{
				for (unsigned i=0; i<Anchors.Length(); i++)
				{
					GTag *t = Anchors[i];
					
					// Turn anchor back into a text node...
					t->Set("href", 0);
					t->Tag.Reset();
					t->TagId = CONTENT;
					t->Empty();
					t->Font = 0;
					t->SetStyle();
				}

				Source.Reset();
				Update(true);
				OnDocumentChange();
			}
		}

		LgiAssert(IsOk());
		
		return false;
	}


	bool OnBold()
	{
		bool Status = false;

		GTag *c = GetCur();
		if (c)
		{
			bool DoUpdate = false;

			if (Selection)
			{
				GCss s;
				s.FontWeight(HasBold() ? GCss::FontWeightNormal : GCss::FontWeightBold);
				DoUpdate = StylizeSelection(s);
			}

			if (DoUpdate)
			{
				Source.Reset();
				Update(true);
				OnDocumentChange();
			}
		}

		LgiAssert(IsOk());
		
		return false;
	}

	bool OnItalic()
	{
		bool Status = false;

		GTag *c = GetCur();
		if (c)
		{
			bool DoUpdate = false;

			if (Selection)
			{
				GCss s;
				s.FontStyle(HasItalic() ? GCss::FontStyleNormal : GCss::FontStyleItalic);
				DoUpdate = StylizeSelection(s);
			}

			if (DoUpdate)
			{
				Source.Reset();
				Update(true);
				OnDocumentChange();
			}
		}

		LgiAssert(IsOk());

		return Status;
	}

	bool OnUnderline()
	{
		bool Status = false;

		GTag *c = GetCur();
		if (c)
		{
			bool DoUpdate = false;

			if (Selection)
			{
				GCss s;
				s.TextDecoration(HasUnderline() ? GCss::TextDecorNone : GCss::TextDecorUnderline);
				DoUpdate = StylizeSelection(s);
			}

			if (DoUpdate)
			{
				Source.Reset();
				Update(true);
				OnDocumentChange();
			}
		}

		LgiAssert(IsOk());

		return Status;
	}

	GTag *GetCur()
	{
		// Gets the current cursor or sets a default one up.
		if (!Cursor)
		{
			GTag *t = Tag ? Tag->GetTagByName("html") : 0;
			if (t)
				t = t->GetTagByName("body");
			if (t)
			{
				for (unsigned i=0; i<t->Children.Length(); i++)
				{
					GTag *c = ToTag(t->Children[i]);
					if (CanHaveText(c->TagId))
					{
						Cursor = c;
						Cursor->Cursor = 0;
						break;
					}
				}
			}

			LgiAssert(Cursor != NULL);
		}

		return Cursor;
	}

	void Update(bool IsEdit)
	{
		// Updates the HTML control
		if (IsEdit)
		{
			// An edit occured..

			// Make the cursor visible
			SetCursorVis(true);
			Edit->SetIgnorePulse(true);

			// Reflow the page
			ViewWidth = -1;

			/*
			// Rebuild the block list...
			Blocks.Length(0);
			*/
		}

		// Update the screen
		Invalidate();
	}

	bool IsEditable(GTag *t)
	{
		// Returns true if the tag is editable
		return	t->TagId == TAG_LI ||
				t->Text();
	}

	void OnCursorChanged()
	{
		if (GetParent())
		{
			GetParent()->SetCtrlValue(IDC_BOLD, HasBold());
			GetParent()->SetCtrlValue(IDC_ITALIC, HasItalic());
			GetParent()->SetCtrlValue(IDC_UNDERLINE, HasUnderline());
		}
		
		Invalidate();
		SendNotify(GTVN_CURSOR_CHANGED);
	}

	bool GetLine(GTag *t, int idx, GArray<GFlowRect*> &Rects)
	{
		if (!t || idx < 0)
			return false;

		char16 *Base = t->Text();
		if (Base)
		{
			GFlowRect *Inside = 0;
			char16 *End = Base + StrlenW(Base);
			for (unsigned i=0; i<t->TextPos.Length(); i++)
			{
				GFlowRect *f = t->TextPos[i];
				
				if (f->Text >= Base && f->Text < End)
				{
					int Start = f->Text - Base;
					if (idx >= Start && idx <= Start + f->Len)
					{
						// This flow rect is where the index is
						Inside = f;
						break;
					}
				}
				else
				{
					int asd=0;
				}
			}

			if (Inside)
			{
				Rects.Add(Inside);

				// Iterate over the current tags block of text
				for (unsigned i=0; i<t->TextPos.Length(); i++)
				{
					GFlowRect *f = t->TextPos[i];
					if (f != Inside && f->OverlapY(Inside))
					{
						Rects.Add(f);
					}
				}

				// Seek backwards looking for blocks of text on the same line
				int Added = 1;
				GTag *b;
				for (b = PrevTag(t); b && Added > 0; b = PrevTag(b))
				{
					Added = 0;
					for (unsigned i=0; i<b->TextPos.Length(); i++)
					{
						GFlowRect *f = b->TextPos[i];
						
						if (f->OverlapY(Inside))
						{
							Rects.Add(f);
							Added++;
						}
					}
				}

				// Seek forwards looking for blocks of text on the same line
				Added = 1;
				for (b = NextTag(t); b && Added > 0; b = NextTag(b))
				{
					Added = 0;
					for (unsigned i=0; i<b->TextPos.Length(); i++)
					{
						GFlowRect *f = b->TextPos[i];
						if (f->OverlapY(Inside))
						{
							Rects.Add(f);
							Added++;
						}
					}
				}
			}
		}

		Rects.Sort(XSort);
		return Rects.Length() > 0;
	}

	void SetCursor(GTag *NewCur, int NewPos, bool Selecting = false)
	{
		if (NewCur)
		{
			// Selection stuff
			if (Selecting && !Selection)
			{
				// Selection is starting, save current cursor as the selection end.
				Selection = Cursor;
				Selection->Selection = Cursor->Cursor;
			}
			else if (!Selecting && Selection)
			{
				// Selection ending...
				Selection->Selection = -1;
				Selection = 0;
			}

			// Set new cursor
			if (Cursor != NewCur)
			{
				Cursor->Cursor = -1;
				Cursor = NewCur;
			}
			Cursor->Cursor = NewPos;

			// Update the window
			SetCursorVis(true);
			Edit->SetIgnorePulse(true);
			OnCursorChanged();

			// Check if we need to scroll the window at all
			if (VScroll)
			{
				LgiYield(); // Cursor position updates on paint

				int LineY = GetFont()->GetHeight();
				int64 Pos = VScroll->Value();
				GRect Client = GetClient();
				GRect c = GetCursorPos();
				c.Offset(0, -(int)Pos * LineY);

				// LgiTrace("cli=%s c=%s\n", Client.GetStr(), c.GetStr());
				if (c.y1 < 0)
				{
					// Scroll up to keep cursor on screen
					int Lines = abs(c.y1 / LineY) + 1;
					VScroll->Value(max(0, Pos - Lines));
				}
				else if (c.y2 > Client.y2)
				{
					// Scroll down to show cursor
					int Lines = (c.y2 - Client.y2) / LineY + 1;
					VScroll->Value(Pos + Lines);
				}
			}
		}
	}

	bool MoveCursor(int Dx, int Dy, bool Selecting = false)
	{
		/*
		if (Blocks.Length() == 0)
			BuildBlocks();
		*/

		GTag *t = GetCur();
		if (!t)
			return false;

		GTag *NewCur = 0;
		int NewPos = -1;
		LgiAssert(t->Cursor >= 0);

		int Base = t->GetTextStart();
		char16 *Txt = t->Text() + Base;

		int len = StrlenW(Txt);
		if (Dx)
		{
			if (Selection && !Selecting)
			{
				// Deselecting the current selection, move the cursor
				// to one of the selection block's ends.
				int i = -1;
				GTag *t = 0;
				if (Dx < 0)
				{
					if (IsCursorFirst())
					{
						t = Cursor;
						i = Cursor->Cursor;
					}
					else
					{
						t = Selection;
						i = Selection->Selection;
					}
				}
				else
				{
					if (IsCursorFirst())
					{
						t = Selection;
						i = Selection->Selection;
					}
					else
					{
						t = Cursor;
						i = Cursor->Cursor;
					}
				}

				if (t)
				{
					Cursor->Cursor = -1;
					Selection->Selection = -1;
					Cursor = t;
					Cursor->Cursor = i;
					Selection = 0;

					OnCursorChanged();
				}
			}
			else
			{
				// Move along the current tag
				int i = t->Cursor + Dx;
				if (i >= 0)
				{
					if (i <= len)
					{
						NewCur = t;
						NewPos = i;
					}
					else
					{
						// Run off the right edge
						GRect *r = GetCursorPos();
						Block *b = FindBlock(Block::Right, r->x2, r->y1 + (r->Y() >> 1));
						if (b)
						{
							NewCur = b->t;
							NewPos = b->StartOffset();
						}
						else
						{
							// No element to the right, so go down
							b = FindBlock(Block::Down, r->x1, r->y1 + (r->Y() >> 1));
							if (b)
							{
								NewCur = b->t;
								NewPos = b->StartOffset();
							}
						}
					}
				}
				else
				{
					// Run off the left edge
					GRect *r = GetCursorPos();
					Block *b = FindBlock(Block::Left, r->x1, r->y1 + (r->Y() >> 1));
					if (b)
					{
						NewCur = b->t;
						NewPos = b->EndOffset();
					}
					else
					{
						b = FindBlock(Block::Up, r->x1, r->y1 + (r->Y() >> 1));
						if (b)
						{
							NewCur = b->t;
							NewPos = b->EndOffset();
						}
					}
				}
			}
		}
		else if (Dy)
		{
			GFont *f = t->GetFont();
			if (f)
			{
				Block *b = 0;
				GRect *r = GetCursorPos();

				if (Dy < 0)
				{
					b = FindBlock(Block::Up, r->x1, r->y1);
				}
				else
				{
					b = FindBlock(Block::Down, r->x1, r->y1 + (r->Y() >> 1));
				}

				if (b)
				{
					int Dx = r->x1 - b->x1;
					int Char = 0;
					if (b->fr)
					{
						GDisplayString Ds(f, b->fr->Text);
						Ds.CharAt(b->fr->Len);
					}
					int Start = b->fr ? (b->fr->Text - b->t->TextPos[b->t->PreText() ? 1 : 0]->Text) : 0;
					NewPos = Start + Char;
					NewCur = b->t;
				}
			}
		}

		SetCursor(NewCur, NewPos, Selecting);
		return true;
	}

	void OnMouseClick(GMouse &m)
	{
		Edit->SetIgnorePulse(true);
		SetCursorVis(true);
		GHtml::OnMouseClick(m);

		LgiAssert(IsOk());
	}

	void Insert(char *utf)
	{
		bool Status = false;

		if (!utf)
			return;

		GTag *t = GetCur();
		if (t)
		{
			Source.Reset();

			if (Selection)
				Delete();

			if (t->Cursor < 0)
			{
				if (Cursor) Cursor->Cursor = -1;
				Cursor = t;
				Cursor->Cursor = 0;
			}

			if (!t->Text())
			{
				t->Text(LgiNewUtf8To16(utf));
				t->Cursor = StrlenW(t->Text());
				Status = true;
			}
			else
			{
				char16 *w = LgiNewUtf8To16(utf);
				if (w)
				{
					int Base = t->GetTextStart();
					int Insert = t->Cursor + Base;
					int NewChars = StrlenW(w);
					int OldChars = StrlenW(t->Text());
					char16 *s = new char16[NewChars+OldChars+1];
					if (s)
					{
						if (Insert > 0)
							memcpy(s, t->Text(), Insert * sizeof(char16));
						memcpy(s + Insert, w, NewChars * sizeof(char16));
						
						int AfterChars = OldChars - Insert;
						if (AfterChars > 0)
						{
							memcpy(s + Insert + NewChars, t->Text() + Insert, AfterChars * sizeof(char16));
						}

						s[NewChars + OldChars] = 0;

						t->Text(s);
						t->Cursor += NewChars;
						Status = true;
					}

					DeleteArray(w);
				}
			}
		}

		if (Status)
		{
			Update(true);
			OnDocumentChange();
		}
	}

	bool DeleteTag(GTag *t, GArray<GTag*> *Others = 0)
	{
		if (t)
		{
			GTag *c;
			while (	t->Children.Length() &&
					(c = ToTag(t->Children.First())))
			{
				if (!DeleteTag(c, Others))
				{
					return false;
				}
			}

			t->Text(0);

			if (t->Parent)
				t->Parent->Children.Delete(t, true);

			if (Others)
				Others->Delete(t, true);

			DeleteObj(t);

			return true;
		}

		return false;
	}
	
	bool DeleteSelection()
	{
		if (!Cursor || !Selection)
			return false;

		if (Cursor->Text() &&
			Selection == Cursor)
		{
			// Delete range within one tag
			int Offset = Cursor->GetTextStart();
			char16 *t  = Cursor->Text();
			int Start  = min(Cursor->Cursor, Selection->Selection) + Offset;
			int End    = max(Cursor->Cursor, Selection->Selection) + Offset;
			int Len    = StrlenW(t + End);
			
			// Move the characters down, including the terminating NULL
			memmove(t + Start, t + End, (Len + 1) * sizeof(*t));

			// Update cursor to the start of the range, and remove the selection point
			Selection->Selection = -1;
			Selection = NULL;
			Cursor->Cursor = Start - Offset;

			return true;
		}
		else
		{
			// Delete over multiple tags.			
			bool CursorFirst     = IsCursorFirst();
			GTag *First          = CursorFirst ? Cursor : Selection;
			GTag *Last           = CursorFirst ? Selection : Cursor;
			int *FirstMarker     = CursorFirst ? &Cursor->Cursor : &Selection->Selection;
			int FirstMarkerValue = *FirstMarker;

			int FirstDepth = GetTagDepth(First);
			int LastDepth = GetTagDepth(Last);
			GTag *Opt = NULL;
			if (FirstDepth <= LastDepth)
				Opt = First->GetBlockParent();
			else
				Opt = Last->GetBlockParent();
			if (Opt)
				OptimizeTags.Add(Opt);

			// Delete from first marker to the end of that tag
			int Offset = First->GetTextStart();
			char16 *t = First->Text();
			t[Offset + *FirstMarker] = 0;
			*FirstMarker = -1;

			GArray<GTag*> PostDelete;

			// Scan through to the end marker					
			GTag *n = First;
			while ((n = NextTag(n)))
			{
				if (n == Last)
				{
					// Last tag
					t = Last->Text() + n->GetTextStart();
					int *LastMarker = CursorFirst ? &Selection->Selection : &Cursor->Cursor;
					int Len = StrlenW(t + *LastMarker) + 1;
					memmove(t, t + *LastMarker, Len * sizeof(*t));
					*LastMarker = -1;
					break;
				}
				else
				{
					// Intermediate tag
					if (!PostDelete.HasItem(n))
						PostDelete.Add(n);
				}
			}
			
			// Don't delete a parent item of the tag getting the cursor...
			for (n = First; n; n = ToTag(n->Parent))
			{
				PostDelete.Delete(n);
			}

			// Process deletes
			for (unsigned i=0; i<PostDelete.Length(); )
			{
				if (!DeleteTag(PostDelete[i], &PostDelete))
					break;
			}

			Cursor = First;
			Cursor->Cursor = FirstMarkerValue;
			Selection = NULL;
		}
		
		return true;
	}

	void Delete(bool Backwards = false)
	{
		bool Status = false;

		GTag *t = GetCur();
		if (t)
		{
			Source.Reset();

			if (Selection)
			{
				Status = DeleteSelection();
			}
			else if (t->Text())
			{
				if (MoveCursor(Backwards ? -1 : 1, 0, true))
				{
					Status = DeleteSelection();
				}
			}
		}

		if (Status)
		{
			Optimize();
			Update(true);
			OnDocumentChange();
		}
	}

	void NewLine()
	{
		bool Status = false;

		GTag *t = GetCur();
		if (t)
		{
			Source.Reset();

			if (Selection)
			{
				Delete();
			}
			
			if (t->Text())
			{
				GTag *Insert = t->TagId == CONTENT ? ToTag(t->Parent) : t;
				int Idx = t->TagId == CONTENT && t->Parent ? t->Parent->Children.IndexOf(t) + 1 : 0;
				int Base = t->GetTextStart();
				char16 *Txt = t->Text() + Base;
				int Chars = StrlenW(Txt);
				int After = Chars - t->Cursor;
				if (After > 0)
				{
					if (Insert)
					{
						// This goes onto the next line
						GAutoWString NextLn(NewStrW(Txt + t->Cursor));
						
						// Truncate the existing line
						Txt[t->Cursor] = 0;
						
						// Insert a <br> tag
						GTag *n;
						if ((n = new GTag(t->Html, 0)))
						{
							n->SetTag("br");
							Insert->Attach(n, Idx++);
						}
						
						// Insert a new CONTENT tag with the next line
						if ((n = new GTag(t->Html, 0)))
						{
							n->TagId = CONTENT;
							n->Text(NextLn.Release());
							Insert->Attach(n, Idx++);
							
							// Put cursor at the start
							Cursor->Cursor = -1;
							Cursor = n;
							Cursor->Cursor = 0;
						}

						Status = true;
					}
				}
				else
				{
					GTag *c;
					if ((c = new GTag(this, 0)))
					{
						c->SetTag("br");
						Insert->Attach(c, Idx++);

						if ((c = new GTag(this, 0)))
						{
							c->TagId = CONTENT;
							char16 Empty[] = {0};
							c->Text(NewStrW(Empty));
							Insert->Attach(c, Idx++);
							Cursor->Cursor = -1;
							Cursor = c;
							Cursor->Cursor = 0;
							Status = true;
						}
					}
				}
			}
		}

		if (Status)
		{
			Update(true);
			OnDocumentChange();
		}
	}

	bool Cut()
	{
		if (!Selection)
			return false;

		GStringPipe p;

		bool CursorFirst = IsCursorFirst();
		GTag *MinTag = CursorFirst ? Cursor : Selection;
		int MinIdx = CursorFirst ? Cursor->Cursor : Selection->Selection;
		GTag *MaxTag = CursorFirst ? Selection : Cursor;
		int MaxIdx = CursorFirst ? Selection->Selection : Cursor->Cursor;

		if (MinTag == MaxTag)
		{
			if (MinTag->Text())
			{
				int Start = MinTag->GetTextStart();
				p.Push(LgiNewUtf16To8(MinTag->Text() + Start + MinIdx, (MaxIdx - MinIdx) * sizeof(char16)));
			}
		}
		else
		{
			for (GTag *t = MinTag; t; t = NextTag(t))
			{
				char16 *s;
				if ((s = t->Text()))
				{
					int Start = t->GetTextStart();
					if (t == MinTag)
					{
						p.Push(LgiNewUtf16To8(t->Text() + Start + MinIdx));
					}
					else if (t == MaxTag)
					{
						p.Push(LgiNewUtf16To8(t->Text() + Start, (MaxIdx - Start) * sizeof(char16)));
					}
					else
					{
						p.Push(LgiNewUtf16To8(t->Text() + Start));
					}
				}
				else
				{
					switch (t->TagId)
					{
						default: break;
						case TAG_P:
						case TAG_BR:
						{
							p.Print("\n");
							break;
						}
					}
				}

				if (t == MaxTag)
					break;
			}
		}

		GClipBoard c(this);
		GAutoString txt(p.NewStr());
		c.Text(txt);

		Delete(false);

		return true;
	}

	bool Paste()
	{
		GClipBoard c(this);
		GAutoString t(c.Text());
		if (t)
		{
			Insert(t);
			return true;
		}

		return false;
	}

	bool OnKey(GKey &k)
	{
		if (k.IsChar)
		{
			if (k.Down())
			{
				if (k.c16 == VK_BACKSPACE)
				{
					if (k.Down())
					{
						Delete(true);
					}
				}
				else if (k.c16 == VK_RETURN)
				{
					if (k.Down())
					{
						NewLine();
					}
				}
				else if (k.c16 >= ' ')
				{
					GAutoString utf(LgiNewUtf16To8(&k.c16, sizeof(k.c16)));
					Insert(utf);
				}
			}
			return true;
		}
		else
		{
			switch (k.c16)
			{
				case VK_HOME:
				{
					if (k.Down())
					{
						GArray<GFlowRect*> Rects;
						if (GetLine(Cursor, Cursor->Cursor, Rects))
						{
							GFlowRect *First = Rects[0];
							if (First)
							{
								SetCursor(First->Tag, First->Start(), k.Shift());
							}
						}
					}
					return true;
					break;
				}
				case VK_END:
				{
					if (Cursor && k.Down())
					{
						GArray<GFlowRect*> Rects;
						if (GetLine(Cursor, Cursor->Cursor, Rects))
						{
							GFlowRect *Last = Rects[Rects.Length()-1];
							if (Last)
							{
								SetCursor(Last->Tag, Last->Start() + Last->Len, k.Shift());
							}
						}
					}
					return true;
					break;
				}
				case VK_DELETE:
				{
					if (k.Down())
					{
						Delete();
					}
					return true;
				}
				case VK_LEFT:
				{
					if (k.Down())
					{
						MoveCursor(-1, 0, k.Shift());
					}
					return true;
					break;
				}
				case VK_RIGHT:
				{
					if (k.Down())
					{
						MoveCursor(1, 0, k.Shift());
					}
					return true;
					break;
				}
				case VK_UP:
				{
					if (k.Down())
					{
						MoveCursor(0, -1, k.Shift());
					}
					return true;
					break;
				}
				case VK_DOWN:
				{
					if (k.Down())
					{
						MoveCursor(0, 1, k.Shift());
					}
					return true;
					break;
				}
				default:
				{
					switch (k.c16)
					{
						case 'x':
						case 'X':
						{
							if (k.Ctrl())
							{
								if (k.Down())
									Cut();
								return true;
							}
							break;
						}
						case 'c':
						case 'C':
						{
							if (k.Ctrl())
							{
								if (k.Down())
									Copy();
								return true;
							}
							break;
						}
						case 'v':
						case 'V':
						{
							if (k.Ctrl())
							{
								if (k.Down())
									Paste();
								return true;
							}
							break;
						}
					}
					break;
				}
			}
		}

		bool Status = GHtml::OnKey(k);
		LgiAssert(IsOk());
		return Status;
	}

	void SetCursorVis(bool b) { GHtml::SetCursorVis(b); }
	bool GetCursorVis() { return GHtml::GetCursorVis(); }

	struct IsOkData
	{
		bool CursorOk;
		bool SelectOk;

		IsOkData()
		{
			CursorOk = false;
			SelectOk = false;
		}
	};

	bool CheckTree(GTag *t, IsOkData &Ok)
	{
		if (!t)
			return false;

		if (t->Parent && !t->Parent->Children.HasItem(t))
		{
			LgiAssert(!"Tag heirarchy error.");
			return false;
		}

		if (Cursor == t)
		{
			if (t->Cursor < 0)
			{
				LgiAssert(!"Cursor pointer points to tag with no Cursor index.");
				return 0;
			}

			if (t->Text())
			{
				Ok.CursorOk = t->Cursor <= StrlenW(t->Text());
				if (!Ok.CursorOk)
				{
					LgiAssert(!"Cursor index outside length of text.");
				}
			}
			else
				Ok.CursorOk = true;
		}
		else if (t->Cursor >= 0)
		{
			LgiAssert(!"Tag has Cursor index without being pointed at.");
			t->Cursor = -1;
			return false;
		}

		if (Selection == t)
		{
			if (t->Selection < 0)
			{
				LgiAssert(!"Selection pointer points to tag with no Selection index.");
				return 0;
			}

			if (t->Text())
			{
				Ok.SelectOk = t->Selection <= StrlenW(t->Text());
				if (!Ok.SelectOk)
				{
					LgiAssert(!"Selection index outside length of text.");
				}
			}
			else Ok.SelectOk = true;
		}
		else if (t->Selection >= 0)
		{
			LgiAssert(!"Tag has Selection index without being pointed at.");
			return false;
		}

		for (unsigned i=0; i<t->Children.Length(); i++)
		{
			GTag *c = ToTag(t->Children[i]);
			if (!CheckTree(c, Ok))
				return false;
		}

		return true;
	}

	bool IsOk()
	{
		IsOkData Ok;
		if (!CheckTree(Tag, Ok))
			return false;
		if (Cursor && !Ok.CursorOk)
			return false;
		if (Selection && !Ok.SelectOk)
			return false;
		return true;
	}

	GTag *GetTag()
	{
		return Tag;
	}

	#ifdef _DEBUG
	struct DbgDs : public GDisplayString
	{
	public:
		COLOUR c;

		DbgDs(GFont *f, const char *s, COLOUR col) : GDisplayString(f, s)
		{
			c = col;
		}

		DbgDs(GFont *f, const char16 *s, COLOUR col) : GDisplayString(f, s)
		{
			c = col;
		}
	};

	struct DbgInf
	{
		int Depth;
		GArray<DbgDs*> Ds;

		DbgInf(GTag *t = 0)
		{
			Depth = 0;
			while (t && (t = ToTag(t->Parent)))
			{
				Depth++;
			}
		}
	};

	void OnDebug(GSurface *pDC)
	{
		GArray<DbgInf*> Inf;

		#if 0
		pDC->Colour(Rgb24(255, 0, 255), 24);
		pDC->Rectangle();
		#endif
		SysFont->Transparent(false);

		char16 Empty[] = {0};

		#define AllowEmpty(s) ((s)?(s):Empty)

		int y = pDC->Y() / 2 - 10;
		if (Cursor)
		{
			char m[256];
			GTag *t;
			int cy;
			DbgDs *ds;
			
			for (cy = y, t = PrevTag(Cursor); t && cy > 0; t = PrevTag(t))
			{
				DbgInf *Line = new DbgInf(t);
				sprintf_s(m, sizeof(m), "<%s>", t->Tag?t->Tag.Get():"CONTENT");
				Line->Ds.Add(ds = new DbgDs(SysFont, m, LC_TEXT));

				char16 *Txt = t->Text();
				if (ValidStrW(Txt))
					Line->Ds.Add(ds = new DbgDs(SysFont, Txt, t == Selection ? Rgb24(0, 222, 0) : LC_TEXT));
				else if (Txt)
					Line->Ds.Add(ds = new DbgDs(SysFont, "[empty-str]", t == Selection ? Rgb24(0, 222, 0) : Rgb24(0xdd, 0xdd, 0xdd)));
				else if (t->TagId == CONTENT)
					Line->Ds.Add(ds = new DbgDs(SysFont, "[null-ptr]", t == Selection ? Rgb24(0, 222, 0) : Rgb24(255, 0, 0)));
				Inf.AddAt(0, Line);
			}

			DbgInf *Cur = new DbgInf(Cursor);
			GAutoString CursorText(LgiNewUtf16To8(Cursor->Text(), Cursor->Cursor));
			sprintf_s(m, sizeof(m), "<%s>%s", Cursor->Tag?Cursor->Tag.Get():"CONTENT", CursorText.Get());
			Cur->Ds.Add(ds = new DbgDs(SysFont, m, Rgb24(0, 0, 255)));
			Cur->Ds.Add(ds = new DbgDs(SysFont, "[cursor]", Rgb24(255, 0, 0)));
			Cur->Ds.Add(ds = new DbgDs(SysFont, Cursor->Text()+Cursor->Cursor, Rgb24(0, 0, 255)));
			int CursorLine = Inf.Length();
			Inf.AddAt(Inf.Length(), Cur);

			SysFont->Fore(LC_TEXT);
			for (cy = y, t = NextTag(Cursor); t && cy < pDC->Y(); t = NextTag(t))
			{
				DbgInf *Line = new DbgInf(t);
				sprintf_s(m, sizeof(m), "<%s>", t->Tag?t->Tag.Get():"CONTENT");
				Line->Ds.Add(ds = new DbgDs(SysFont, m, LC_TEXT));

				char16 *Txt = t->Text();
				if (ValidStrW(Txt))
					Line->Ds.Add(ds = new DbgDs(SysFont, Txt, t == Selection ? Rgb24(0, 222, 0) : LC_TEXT));
				else if (Txt)
					Line->Ds.Add(ds = new DbgDs(SysFont, "[empty-str]", t == Selection ? Rgb24(0, 222, 0) : Rgb24(0xdd, 0xdd, 0xdd)));
				else if (t->TagId == CONTENT)
					Line->Ds.Add(ds = new DbgDs(SysFont, "[null-ptr]", t == Selection ? Rgb24(0, 222, 0) : Rgb24(255, 0, 0)));
				Inf.AddAt(Inf.Length(), Line);
			}

			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(0, 0, pDC->X(), cy-1);

			cy = (pDC->Y() / 2 - 10) - (SysFont->GetHeight() * CursorLine);
			for (unsigned i=0; i<Inf.Length(); i++)
			{
				DbgInf *Line = Inf[i];
				int x = Line->Depth * 12;
				int cy2 = cy + SysFont->GetHeight() - 1;
				pDC->Colour(LC_WORKSPACE, 24);
				pDC->Rectangle(0, cy, x-1, cy2);
				pDC->Colour(Rgb24(0xdd, 0xdd, 0xdd), 24);
				for (int a=0; a<Line->Depth; a++)
				{
					int lx = a * 12 + 6;
					pDC->Line(lx, cy, lx, cy2); 
				}

				for (unsigned n=0; n<Line->Ds.Length(); n++)
				{
					DbgDs *ds = Line->Ds[n];
					SysFont->Colour(ds->c, LC_WORKSPACE);
					ds->Draw(pDC, x, cy);
					x += ds->X();
					LgiTrace("Deleting %p\n", ds);
					DeleteObj(ds);

					pDC->Colour(LC_WORKSPACE, 24);
					pDC->Rectangle(x, cy, pDC->X(), cy2);
				}
				cy = cy2 + 1;
				DeleteObj(Line);
			}

			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle(0, cy, pDC->X(), pDC->Y());
		}
		else
		{
			GDisplayString s(SysFont, "No cursor.");
			s.Draw(pDC, 6, 6);
		}
	}
	#endif
};

class GHtmlEditPriv
{
public:
	::HtmlEdit *e;
	bool IgnorePulse;
	GCombo *SelectStyle;
	GCombo *SelectFont;
	GCombo *SelectPtSize;
	GImageList *Icons;
	Btn *IsBold;
	Btn *IsItalic;
	Btn *IsUnderline;
	Btn *ForeColour;
	Btn *BackColour;
	Btn *MakeLink;
	#ifdef _DEBUG
	GWindow *DebugWnd;
	#endif

	GHtmlEditPriv()
	{
		e = 0;
		IgnorePulse = 0;
		Icons = 0;
		IsBold = 0;
		IsItalic = 0;
		IsUnderline = 0;
		SelectStyle = 0;
		SelectFont = 0;
		SelectPtSize = 0;
		MakeLink = 0;
		ForeColour = 0;
		BackColour = 0;
		#ifdef _DEBUG
		DebugWnd = 0;
		#endif
	}

	~GHtmlEditPriv()
	{
		#ifdef _DEBUG
		DeleteObj(DebugWnd);
		#endif
		DeleteObj(Icons);
	}

	void PrepareForEdit(GTag *t)
	{
		if (!t)
			return;

		if
		(
			t->Tag &&
			(
				!_stricmp(t->Tag, "b") ||
				!_stricmp(t->Tag, "i") ||
				!_stricmp(t->Tag, "u")
			)
		)
		{
			t->SetTag("span");
		}

		if (t->TagId == TAG_BR)
		{			
			int Idx = t->Parent->Children.IndexOf(t);
			GTag *Prev = Idx > 0 ? ToTag(t->Parent->Children[Idx-1]) : NULL;
			if (!Prev || !Prev->Text())
			{
				// Insert empty content tag to allow use to insert text at this point
				GTag *Content = new GTag(t->Html, 0);
				if (Content)
				{
					char16 Empty[] = {0};
					Content->Text(NewStrW(Empty));
					t->Parent->Attach(Content, Idx >= 0 ? Idx : 0);
				}
			}
		}

		for (unsigned i=0; i<t->Children.Length(); i++)
		{
			GTag *c = ToTag(t->Children[i]);
			PrepareForEdit(c);
		}
	}
};

#ifdef _DEBUG
class DebugWnd : public GWindow
{
	GHtmlEditPriv *d;

public:
	DebugWnd(GHtmlEditPriv *priv)
	{
		d = priv;
		Name("Html Edit Debug");
		GRect r(1800, 200, 3000, 1000);
		SetPos(r);
		if (Attach(0))
		{
			Visible(true);
		}
	}

	~DebugWnd()
	{
		d->DebugWnd = 0;
	}

	void OnPaint(GSurface *pDC)
	{
		d->e->OnDebug(pDC);
	}
};
#endif

GHtmlEdit::GHtmlEdit(int Id)
{
	d = new GHtmlEditPriv;
	if (Id > 0)
		SetId(Id);

	// Setup the toolbar
	Children.Insert(d->SelectStyle = new GCombo(IDC_STYLE, 0, 0, 80, TOOLBAR_HT, 0));
	if (d->SelectStyle)
	{
		d->SelectStyle->SetTabStop(TAB_STOP);
		d->SelectStyle->Insert("Normal");
		d->SelectStyle->Insert("Heading 1");
		d->SelectStyle->Insert("Heading 2");
		d->SelectStyle->Insert("Heading 3");
	}
	Children.Insert(d->SelectFont = new GCombo(IDC_FONT, 0, 0, 80, TOOLBAR_HT, 0));
	if (d->SelectFont)
	{
		d->SelectFont->SetTabStop(TAB_STOP);
		d->SelectFont->Insert("Arial");
		d->SelectFont->Insert("Courier");
		d->SelectFont->Insert("Tahoma");
		d->SelectFont->Insert("Times");
		d->SelectFont->Insert("Verdana");
	}
	Children.Insert(d->SelectPtSize = new GCombo(IDC_POINTSIZE, 0, 0, 60, TOOLBAR_HT, 0));
	if (d->SelectPtSize)
	{
		d->SelectPtSize->SetTabStop(TAB_STOP);
		d->SelectPtSize->Insert("8 pt");
		d->SelectPtSize->Insert("9 pt");
		d->SelectPtSize->Insert("10 pt");
		d->SelectPtSize->Insert("11 pt");
		d->SelectPtSize->Insert("12 pt");
		d->SelectPtSize->Insert("13 pt");
		d->SelectPtSize->Insert("14 pt");
		d->SelectPtSize->Insert("16 pt");
		d->SelectPtSize->Insert("18 pt");
		d->SelectPtSize->Insert("22 pt");
		d->SelectPtSize->Insert("28 pt");
		d->SelectPtSize->Insert("32 pt");
	}	

	d->Icons = new GImageList(16, 16, Icons.Create());
	Children.Insert(d->IsBold = new Btn(IDC_BOLD, 0, true, d->Icons, 24));
	Children.Insert(d->IsItalic = new Btn(IDC_ITALIC, 1, true, d->Icons, 20));
	Children.Insert(d->IsUnderline = new Btn(IDC_UNDERLINE, 2, true, d->Icons, 24));
	d->IsUnderline->SpaceAfter = 8;
	Children.Insert(d->ForeColour = new Btn(IDC_FORE_COLOUR, 3, false, d->Icons, 24));
	Children.Insert(d->BackColour = new Btn(IDC_BACK_COLOUR, 4, false, d->Icons, 24));
	d->BackColour->SpaceAfter = 8;
	Children.Insert(d->MakeLink = new Btn(IDC_MAKE_LINK, 5, false, d->Icons, 24));

	#ifdef _DEBUG
	d->MakeLink->SpaceAfter = 8;
	Children.Insert(new Btn(IDC_DEBUG_WND, false, "Debug"));
	#endif

	Children.Insert(d->e = new ::HtmlEdit(this));

	OnPosChange();
}

GHtmlEdit::~GHtmlEdit()
{
	DeleteObj(d);
}

int GHtmlEdit::OnNotify(GViewI *c, int f)
{
	switch (c->GetId())
	{
		case IDC_HTML_EDIT:
		{
			#ifdef _DEBUG
			if (TestFlag(f, GTVN_CURSOR_CHANGED) ||
				TestFlag(f, GTVN_DOC_CHANGED))
			{
				if (d->DebugWnd)
					d->DebugWnd->Invalidate();

			}
			#endif

			SendNotify(f);
			return 0;
			break;
		}
		case IDC_BOLD:
		{
			d->IsBold->Value(d->e->OnBold());
			break;
		}
		case IDC_ITALIC:
		{
			d->IsItalic->Value(d->e->OnItalic());
			break;
		}
		case IDC_UNDERLINE:
		{
			d->IsUnderline->Value(d->e->OnUnderline());
			break;
		}
		case IDC_MAKE_LINK:
		{
			if (d->e->HasElement("a"))
			{
				d->e->RemoveLink();
			}
			else
			{
				GInput i(this, 0, "Specify the URL:", "Link");
				if (i.DoModal())
					d->e->MakeLink(i.Str);
			}
			break;
		}
		#ifdef _DEBUG
		case IDC_DEBUG_WND:
		{
			if (!d->DebugWnd)
				d->DebugWnd = new DebugWnd(d);
			else
				DeleteObj(d->DebugWnd);
			break;
		}
		#endif
	}

	return GDocView::OnNotify(c, f);
}

void GHtmlEdit::SetIgnorePulse(bool b)
{
	d->IgnorePulse = b;
}

void GHtmlEdit::OnCreate()
{
	OnPosChange();
	AttachChildren();
	if (d->e)
	{
		d->e->Focus(true);
	}
	SetPulse(750);
}

void GHtmlEdit::OnPosChange()
{
	int x = 0;
	int y = 0;
	GRect Client = GetClient();
	if (Client.X() > 16)
	{
		Btn *btn;
		for (GViewI *c = Children.First(); c; c = Children.Next())
		{
			if (c != (GView*)d->e)
			{
				GRect p = c->GetPos();
				p.Offset(x - p.x1, y - p.y1);
				if (x > 0 && p.x2 > Client.x2)
				{
					x = 0;
					y += 20;
					p.Offset(x - p.x1, y - p.y1);
				}
				c->SetPos(p);
				x += p.X();
				if (dynamic_cast<GCombo*>(c))
					x += 8;
				else if ((btn = dynamic_cast<Btn*>(c)))
					x += btn->SpaceAfter;
			}
		}
	}

	if (d->e)
	{
		GRect c = GetClient();
		c.y1 += y + TOOLBAR_HT + 6;
		if (c.Valid())
		{
			d->e->SetPos(c);
			d->e->Visible(true);
		}
		else
		{
			d->e->Visible(false);
		}
	}
}

void GHtmlEdit::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

char *GHtmlEdit::Name()
{
	return d->e->Name();
}

bool GHtmlEdit::Name(const char *s)
{
	d->e->OriginalSrcW.Reset(LgiNewUtf8To16(s));
	bool r = d->e->Name(s);
	if (r)
	{
		d->PrepareForEdit(d->e->GetTag());
		d->e->GetCur();
	}

	return r;
}

void GHtmlEdit::OnPulse()
{
	if (d->e->Focus())
	{
		if (!d->IgnorePulse)
		{
			d->e->SetCursorVis(!d->e->GetCursorVis());
		}
	}
	else
	{
		d->e->SetCursorVis(false);
	}

	d->IgnorePulse = false;
}

bool GHtmlEdit::Sunken()
{
	return d->e ? d->e->Sunken() : false;
}

void GHtmlEdit::Sunken(bool i)
{
	if (d->e)
		d->e->Sunken(i);
}

void DumpNode(GTag *Tag, GTreeItem *Item)
{
	static char s[512];
	const char *TagName;
	if (Tag->TagId == CONTENT)
		TagName = "CONTENT";
	else if (Tag->TagId == ROOT)
		TagName = "ROOT";
	else if (Tag->Tag)
		TagName = Tag->Tag;
	else
		TagName = "NULL";

	sprintf_s(s, sizeof(s), "%p %s", Tag, TagName);
	Item->SetText(s, 0);

	sprintf_s(s, sizeof(s), "%i,%i - %i,%i", Tag->Pos.x, Tag->Pos.y, Tag->Size.x, Tag->Size.y);
	Item->SetText(s, 1);
	
	char16 *t = Tag->Text();
	if (t)
	{
		int ch = sprintf_s(s, sizeof(s), "%i ", Tag->GetTextStart());
		int i;
		for (i=0; t[i] && ch < sizeof(s) - 16; i++)
		{
			if (Tag->Cursor == i)
				ch += sprintf_s(s+ch, sizeof(s)-ch, "[cur]");
			if (Tag->Selection == i)
				ch += sprintf_s(s+ch, sizeof(s)-ch, "[sel]");
			
			if (t[i] == ' ')
				ch += sprintf_s(s+ch, sizeof(s)-ch, ".");
			else if (t[i] == '\n')
				ch += sprintf_s(s+ch, sizeof(s)-ch, "\\n");
			else if (t[i] == '\t')
				ch += sprintf_s(s+ch, sizeof(s)-ch, "\\t");
			else
				ch += sprintf_s(s+ch, sizeof(s)-ch, "%C", t[i]);
		}
		if (Tag->Cursor == i)
			ch += sprintf_s(s+ch, sizeof(s)-ch, "[cur]");
		if (Tag->Selection == i)
			ch += sprintf_s(s+ch, sizeof(s)-ch, "[sel]");
		s[ch++] = 0;
		Item->SetText(s, 2);
	}
	
	for (unsigned i=0; i<Tag->Children.Length(); i++)
	{
		GTreeItem *ci = new GTreeItem;
		if (ci)
		{
			Item->Insert(ci);
			GTag *c = dynamic_cast<GTag*>(Tag->Children[i]);
			if (c)
				DumpNode(c, ci);
			else
				ci->SetText("NULL child");
		}		
	}
	
	Item->Expanded(true);
}

void GHtmlEdit::DumpNodes(GTree *Out)
{
	if (!Out) return;
	
	Out->Empty();
	Out->ShowColumnHeader(true);
	Out->AddColumn("Tag", 190);
	Out->AddColumn("Pos", 80);
	Out->AddColumn("Text", 280);
	
	GTag *RootTag = d->e->GetTag();
	GTreeItem *RootItem = new GTreeItem;
	if (RootTag && RootItem)
	{
		Out->Insert(RootItem);
		DumpNode(RootTag, RootItem);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
class GHtmlEdit_Factory : public GViewFactory
{
	GView *NewView(const char *Class, GRect *Pos, const char *Text)
	{
		if (_stricmp(Class, "GHtmlEdit") == 0)
		{
			return new GHtmlEdit;
		}

		return 0;
	}

} GHtmlEdit_Factory;
