/// \file
/// \author Matthew Allen

#ifndef _GXML_TREE_UI_H_
#define _GXML_TREE_UI_H_

#include "GXmlTree.h"
#include "GVariant.h"
#include "GList.h"


/// This class allows you to serialize data between the user interface and a
/// GXmlTag instance. Useful for storing user visible application settings in
/// XML.
class GXmlToUi
{
	class GXmlToUiPriv *d;

public:
	GXmlToUi();
	virtual ~GXmlToUi();

	typedef GListItem *(*CreateListItem)(void *User);
	typedef GTreeItem *(*CreateTreeItem)(void *User);

	/// Create attribute <-> UI element mapping for generic control
	void Map(char *Attr, int UiIdent, int Type = GV_NULL);
	/// Create attribute <-> UI element mapping for GList control
	void Map(char *Element, int UiIdent, CreateListItem Factory, char *ChildElements, void *User = 0);
	/// Create attribute <-> UI element mapping for GTree control
	void Map(char *Element, int UiIdent, CreateTreeItem Factory, char *ChildElements, void *User = 0);
	/// Clear all mappings
	void EmptyMaps();
	/// Convert data to/from an XML tag
	virtual bool Convert(GDom *Tag, GViewI *ui, bool ToUI);
	/// Disable/enable all control
	void EnableAll(GViewI *ui, bool Enable);
};

#endif