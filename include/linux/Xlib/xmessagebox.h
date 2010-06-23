

#ifndef __QMessageBox_h
#define __QMessageBox_h

#include "LgiLinux.h"

class QMessageBox : public QObject
{
public:
	enum
	{
		Ok,
		Yes,
		No,
		Cancel,
		NoButton
	};

	enum
	{
		NoIcon
	};

	QMessageBox(char *Title, char *Msg, int i, int b1, int b2, int b3, QWidget *parent = 0, char *name = 0, bool modal = true)
	{
	}

	int exec() { return 0; }
};

#endif
