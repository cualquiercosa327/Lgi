#ifndef _LGIWIDGET_H_
#define _LGIWIDGET_H_

G_BEGIN_DECLS

#define LGI_WIDGET(obj) GTK_CHECK_CAST(obj, lgi_widget_get_type(), LgiWidget)
#define LGI_WIDGET_CLASS(klass) GTK_CHECK_CLASS_CAST(klass, lgi_widget_get_type(), LgiWidgetClass)
#define LGI_IS_WIDGET(obj) GTK_CHECK_TYPE(obj, lgi_widget_get_type())

typedef struct _LgiWidget LgiWidget;
typedef struct _LgiWidgetClass LgiWidgetClass;

struct _LgiWidget
{
	GtkContainer widget;
	
	GViewI *target;
	int w, h;
	bool pour_largest;
	bool drag_over_widget;
	char *drop_format;
	bool debug;
	
	struct ChildInfo
	{
		int x;
		int y;
		GtkWidget *w;
	};
	
	::GArray<ChildInfo> child;
};

struct _LgiWidgetClass
{
	GtkContainerClass parent_class;
};

GtkType lgi_widget_get_type();
GtkWidget *lgi_widget_new(GViewI *target, int width, int height, bool pour_largest);

// Children management
void lgi_widget_add(GtkContainer *wid, GtkWidget *child);
void lgi_widget_remove(GtkContainer *wid, GtkWidget *child);

// Positioning of widgets
void lgi_widget_setsize(GtkWidget *wid, int width, int height);
void lgi_widget_setchildpos(GtkWidget *wid, GtkWidget *child, int x, int y);

G_END_DECLS

#endif