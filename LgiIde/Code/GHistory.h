/// \file
/// \author Matthew Allen

#ifndef __GHISTORY__
#define __GHISTORY__

#include "GPopup.h"

/// A history drop down for an edit box
class GHistory : public GDropDown, public List<char>, public ResObject
{
	class GHistoryPrivate *d;

public:
	GHistory();
	~GHistory();

	// Props
	int GetTargetId();
	void SetTargetId(int id);
	
	/// Call after you change the list of strings
	void Update();
	void OnPopupClose();
	void Add(char *s);
};

#endif
