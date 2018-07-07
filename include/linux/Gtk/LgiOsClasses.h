#ifndef __OS_CLASS_H
#define __OS_CLASS_H

extern __thread int GtkLockCount;

class LgiClass GtkLock
{
public:
	GtkLock()
	{
		if (GtkLockCount == 0)
			Gtk::gdk_threads_enter();
		GtkLockCount++;
	}
	
	~GtkLock()
	{
		GtkLockCount--;
		if (GtkLockCount == 0)
			Gtk::gdk_threads_leave();
	}
};


#endif
