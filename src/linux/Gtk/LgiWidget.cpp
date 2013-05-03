#include "Lgi.h"

using namespace Gtk;
#include "LgiWidget.h"
#include "gdk/gdkkeysyms.h"

static void lgi_widget_class_init(LgiWidgetClass *klass);
static void lgi_widget_init(LgiWidget *w);

static void
lgi_widget_forall(	GtkContainer   *container,
					gboolean	include_internals,
					GtkCallback     callback,
					gpointer        callback_data)
{
	LgiWidget *w = LGI_WIDGET(container);
	for (int i=0; i<w->child.Length(); i++)
		(*callback)(w->child[i].w, callback_data);
}

static
GType lgi_widget_child_type(GtkContainer   *container)
{
	return GTK_TYPE_WIDGET;
}

GtkType
lgi_widget_get_type(void)
{
	static GtkType lgi_widget_type = 0;

	if (!lgi_widget_type)
	{
		static const GtkTypeInfo lgi_widget_info =
		{
			"LgiWidget",
			sizeof(LgiWidget),
			sizeof(LgiWidgetClass),
			(GtkClassInitFunc) lgi_widget_class_init,
			(GtkObjectInitFunc) lgi_widget_init,
			NULL,
			NULL,
			(GtkClassInitFunc) NULL
		};
		lgi_widget_type = gtk_type_unique(GTK_TYPE_CONTAINER, &lgi_widget_info);
	}

	return lgi_widget_type;
}

GtkWidget *lgi_widget_new(GViewI *target, int w, int h, bool pour_largest)
{
	LgiWidget *p = LGI_WIDGET(gtk_type_new(lgi_widget_get_type()));
	if (p)
	{
		p->target = target;
		p->w = w;
		p->h = h;
		p->pour_largest = pour_largest;

        if (target->GetTabStop())
        {
			#if GtkVer(2, 18)
            gtk_widget_set_can_focus(GTK_WIDGET(p), TRUE);
			#else			
			GTK_OBJECT_FLAGS(GTK_WIDGET(p)) |= GTK_CAN_FOCUS;
			#endif
        }

		gtk_widget_add_events(GTK_WIDGET(p), GDK_ALL_EVENTS_MASK);
	}	
	return GTK_WIDGET(p);
}

static void
lgi_widget_remove(GtkContainer *wid, GtkWidget *child)
{
	LgiWidget *p = LGI_WIDGET(wid);
	if (p)
	{
		for (int i=0; i<p->child.Length(); i++)
		{
			_LgiWidget::ChildInfo &c = p->child[i];
			if (c.w == child)
			{
				bool widget_was_visible = GTK_WIDGET_VISIBLE(child);
				gtk_widget_unparent(child);
				p->child.DeleteAt(i, true);
				if (widget_was_visible)
					gtk_widget_queue_resize(GTK_WIDGET(wid));
				break;
			}
		}
	}
}

static gboolean lgi_widget_click(GtkWidget *widget, GdkEventButton *ev)
{
    bool BtnDown =  ev->type == GDK_BUTTON_PRESS ||
                    ev->type == GDK_2BUTTON_PRESS ||
                    ev->type == GDK_3BUTTON_PRESS;

    // LgiTrace("click %i,%i,%i,%i,%i,%i,%s,%i,%i\n", ev->axes, ev->button, ev->device, ev->send_event, ev->state, ev->time, BtnDown?"down":"up", ev->x, ev->y);

	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        GMouse m;
        m.Target = v;
        m.x = ev->x;
        m.y = ev->y;
        m.Double(ev->type == GDK_2BUTTON_PRESS ||
                ev->type == GDK_3BUTTON_PRESS);
        m.Down( ev->type == GDK_BUTTON_PRESS ||
                ev->type == GDK_2BUTTON_PRESS ||
                ev->type == GDK_3BUTTON_PRESS);
        m.Left(ev->button == 1);
        m.Middle(ev->button == 2);
        m.Right(ev->button == 3);

		// LgiTrace("%s::OnMouseClick %i,%i\n", v->GetClass(), m.x, m.y);
        v->_Mouse(m, false);
    }    
    return TRUE;
}

