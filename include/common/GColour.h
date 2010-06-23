/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A colour selector control

#ifndef __GCOLOUR_H
#define __GCOLOUR_H

#include "GPopup.h"

/// \brief 32-bit colour button.
class GColour :
	public GDropDown,
	public ResObject
{
	friend class GColourPopup;

	COLOUR c32;
	GArray<COLOUR> Presets;

public:
	enum
	{
		Transparent = 0
	};

	GColour(COLOUR *list = 0, int items = 0);

	// Methods
	void SetColourList(COLOUR *c32_list, int items);

	// GView
	int64 Value();
	void Value(int64 i);
	void OnPaint(GSurface *pDC);
};

#endif
