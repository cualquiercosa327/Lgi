#ifndef _Lvc_h_
#define _Lvc_h_

#include "Lgi.h"
#include "LList.h"
#include "GBox.h"
#include "GTree.h"
#include "GOptionsFile.h"
#include "GTextView3.h"

#define OPT_Folders	"Folders"
#define OPT_Folder	"Folder"

extern const char *AppName;

enum AppIds
{
	IDC_LIST = 100,
	IDC_TOOLS_BOX,
	IDC_FOLDERS_BOX,
	IDC_COMMITS_BOX,
	IDC_TREE,
	IDC_FILES,
	IDC_FILES_BOX,
	IDC_TXT,

	IDM_ADD = 200,
	IDM_UPDATE,
	IDM_COPY_REV,
	IDM_REMOVE,
};

enum VersionCtrl
{
	VcNone,
	VcCvs,
	VcSvn,
	VcGit,
	VcHg,
};

struct AppPriv
{
	GTree *Tree;
	LList *Lst;
	LList *Files;
	GOptionsFile Opts;
	GTextView3 *Txt;

	AppPriv()  : Opts(GOptionsFile::PortableMode, AppName)
	{
		Lst = NULL;
		Tree = NULL;
		Files = NULL;
		Txt = NULL;
	}

	void ClearFiles()
	{
		if (Files)
			Files->Empty();
		if (Txt)
			Txt->Name(NULL);
	}
};

extern VersionCtrl DetectVcs(const char *Path);

#include "VcFile.h"
#include "VcCommit.h"
#include "VcFolder.h"

#endif