static gboolean lgi_widget_motion(GtkWidget *widget, GdkEventMotion *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        // LgiTrace("motion %i,%i,%i,%i,%x,%i,%i,%i,%i\n", ev->axes, ev->device, ev->is_hint, ev->send_event, ev->state, ev->time, ev->type, ev->x, ev->y);
        GMouse m;
        m.Target = v;
        m.x = ev->x;
        m.y = ev->y;
        m.Down((ev->state & GDK_BUTTON_PRESS_MASK) != 0);
        m.Left(ev->state & 0x100);
        m.Middle(ev->state & 0x200);
        m.Right(ev->state & 0x400);

		// m.Trace("Move");
        v->_Mouse(m, true);
    }    
    return TRUE;
}

static gboolean lgi_widget_scroll(GtkWidget *widget, GdkEventScroll *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
		double Lines = ev->direction == GDK_SCROLL_DOWN ? 3 : -3;
		// LgiTrace("%s::OnMouseWheel %g\n", v->GetClass(), Lines);
        v->OnMouseWheel(Lines);
    }    
	return TRUE;
}

static gboolean lgi_widget_mouse_enter_leave(GtkWidget *widget, GdkEventCrossing *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        GMouse m;
        m.Target = v;
        m.x = ev->x;
        m.y = ev->y;

		if (ev->type == GDK_LEAVE_NOTIFY)
		{
			// LgiTrace("%s::OnMouseExit %i,%i\n", v->GetClass(), m.x, m.y);
			v->OnMouseExit(m);
		}
		else
		{
			// LgiTrace("%s::OnMouseEnter %i,%i\n", v->GetClass(), m.x, m.y);
			v->OnMouseEnter(m);
		}
    }    
	return TRUE;
}

static gboolean lgi_widget_client_event(GtkWidget *wid, GdkEventClient *ev)
{
	LgiWidget *p = LGI_WIDGET(wid);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
		GMessage m(ev->data.l[0], ev->data.l[1], ev->data.l[2]);
        v->OnEvent(&m);
    }
	return TRUE;
}

static gboolean lgi_widget_focus_event(GtkWidget *wid, GdkEventFocus *e)
{
	LgiWidget *p = LGI_WIDGET(wid);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
		char buf[1024];
		int ch = 0;
		::GArray<GViewI*> a;
		for (GViewI *i = v; i; i = i->GetParent())
		{
			a.Add(i);
		}
		for (int n=a.Length()-1; n>=0; n--)
		{
			ch += sprintf_s(buf + ch, sizeof(buf) - ch, "> %s \"%-.8s\" ", a[n]->GetClass(), a[n]->Name());
		}
		LgiTrace("%s : focus=%i\n", buf, e->in);
		
        v->OnFocus(e->in);
    }
    else LgiAssert(0);
    
	return TRUE;
}

void BuildTabStops(GViewI *v, ::GArray<GViewI*> &a)
{
    if (v->Enabled() &&
        v->Visible() &&
        v->GetTabStop())
        a.Add(v);
    
    GAutoPtr<GViewIterator> it(v->IterateViews());
    for (GViewI *c = it->First(); c; c = it->Next())
    {
        if (c->Enabled() &&
            c->Visible())
            BuildTabStops(c, a);
    }
}

