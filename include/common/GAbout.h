/// \file
/// \author Mathew Allen (fret@memecode.com)

#ifndef __GABOUT_H
#define __GABOUT_H

#include "GDocView.h"

/// A simple about dialog
class GAbout : public GDialog, public GDefaultDocumentEnv
{
public:
	/// Constructor
	GAbout
	(
		/// The parent window
		GView *parent,
		/// The application name
		const char *AppName,
		/// The version
		const char *Ver,
		/// The description of the application
		const char *Text,
		/// The filename of a graphic to display
		const char *AboutGraphic,
		/// URL for the app
		const char *Url,
		/// Support email addr for the app
		const char *Email
	);

	int OnNotify(GViewI *Ctrl, int Flags);
};

#endif
