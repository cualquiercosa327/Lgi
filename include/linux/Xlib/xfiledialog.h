
#ifndef __QFileDialog_h
#define __QFileDialog_h

#include "qmainwindow.h"

class QFileDialog : public QMainWindow
{
public:
	static char *getOpenFileName(char *dir, char *mask, QWidget *parent) { return 0; }
	static char *getSaveFileName(char *dir, char *mask, QWidget *parent) { return 0; }
};

#endif
