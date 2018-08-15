
#ifndef __QMenuBar_h
#define __QMenuBar_h

#include "GRect.h"
#include "qwidget.h"

/*
class QMenuData;
class QMenuBar;
class GMenuItem;

class QMenuItem : public QWidget
{
	friend class QMenuData;
	friend class QMenuBar;
	friend class QPopupMenu;
	friend class QMenuItemPrivate;

	class QMenuItemPrivate *Item;
	static QMenuItem *Cursor;
	
	int x();
	int y();

public:
	QMenuItem(QMenuBar *Menu, QMenuData *Parent, GMenuItem *Item);
	~QMenuItem();

	// Api
	int type();
	void setType(int i);
	QPopupMenu *sub();
	QMenuData *GetParent();
	void setSub(QPopupMenu *m);
	void showPopup();
	void hidePopup();
	int cmd();
	void setCmd(int i);
	void Activate();
	
	bool Checked();
	void Checked(bool i);

	// Widget
	void paintEvent(QEvent *e);
	void mousePressEvent(QEvent *e);
	void mouseReleaseEvent(QEvent *e);
	void leaveEvent(QEvent *e);
	void enterEvent(QEvent *e);
};

class QMenuData : public QObject
{
	friend class QPopupMenu;
	friend class QMenuItemPrivate;

protected:
	class QMenuDataPrivate *Data;
	QList<QMenuItem> *GetItems();

public:
	QMenuData(QMenuBar *Menu, QMenuItem *Parent);
	~QMenuData();

	// Api
	virtual QMenuItem *insertItem(GMenuItem *item, int id, int where = -1); // char *text, int cmd
	virtual QMenuItem *insertItem(GMenuItem *item, QPopupMenu *sub, int where = -1);
	virtual QMenuItem *insertSeparator(int where = -1);
	virtual void deleteItem(QMenuItem *i);

	virtual QWidget *GetWidget();

	// Events
	virtual void OnSubMenuVisible(QPopupMenu *Item, bool Visible) {}
	virtual void OnMenuClick(QMenuItem *Item) {}
	virtual void OnMenuEnter(QMenuItem *Item) {}
};

class QMenuBar : public QWidget, public QMenuData
{
	class QMenuBarPrivate *MenuBar;

public:
	QMenuBar();
	~QMenuBar();

	// Api
	GRect &Pour(GRect &p);

	// Widget
	void paintEvent(QEvent *e);

	// Impl
	void OnSubMenuVisible(QPopupMenu *Item, bool Visible);
	void OnMenuClick(QMenuItem *Item);
	void OnMenuEnter(QMenuItem *Item);

};
*/

#endif
