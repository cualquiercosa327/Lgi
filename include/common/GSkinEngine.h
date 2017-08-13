#ifndef _GSKIN_ENGINE_H_
#define _GSKIN_ENGINE_H_

#include "LList.h"

// Feature support flags
#define GSKIN_COLOUR		0x00000001
#define GSKIN_BUTTON		0x00000002
#define GSKIN_EDIT			0x00000004
#define GSKIN_CHECKBOX		0x00000008
#define GSKIN_RADIO			0x00000010
#define GSKIN_GROUP			0x00000020
#define GSKIN_SLIDER		0x00000040
#define GSKIN_COMBO			0x00000080
#define GSKIN_BITMAP		0x00000100
#define GSKIN_PROGRESS		0x00000200
#define GSKIN_TREE			0x00000400
#define GSKIN_LIST			0x00000800
#define GSKIN_LISTCOL		0x00001000
#define GSKIN_TABVIEW		0x00002000

// Drawing state
class GSkinState
{
	GArray<GDisplayString*> Tmp;
	
public:
	int Size;						// Class size, for version checking
	GSurface *pScreen;				// Output surface
	GArray<GDisplayString*> *aText;	// Array of display strings for the view
	GDisplayString **ptrText;		// Ptr to ptr for display string
	GRect Rect;						// Region to paint (if relevant)
	bool MouseOver;					// TRUE if the mouse is over the view
	int64 Value;					// Value of the control if available
	bool Enabled;					// TRUE if the control is enabled
	GSurface *Image;				// Any icon that needs displaying

	GSkinState()
	{
		aText = NULL;
		ptrText = NULL;
		Value = 0;
		Enabled = true;
		Size = sizeof(*this);
		pScreen = 0;
		MouseOver = false;
		Image = NULL;
	}

	size_t TextObjects()
	{
		if (aText)
			return aText->Length();
		if (ptrText)
			return *ptrText != NULL ? 1 : 0;
		return 0;
	}
	
	GDisplayString *FirstText()
	{
		if (aText)
		{
			return aText->Length() > 0 ? aText->First() : NULL;
		}
		else if (ptrText)
		{
			return *ptrText;
		}
		
		return NULL;
	}
	
	GArray<GDisplayString*> *AllText()
	{
		if (aText)
			return aText;
		
		if (ptrText && *ptrText && Tmp.Length(1))
		{
			Tmp[0] = *ptrText;
			return &Tmp;
		}
		
		return NULL;
	}
};

typedef void (*ProcColumnPaint)(void *UserData, GSurface *pDC, GRect &r, bool FillBackground);

// Engine class
class GSkinEngine
{
public:
	virtual ~GSkinEngine() {}

	// Return the features the skin supports. Return the
	// bitwise OR of all the features you support (the GSKIN_?? flags)
	//
	// The class is designed to be extended by adding more virtual functions
	// on the bottom and then Lgi queries the class to see if it can call them
	// by using the flags returned from this function. At no time should the
	// existing functions change order or be removed.
	virtual uint32 GetFeatures() { return 0; }	

	// Return the RGB24 for the LC_??? colour index
	// Will only be called if you return GSKIN_COLOUR from GetFeatures()
	virtual uint32 GetColour(int i) { return 0; }

	// Do painting for the various controls, the relevant GSKIN_??? flag needs to
	// be returned from GetFeatures before you can call any of these.
	virtual void OnPaint_GText        (GText *Ctrl,        GSkinState *State) {};
	virtual void OnPaint_GButton      (GButton *Ctrl,      GSkinState *State) {};
	virtual void OnPaint_GEdit        (GEdit *Ctrl,        GSkinState *State) {};
	virtual void OnPaint_GCheckBox    (GCheckBox *Ctrl,    GSkinState *State) {};
	virtual void OnPaint_GRadioGroup  (GRadioGroup *Ctrl,  GSkinState *State) {};
	virtual void OnPaint_GRadioButton (GRadioButton *Ctrl, GSkinState *State) {};
	virtual void OnPaint_GTabView     (GTabView *Ctrl,     GSkinState *State) {};
	virtual void OnPaint_GSlider      (GSlider *Ctrl,      GSkinState *State) {};
	virtual void OnPaint_GCombo       (GCombo *Ctrl,       GSkinState *State) {};
	virtual void OnPaint_GBitmap      (GBitmap *Ctrl,      GSkinState *State) {};
	virtual void OnPaint_GProgress    (GProgress *Ctrl,    GSkinState *State) {};
	virtual void OnPaint_GTree        (GTree *Ctrl,        GSkinState *State) {};
	virtual void OnPaint_LList        (LList *Ctrl,        GSkinState *State) {};
	
	// 'Col' may be NULL in the case that the GList control wants to draw the
	// header beyond the last column. This function can use the call back
	// GListColumn::OnPaint_Content to draw the text, icon and mark.
	virtual void OnPaint_ListColumn(ProcColumnPaint Callback, void *UserData, GSkinState *State) = 0;

	// Get the default font for a control
	virtual GFont *GetDefaultFont(char *Class) { return SysFont; }

	// Fills an abitary path with the skin's default fill...
	virtual void FillPath(class GPath *Path, GSurface *pDC, bool Down, bool Enabled = true) {}

	// Draws a button
	virtual void DrawBtn(GSurface *pDC, GRect &r, GColour *Base, bool Down, bool Enabled, bool Default = false) = 0;

	///////////////////////////////////////////////////////////////////////////////
	// Add new features down here with an associated feature flag defined above. //
	///////////////////////////////////////////////////////////////////////////////
};

#define LgiSkinEntryPoint		"CreateSkinEngine"
typedef GSkinEngine				*(*Proc_CreateSkinEngine)(class GApp *App);

#endif