static gboolean lgi_widget_key_event(GtkWidget *wid, GdkEventKey *e)
{
    #if 1
    // This is useful for debugging...
    if (e->keyval == GDK_Shift_L ||
        e->keyval == GDK_Shift_R ||
        e->keyval == GDK_Control_L ||
        e->keyval == GDK_Control_R ||
        e->keyval == GDK_Alt_L ||
        e->keyval == GDK_Alt_R)
    {
        return TRUE;
    }
    #endif

	LgiWidget *p = LGI_WIDGET(wid);
    GView *v = dynamic_cast<GView*>(p->target);
    if (v)
    {
        GKey k;
        k.Down(e->type == GDK_KEY_PRESS);
        k.c16 = k.vkey = e->keyval;
        k.Shift((e->state & 1) != 0);
        k.Ctrl((e->state & 4) != 0);
        k.Alt((e->state & 8) != 0);
        
        k.IsChar = !k.Ctrl() && (k.c16 >= ' ' && k.c16 <= 0x7f);
        switch (k.c16)
        {
            case GDK_ISO_Left_Tab:
            case GDK_Tab:       k.c16 = VK_TAB; k.IsChar = true; break;
            case GDK_Return:    k.c16 = VK_RETURN; k.IsChar = true; break;
            case GDK_BackSpace: k.c16 = VK_BACKSPACE; k.IsChar = true; break;
            case GDK_Left:      k.vkey = VK_LEFT; break;
            case GDK_Right:     k.vkey = VK_RIGHT; break;
            case GDK_Up:        k.vkey = VK_UP; break;
            case GDK_Down:      k.vkey = VK_DOWN; break;
            case GDK_Home:      k.vkey = VK_HOME; break;
            case GDK_End:       k.vkey = VK_END; break;
        }        

        GWindow *w = v->GetWindow();
        if (w)
        {
            if (!w->HandleViewKey(v, k) &&
                (k.vkey == GDK_Tab || k.vkey == GDK_ISO_Left_Tab)&&
                k.Down())
            {
                // Do tab between controls
                ::GArray<GViewI*> a;
                BuildTabStops(w, a);
                int idx = a.IndexOf((GViewI*)v);
                if (idx >= 0)
                {
                    idx += k.Shift() ? -1 : 1;
                    int next_idx = idx == 0 ? a.Length() -1 : idx % a.Length();                    
                    GViewI *next = a[next_idx];
                    if (next)
                    {
                        // LgiTrace("Setting focus to %i of %i: %s, %s, %i\n", next_idx, a.Length(), next->GetClass(), next->GetPos().GetStr(), next->GetId());
                        next->Focus(true);
                    }
                }
            }
        }
        else
            v->OnKey(k);
        
        
    }
    return TRUE;
}

static void
lgi_widget_destroy(GtkObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(LGI_IS_WIDGET(object));

	LgiWidget *p = LGI_WIDGET(object);
	void *klass = gtk_type_class(gtk_widget_get_type());

	if (GTK_OBJECT_CLASS(klass)->destroy)
	{
		(* GTK_OBJECT_CLASS(klass)->destroy)(object);
	}
}

static void
lgi_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(LGI_IS_WIDGET(widget));
	g_return_if_fail(allocation != NULL);

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED(widget))
	{
		gdk_window_move_resize( widget->window,
			                    allocation->x, allocation->y,
			                    allocation->width, allocation->height);
		
		LgiWidget *w = LGI_WIDGET(widget);
		GtkAllocation child_allocation;
		GtkRequisition child_requisition;
		guint16 border_width = GTK_CONTAINER(w)->border_width;

		for (int i=0; i<w->child.Length(); i++)
		{
			_LgiWidget::ChildInfo &c = w->child[i];
			if (GTK_WIDGET_VISIBLE(c.w))
			{
				gtk_widget_size_request(c.w, &child_requisition);
				child_allocation.x = c.x + border_width;
				child_allocation.y = c.y + border_width;

				if (GTK_WIDGET_NO_WINDOW(widget))
				{
					child_allocation.x += widget->allocation.x;
					child_allocation.y += widget->allocation.y;
				}

				child_allocation.width = child_requisition.width;
				child_allocation.height = child_requisition.height;
				gtk_widget_size_allocate(c.w, &child_allocation);
			}
		}
	}
}

static void
lgi_widget_realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	guint attributes_mask;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(LGI_IS_WIDGET(widget));

	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
	LgiWidget *w = LGI_WIDGET(widget);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = w->w;
	attributes.height = w->h;

	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y;

	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
		                            &attributes,
                            		attributes_mask);

	gdk_window_set_user_data(widget->window, widget);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	lgi_widget_size_allocate(widget, &widget->allocation);
}

static gboolean
lgi_widget_expose(GtkWidget *widget, GdkEventExpose *event)
{
	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(LGI_IS_WIDGET(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	LgiWidget *p = LGI_WIDGET(widget);
	
	if (GTK_WIDGET_DRAWABLE(widget))
	{
		if (p && p->target)
		{
			GScreenDC Dc(widget);
			GView *v = dynamic_cast<GView*>(p->target);
			if (v)
			    v->_Paint(&Dc);
			else
			    p->target->OnPaint(&Dc);
		}
		else printf("%s:%i - No view to paint widget.\n", _FL);
	}

	return FALSE;
}

void
lgi_widget_setsize(GtkWidget *wid, int width, int height)
{
	LgiWidget *p = LGI_WIDGET(wid);
	if (p)
	{
	    if (p->w != width ||
	        p->h != height)
	    {
		    p->w = width;
		    p->h = height;
		    
		    wid->requisition.width = width;
		    wid->requisition.height = height;
		}
	}
}

void
lgi_widget_setchildpos(GtkWidget *parent, GtkWidget *child, int x, int y)
{
	if (!LGI_IS_WIDGET(parent))
	{
		LgiAssert(0); // should this ever happen?
		return;
	}

	LgiWidget *p = LGI_WIDGET(parent);
	if (p)
	{
		for (int i=0; i<p->child.Length(); i++)
		{
			_LgiWidget::ChildInfo &c = p->child[i];
			if (c.w == child)
			{
				c.x = x;
				c.y = y;
				
				if (GTK_WIDGET_REALIZED(c.w))
				{
                	LgiWidget *child_wid = LGI_WIDGET(c.w);
				    GtkAllocation a;
				    a.x = c.x;
				    a.y = c.y;
				    a.width = child_wid->w;
				    a.height = child_wid->h;
    				gtk_widget_size_allocate(c.w, &a);

            		gdk_window_invalidate_rect(
												#if GtkVer(2, 14)
            									gtk_widget_get_window(c.w)
            									#else
            									c.w->window
							            		#endif
            									, &a, FALSE);
				}
				return;
			}
		}
	}
}

void
lgi_widget_add(GtkContainer *wid, GtkWidget *child)
{
	LgiWidget *p = LGI_WIDGET(wid);
	if (p)
	{
		for (int i=0; i<p->child.Length(); i++)
		{
			_LgiWidget::ChildInfo &c = p->child[i];
			if (c.w == child)
			{
				printf("%s:%i - Already a child.\n", _FL);
				return;
			}
		}
		
		_LgiWidget::ChildInfo &c = p->child.New();
		c.w = child;
		c.x = 0;
		c.y = 0;
		gtk_widget_set_parent(child, GTK_WIDGET(wid));
	}
}

static void
lgi_widget_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(LGI_IS_WIDGET(widget));
	g_return_if_fail(requisition != NULL);

	LgiWidget *p = LGI_WIDGET(widget);
	if (p->pour_largest)
	{
	    requisition->width = 10;
	    requisition->height = 10;
	}
	else
	{
        requisition->width = p->w;
        requisition->height = p->h;
	}

	// LgiTrace("%s::req %i,%i\n", p->target->GetClass(), requisition->width, requisition->height);
}

gboolean
lgi_widget_configure(GtkWidget *widget, GdkEventConfigure *ev)
{
	LgiWidget *p = LGI_WIDGET(widget);
	printf("lgi_widget_configure %p\n", p);
	if (p)
	{
	    p->target->OnPosChange();
    }    
    return TRUE;
}

static void
lgi_widget_class_init(LgiWidgetClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *)klass;
	object_class->destroy = lgi_widget_destroy;

	GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
	widget_class->realize = lgi_widget_realize;
	// widget_class->configure_event = lgi_widget_configure;
	widget_class->size_request = lgi_widget_size_request;
	widget_class->size_allocate = lgi_widget_size_allocate;
	widget_class->expose_event = lgi_widget_expose;
	widget_class->button_press_event = lgi_widget_click;
	widget_class->button_release_event = lgi_widget_click;
	widget_class->motion_notify_event = lgi_widget_motion;
	widget_class->scroll_event = lgi_widget_scroll;
	widget_class->enter_notify_event = lgi_widget_mouse_enter_leave;
	widget_class->leave_notify_event = lgi_widget_mouse_enter_leave;
	widget_class->client_event = lgi_widget_client_event;
	widget_class->focus_in_event = lgi_widget_focus_event;
	widget_class->focus_out_event = lgi_widget_focus_event;
	widget_class->key_press_event = lgi_widget_key_event;
	widget_class->key_release_event = lgi_widget_key_event;

	GtkContainerClass *container_class = (GtkContainerClass*)klass;
	container_class->add = lgi_widget_add;
	container_class->remove = lgi_widget_remove;
	container_class->forall = lgi_widget_forall;
	container_class->child_type = lgi_widget_child_type;
}

static void
lgi_widget_init(LgiWidget *w)
{
	GTK_WIDGET_UNSET_FLAGS(w, GTK_NO_WINDOW);
	w->target = 0;
	w->w = 0;
	w->h = 0;
	w->pour_largest = false;
}

