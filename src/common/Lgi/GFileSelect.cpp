/*hdr
**      FILE:           GFileSelect.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           20/5/2002
**      DESCRIPTION:    Common file/directory selection dialog
**
**      Copyright (C) 1998-2002, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "GPopup.h"
#include "GToken.h"
#include "GList.h"
#include "GTextLabel.h"
#include "GEdit.h"
#include "GButton.h"
#include "GCheckBox.h"
#include "GCombo.h"
#include "GTree.h"

#define FSI_FILE			0
#define FSI_DIRECTORY		1
#define FSI_BACK			2
#define FSI_UPDIR			3
#define FSI_NEWDIR			4
#define FSI_DESKTOP			5
#define FSI_HARDDISK		6
#define FSI_CDROM			7
#define FSI_FLOPPY			8
#define FSI_NETWORK			9

enum DlgType
{
	TypeNone,
	TypeOpenFile,
	TypeOpenFolder,
	TypeSaveFile
};

char ModuleName[] = "File Select";

uint32 IconBits[] = {
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xC980FA8A, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x738E738E, 0xF81F738E, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F,
0xF81FF81F, 0x8430F81F, 0x84308430, 0x84308430, 0xF81F8430, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9CE09CE0, 0x9CE09CE0, 0x00009CE0, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCCE0F81F, 0x9800FCF9, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F,
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x738E738E, 0xCE6C9E73, 0x738EC638, 0xF81F738E, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9FFF738E, 0x9FFF9FFF, 0x9FFF9FFF, 0x00009FFF, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9CE0F81F, 0xFFF9F7BE, 0xFFF3FFF9, 0x9CE0FFF3, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9CE09CE0, 0x9CE09CE0, 0x00009CE0, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0xF81FF81F, 0xF81FF81F, 0x04F904F9,
0x04F904F9, 0xAD720313, 0xAD72AD72, 0xAD72AD72, 0xFE60CCE0, 0x00009B00, 0x0000AD72, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x738EF81F, 0x667334F3, 0xCE6C9E73, 0xC638B5B6, 0x3186C638, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFFF738E, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xF81FF81F, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0xF81F738E, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCE6C9CE0, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0x9CE09CE0, 0x9CE09CE0, 0x9CE09CE0, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x9CE0F81F, 0xFFF9F7BE, 0xFFF3FFF9, 0x9CE0FFF3,
0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81F0000, 0x667F04F9, 0x031304F9, 0xFFFFCE73, 0xFFF9FFFF, 0xCCE0FFFF, 0x9B00FFF3, 0x04F90000, 0x00000313, 0xF81FF81F, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0xF81F738E, 0xF81FF81F, 0xF81FF81F, 0x6673738E, 0x34F36673, 0xCE736673, 0xB5B6C638, 0xB5B6DEFB, 0xF81F3186, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFFF738E, 0xFFFFFFFF, 0xCFFFCFFF, 0x0000CFFF, 0x738EF81F, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0x0000738E, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0x0000FFFF, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xCE6CFFF3,
0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCE6C9CE0, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0x9CE09CE0, 0x9CE09CE0, 0x9CE09CE0, 0xF81FF81F, 0xF81FF81F, 0x9CE09CE0, 0x9CE09CE0, 0xF81F0000, 0x0000F81F, 0x0000F81F, 0x0000F81F, 0xF81FF81F, 0x04F904F9, 0xCE730313, 0xFFFFFFFF, 0xFFF9FFF9, 0xFE60CCE0, 0x00009B00, 0x667F3313, 0x00000313, 0x738EF81F, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0x0000738E, 0xF81FF81F, 0xF81FF81F, 0x34F98430, 0x667364F9, 0x84306679, 0xD6BAB5B6, 0xB5B6C638, 0xF81F3186, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCFFF738E, 0xCFFFCFFF, 0xCFFFCFFF, 0x0000CFFF, 0xFFFF738E, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CF3FFFF, 0x0000738E, 0xFFFF0000, 0xFFFFFFFF,
0xFFFFFFFF, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xCE6CFE73, 0xF81F0000, 0xF81FF81F, 0x0000F81F, 0x000007FF, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xCE6CFFF3, 0xF81F0000, 0x9CE0F81F, 0xFFF9F7BE, 0xFFF3FFF3, 0x00009CE0, 0xF81FF81F, 0xF81F0000, 0xF81F0000, 0xF81FF81F, 0x031304F9, 0xFFFFCE73, 0x94B294B2, 0xCCE094B2, 0x9B00FFF3, 0xAD720000, 0x3313FFF9, 0x00000313, 0xFFFF738E, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x9CF3FFFF, 0x0000738E, 0xF81FF81F, 0x738EF81F, 0xA53494B2, 0x667964F3, 0x00008430, 0xA5348430, 0xCE79B5B6, 0x3186CE79, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCE79738E,
0xCE79C638, 0xC638B5B6, 0x0000B5B6, 0xD6BA738E, 0xC638C638, 0xC638C638, 0xC638C638, 0xB5B6C638, 0x04200660, 0x94B2B5B6, 0x0000738E, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xD699FFFF, 0xD699D699, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xCE6CFFF3, 0xF81F0000, 0xF81FF81F, 0x07FF0000, 0x000007FF, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0x0000FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xCE6CFE73, 0xF81F0000, 0xCE6C9CE0, 0xCE6CCE6C, 0xCE6CCE6C, 0x9CE0CE6C, 0x9CE09CE0, 0xF81F9CE0, 0x0000F81F, 0x0000F81F, 0xCE730313, 0xFFFFFFFF, 0xFFFF94B2, 0xFE60CCE0, 0x00009B00, 0xFFF9AD72, 0xCE73CE73, 0x00003313, 0xD6BA738E, 0xC638C638, 0xC638C638, 0xC638C638, 0xB5B6C638, 0x04200660, 0x94B2B5B6, 0x0000738E,
0xF81FF81F, 0x738EF81F, 0xB5B6B5B6, 0x8430CE79, 0xF81F0000, 0x84300000, 0xCE79CE79, 0x3186CE79, 0xF81FF81F, 0x84308430, 0x84308430, 0x84308430, 0xC638738E, 0x84308430, 0x84308430, 0x0000C638, 0xDEFB738E, 0xC638B5B6, 0xC638C638, 0xC638C638, 0xB5B6B5B6, 0xB5B6B5B6, 0x94B2B5B6, 0x0000738E, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xD699FFFF, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FE73, 0xCE6CFE73, 0xF81F0000, 0x0000F81F, 0x07FF07FF, 0x07FF07FF, 0x07FF07FF, 0x07FF07FF, 0x07FF07FF, 0xF81F0000, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0x00000000, 0xFFF30000, 0xFE73FFF3, 0xFE73FFF3, 0xCE6CFFF3, 0xF81F0000, 0xFFF99CE0, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF9FFF9, 0xFFF3FFF9, 0x0000CE6C, 0xF81F0000, 0xF81FF81F, 0xFFFFAD72, 0xFFF9FFFF, 0xFFFF94B2,
0x9B009CEC, 0x00000000, 0xCE73FFF9, 0xAD72FFF9, 0x000094B2, 0xDEFB738E, 0xC638B5B6, 0xC638C638, 0xC638C638, 0xB5B6B5B6, 0xB5B6B5B6, 0x94B2B5B6, 0x0000738E, 0xF81FF81F, 0x738EF81F, 0xF7BEE73C, 0xB5B6E73C, 0x00008430, 0xB5B68430, 0xF7BEEF7D, 0x3186CE79, 0x8430F81F, 0xC638C638, 0xC638C638, 0xC638C638, 0xCE79738E, 0x0000738E, 0xFFFF738E, 0x0000B5B6, 0xDEFB738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x94B2B5B6, 0x0000738E, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xD699FFFF, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xCE6CFFF3, 0xF81F0000, 0x0000F81F, 0x07FF07FF, 0x07FF07FF, 0x07FF07FF, 0x07FF07FF, 0x07FF07FF, 0xF81F0000, 0xF81FF81F, 0xFFF99CE0, 0x0000FFF3, 0x00000000, 0x00000000, 0xFFF3FFF3, 0xFFF3FE73,
0xCE6CFE73, 0xF81F0000, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFE73FFF3, 0x0000CE6C, 0x0000F81F, 0xF81FF81F, 0xFFFFAD72, 0xFFF9FFF9, 0xFFFF94B2, 0xFFFF0000, 0x0000FFF9, 0xFFF9CE73, 0xCE73AD72, 0x000094B2, 0xDEFB738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x94B2B5B6, 0x0000738E, 0x8430F81F, 0x84308430, 0xA5348430, 0xA534B5B6, 0x8430A534, 0xDEFB34F3, 0xFFFFE73C, 0xF81F3186, 0xFFFF8430, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000FFFF, 0x00000000, 0x00000000, 0xF81F0000, 0xD6BA738E, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x94B2B5B6, 0x0000738E, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xD699FFFF, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FE73, 0xFFF3FE73, 0xCE6CFE73, 0xF81F0000, 0xF81FF81F,
0x07FF0000, 0x000007FF, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0x0000FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xCE6CFFF3, 0xF81F0000, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FFF3, 0xFFF3FE73, 0x0000CE6C, 0xF81FF81F, 0xF81F0000, 0xFFF9AD72, 0xFFF9FFF9, 0xFFFF94B2, 0xFFFFFFFF, 0x0000FFF9, 0xCE73FFF9, 0xAD72CE73, 0x000094B2, 0xD6BA738E, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x94B2B5B6, 0x0000738E, 0xFFFF8430, 0xFFFFFFFF, 0xA534738E, 0xB5B6A534, 0xCE73D6BA, 0x34F96673, 0xB5B6CFFF, 0xF81F3186, 0xC6388430, 0xC638C638, 0xC638C638, 0xC638C638, 0xC638C638, 0xC638F800, 0x738E8430, 0xF81F0000, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0xF81F0000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
0xD699FFFF, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xFE73FFF3, 0xCE6CFE73, 0xF81F0000, 0xF81FF81F, 0x0000F81F, 0x000007FF, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0x0000FFF3, 0xFFF3FFF3, 0xFFF3FE73, 0xFFF3FE73, 0xCE6CFE73, 0xF81F0000, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FFF3, 0xFE73FFF3, 0xFE73FFF3, 0x0000CE6C, 0xF81FF81F, 0xF81FF81F, 0xFFF9AD72, 0xFFF9FFF9, 0xFFFF94B2, 0xFFFFFFFF, 0x0000FFF9, 0xCE73CE73, 0xCE73AD72, 0x000094B2, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0x738E738E, 0xF81F0000, 0xFFFF8430, 0xC638C638, 0x738EC638, 0xB5B6A534, 0xCE73CE79, 0x04F99E73, 0x000004F9, 0xF81FF81F, 0xC6388430, 0xC638C638, 0x84308430, 0x84308430, 0xC638C638, 0xC638C638, 0x738E8430,
0xF81F0000, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81FF81F, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xD699FFFF, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FE73, 0xFFF3FE73, 0xFFF3FE73, 0xFFF3FE73, 0xFE73FE73, 0xCE6CFE73, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FFF3, 0x0000FFF3, 0x00000000, 0x00000000, 0xFE730000, 0xCE6CFE73, 0xF81F0000, 0xFFF99CE0, 0xFFF3FFF3, 0xFFF3FE73, 0xFFF3FE73, 0xFFF3FE73, 0x0000CE6C, 0xF81FF81F, 0xF81FF81F, 0xFFF904F9, 0xFFF9FFF9, 0xFFF994B2, 0xFFF9FFF9, 0x0000FFF9, 0xAD72CE73, 0xAD72CE73, 0x00003313, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81FF81F, 0xFFFF8430, 0x31863186,
0x31863186, 0x84308430, 0xCE73CE79, 0x31869E73, 0x00003186, 0xF81FF81F, 0xC6388430, 0x84308430, 0x00000000, 0x00000000, 0x84308430, 0xC6388430, 0x738E8430, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xB5B6F81F, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xD699FFFF, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xCE6C9CE0, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFF99CE0, 0xFFF3FE73, 0xFFF3FE73, 0xFFF3FE73, 0xFFF3FE73, 0xFE73FE73, 0xCE6CFE73, 0xF81F0000, 0xFFF99CE0, 0xFFF3FE73, 0xFE73FFF3, 0xFE73FFF3, 0xFE73FFF3, 0x0000CE6C, 0xF81FF81F, 0xF81FF81F, 0x04F904F9, 0xFFF9FFF9, 0x00000000, 0x00000000, 0x00000000,
0xCE73AD72, 0x3313AD72, 0x00003313, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFFF8430, 0x042007E0, 0xFFFFFFFF, 0xC638C638, 0x31863186, 0xC6383186, 0x0000738E, 0xF81FF81F, 0xC6388430, 0xC638C638, 0xFFFFFFFF, 0xFFFFFFFF, 0xC638C638, 0xC638C638, 0x738E8430, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xB5B6F81F, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xFFFF0000, 0xD699D699, 0xD699D699, 0xD699D699, 0xD699D699, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xCE6C9CE0, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xF81F0000,
0xCE6C9CE0, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0xCE6CCE6C, 0x0000CE6C, 0xF81FF81F, 0xF81FF81F, 0x667F04F9, 0xFFF904F9, 0xCE73FFF9, 0xCE73FFF9, 0xAD72CE73, 0xAD72CE73, 0x667F3313, 0x00003313, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x8430738E, 0x84308430, 0x84308430, 0x84308430, 0x84308430, 0x84308430, 0x00008430, 0xF81FF81F, 0x84308430, 0x84308430, 0x84308430, 0x84308430, 0x84308430, 0x84308430, 0x00008430, 0xF81FF81F, 0xB5B6F81F, 0xB5B6F81F, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xB5B6B5B6, 0xF81FB5B6, 0xF81FB5B6, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F,
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81FF81F, 0xF81FF81F, 0x03130313, 0x04F90313, 0x94B294B2, 0x94B294B2, 0x94B294B2, 0x331394B2, 0x33133313, 0x00003313, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81FF81F, 0x0000F81F, 0x0000F81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81F0000, 0xF81F0000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F,
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F,
0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F, 0xF81FF81F};

GInlineBmp FileSelectIcons = { 160, 16, 16, IconBits };

//////////////////////////////////////////////////////////////////////////
char *GFileType::DefaultExtension()
{
	char *Status = 0;
	GToken T(Extension(), ";");
	if (T.Length())
	{
		char s[256];
		strcpy(s, T[0]);
		char *Dir = strchr(s, '.');
		if (Dir)
		{
			Status = NewStr(Dir+1);
			if (Status) strlwr(Status);
		}
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////
class GFolderItem : public GListItem
{
	class GFileSelectDlg *Dlg;
	char *Path;

public:
	char *File;
	bool IsDir;

	GFolderItem(GFileSelectDlg *dlg, char *FullPath, GDirectory *Dir);
	~GFolderItem();


	void OnActivate();
	char *GetText(int i);
	int GetImage(int Flags);
	void OnSelect();
	void OnDelete(bool Ask = true);
	void OnRename();
	void OnMouseClick(GMouse &m);
};


//////////////////////////////////////////////////////////////////////////
// This is just a private data container to make it easier to change the
// implementation of this class without effecting headers and applications.
class GFileSelectPrivate
{
	friend class GFileSelect;
	friend class GFileSelectDlg;
	friend class GFolderList;

	GView *Parent;
	GFileSelect *Select;

	DlgType Type;
	char *Title;
	char *DefExt;
	bool MultiSelect;
	List<char> Files;
	int CurrentType;
	List<GFileType> Types;
	List<char> History;
	bool ShowReadOnly;
	bool ReadOnly;
	
	bool EatClose;

public:
	static GImageList *Icons;
	static char *InitPath;
	static bool InitShowHiddenFiles;
	static GRect InitSize;

	GFileSelectPrivate(GFileSelect *select)
	{
		ShowReadOnly = false;
		ReadOnly = false;
		EatClose = false;
		Select = select;
		Type = TypeNone;
		Title = 0;
		DefExt = 0;
		MultiSelect = false;
		Parent = 0;
		CurrentType = -1;

		if (!Icons)
		{
			GSurface *pDC = FileSelectIcons.Create();
			if (pDC)
			{
				/*
				printf("pDC->Bit=%i\n", pDC->GetBits());
				if (pDC->GetBits() == 32)
				{
					COLOUR c = pDC->Get(0, 0);
					printf("c = %x\n", c);
					for (int y=0; y<pDC->Y(); y++)
					{
						uint32 *p = (uint32*)(*pDC)[y];
						uint32 *e = p + pDC->X();
						while (p < e)
						{
							if (c == *p) *p = 0;
							p++;
						}
					}
				}
				*/
				
				Icons = new GImageList(16, 16, pDC);
				if (Icons)
				{
					printf("0,0 = %x\n", Icons->Get(0, 0));
					#ifdef WIN32
					Icons->Create(pDC->X(), pDC->Y(), pDC->GetBits());
					#endif
				}
			}
		}
	}

	virtual ~GFileSelectPrivate()
	{
		DeleteArray(Title);
		DeleteArray(DefExt);

		Types.DeleteObjects();
		Files.DeleteArrays();
		History.DeleteArrays();
	}
};

GImageList *GFileSelectPrivate::Icons = 0;
char *GFileSelectPrivate::InitPath = 0;
bool GFileSelectPrivate::InitShowHiddenFiles = false;
GRect GFileSelectPrivate::InitSize(0, 0, 600, 500);

//////////////////////////////////////////////////////////////////////////
// This class implements the UI for the selector.
class GFileSelectDlg;

class GFolderView
{
protected:
	GFileSelectDlg *Dlg;

public:
	GFolderView(GFileSelectDlg *dlg)
	{
		Dlg = dlg;
	}

	virtual void OnFolder() {}
};

class GFolderDrop : public GDropDown, public GFolderView
{
public:
	GFolderDrop(GFileSelectDlg *dlg, int Id, int x, int y, int cx, int cy);

	void OnFolder();
};

class GIconButton : public GLayout
{
	GImageList *Icons;
	int Icon;
	bool Down;

public:
	GIconButton(int Id, int x, int y, int cx, int cy, GImageList *icons, int icon)
	{
		Icons = icons;
		Icon = icon;
		SetId(Id);
		GRect r(x, y, x+cx, y+cy);
		SetPos(r);
		Down = false;
		SetTabStop(true);
	}

	void OnPaint(GSurface *pDC)
	{
		GRect c=GetClient();
		c.Offset(-c.x1, -c.y1);
		LgiWideBorder(pDC, c, Down ? SUNKEN : RAISED);
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&c);
		
		int x = (c.X()-Icons->TileX()) / 2;
		int y = (c.Y()-Icons->TileY()) / 2;

		if (Focus())
		{
			#if WIN32NATIVE
			RECT r = c;
			DrawFocusRect(pDC->Handle(), &r);
			#endif
		}

		Icons->Draw(pDC, c.x1+x+Down, c.y1+y+Down, Icon, Enabled() ? 0 : IMGLST_DISABLED);
	}

	void OnFocus(bool f)
	{
		Invalidate();
	}

	void OnMouseClick(GMouse &m)
	{
		if (Enabled())
		{
			bool Trigger = Down AND !m.Down();

			Capture(Down = m.Down());
			if (Down) Focus(true);
			Invalidate();		

			if (Trigger)
			{
				GViewI *n=GetNotify()?GetNotify():GetParent();
				if (n) n->OnNotify(this, 0);
			}
		}
	}

	void OnMouseEnter(GMouse &m)
	{
		if (IsCapturing())
		{
			Down = true;
			Invalidate();
		}
	}

	void OnMouseExit(GMouse &m)
	{
		if (IsCapturing())
		{
			Down = false;
			Invalidate();
		}
	}

	bool OnKey(GKey &k)
	{
		if (k.c16 == ' ' OR k.c16 == VK_RETURN)
		{
			if (Enabled() AND Down ^ k.Down())
			{
				Down = k.Down();
				Invalidate();

				if (!Down)
				{
					GViewI *n=GetNotify()?GetNotify():GetParent();
					if (n) n->OnNotify(this, 0);
				}
			}
			
			return true;
		}
		
		return false;
	}
};

class GFolderList : public GList, public GFolderView
{
public:
	GFolderList(GFileSelectDlg *dlg, int Id, int x, int y, int cx, int cy);

	void OnFolder();
	bool OnKey(GKey &k);
};

#define IDC_STATIC					-1
#define IDD_FILE_SELECT				1000
#define IDC_PATH					1002
#define IDC_DROP					1003
#define IDC_BACK					1004
#define IDC_UP						1005
#define IDC_NEW						1006
#define IDC_VIEW					1007
#define IDC_FILE					1010
#define IDC_TYPE					1011
#define IDC_SHOWHIDDEN				1012

class GFileSelectDlg :
	public GDialog
{
	GRect OldPos;
	GRect MinSize;
	
public:
	GFileSelectPrivate *d;

	GText *Ctrl1;
	GEdit *Ctrl2;
	GFolderDrop *Ctrl3;
	GIconButton *BackBtn;
	GIconButton *UpBtn;
	GIconButton *NewDirBtn;
	GFolderList *FileLst;
	GText *Ctrl8;
	GText *Ctrl9;
	GEdit *FileNameEdit;
	GCombo *FileTypeCbo;
	GButton *SaveBtn;
	GButton *CancelBtn;
	GCheckBox *ShowHidden;

	GFileSelectDlg(GFileSelectPrivate *Select);
	~GFileSelectDlg();

	int OnNotify(GViewI *Ctrl, int Flags);
	void OnUpFolder();
	void SetFolder(char *f);
	void OnFolder();
	void OnFile(char *f);
	void OnPosChange();
};

GFileSelectDlg::GFileSelectDlg(GFileSelectPrivate *select)
{
	SaveBtn = 0;
	BackBtn = 0;
	CancelBtn = 0;
	ShowHidden = 0;
	FileTypeCbo = 0;
	FileNameEdit = 0;
	Ctrl8 = 0;
	Ctrl9 = 0;
	FileLst = 0;
	Ctrl3 = 0;
	BackBtn = 0;
	UpBtn = 0;
	NewDirBtn = 0;
	Ctrl2 = 0;

	d = select;
	SetParent(d->Parent);
	MinSize.ZOff(450, 300);
	OldPos.Set(0, 0, 475, 350 + LgiApp->GetMetric(LGI_MET_DECOR_Y) );
	SetPos(OldPos);

	Children.Insert(Ctrl1 = new GText(IDC_STATIC, 14, 14, -1, -1, "Look in:"));
	Children.Insert(Ctrl2 = new GEdit(IDC_PATH, 91, 7, 245, 21, ""));
	Children.Insert(Ctrl3 = new GFolderDrop(this, IDC_DROP, 336, 7, 16, 21));
	Children.Insert(BackBtn = new GIconButton(IDC_BACK, 378, 7, 27, 21, d->Icons, FSI_BACK));
	Children.Insert(UpBtn = new GIconButton(IDC_UP, 406, 7, 27, 21, d->Icons, FSI_UPDIR));
	Children.Insert(NewDirBtn = new GIconButton(IDC_NEW, 434, 7, 27, 21, d->Icons, FSI_NEWDIR));
	Children.Insert(FileLst = new GFolderList(this, IDC_VIEW, 14, 35, 448, 226));
	Children.Insert(Ctrl8 = new GText(IDC_STATIC, 14, 275, -1, -1, "File name:"));
	Children.Insert(Ctrl9 = new GText(IDC_STATIC, 14, 303, -1, -1, "Files of type:"));
	Children.Insert(FileNameEdit = new GEdit(IDC_FILE, 100, 268, 266, 21, ""));
	Children.Insert(FileTypeCbo = new GCombo(IDC_TYPE, 100, 296, 266, 21, ""));
	Children.Insert(ShowHidden = new GCheckBox(IDC_SHOWHIDDEN, 14, 326, -1, -1, "Show hidden files."));
	Children.Insert(SaveBtn = new GButton(IDOK, 392, 268, 70, 21, "Ok"));
	Children.Insert(CancelBtn = new GButton(IDCANCEL, 392, 296, 70, 21, "Cancel"));

	// Init
	BackBtn->Enabled(false);
	SaveBtn->Enabled(false);
	FileLst->MultiSelect(d->MultiSelect);
	ShowHidden->Value(d->InitShowHiddenFiles);

	// Load types
	if (!d->Types.First())
	{
		GFileType *t = new GFileType;
		if (t)
		{
			t->Description("All Files");
			t->Extension(LGI_ALL_FILES);
			d->Types.Insert(t);
		}
	}
	for (GFileType *t=d->Types.First(); t; t=d->Types.Next())
	{
		char s[256];
		sprintf(s, "%s (%s)", t->Description(), t->Extension());
		FileTypeCbo->Insert(s);
	}
	d->CurrentType = 0;

	// File + Path
	char *File = d->Files.First();
	if (File)
	{
		char *Dir = strrchr(File, DIR_CHAR);
		if (Dir)
		{
			OnFile(Dir + 1);
		}
		else
		{
			OnFile(File);
		}
	}
	
	if (d->InitPath)
	{
		SetFolder(d->InitPath);
	}
	else
	{
		char Str[256];
		LgiGetExePath(Str, sizeof(Str));
		SetFolder(Str);
	}
	OnFolder();

	// Size/layout
	SetPos(d->InitSize);
	MoveToCenter();
}

GFileSelectDlg::~GFileSelectDlg()
{
	d->InitShowHiddenFiles = ShowHidden->Value();
	d->InitSize = GetPos();

	char *CurPath = GetCtrlName(IDC_PATH);
	if (ValidStr(CurPath))
	{
		DeleteArray(d->InitPath);
		d->InitPath = NewStr(CurPath);
	}
}

void GFileSelectDlg::OnPosChange()
{
	GRect NewPos = GetPos();
	if (!NewPos.Valid()) return;	
	
	if (NewPos.X() < MinSize.X())
	{
		NewPos.x2 += MinSize.X() - NewPos.X();
	}
	if (NewPos.Y() < MinSize.Y())
	{
		NewPos.y2 += MinSize.Y() - NewPos.Y();
	}
	
	int Dx = NewPos.X() - OldPos.X();
	int Dy = NewPos.Y() - OldPos.Y();
	GRect r;

	#define MoveRel(Ctrl, x, y) \
		if (Ctrl) \
		{ \
			r = Ctrl->GetPos(); \
			r.Offset(x, y); \
			Ctrl->SetPos(r); \
		}
	#define MoveSizeRel(Ctrl, x, y, cx, cy) \
		if (Ctrl) \
		{ \
			r = Ctrl->GetPos(); \
			r.Offset(x, y); \
			r.x2 += cx; \
			r.y2 += cy; \
			Ctrl->SetPos(r); \
		}
		
	MoveSizeRel(Ctrl2, 0, 0, Dx, 0);
	MoveRel(Ctrl3, Dx, 0);
	MoveRel(BackBtn, Dx, 0);
	MoveRel(UpBtn, Dx, 0);
	MoveRel(NewDirBtn, Dx, 0);
	MoveRel(SaveBtn, Dx, Dy);
	MoveRel(CancelBtn, Dx, Dy);
	MoveRel(ShowHidden, 0, Dy);
	MoveSizeRel(FileNameEdit, 0, Dy, Dx, 0);
	MoveSizeRel(FileTypeCbo, 0, Dy, Dx, 0);
	MoveRel(Ctrl8, 0, Dy);
	MoveRel(Ctrl9, 0, Dy);
	MoveSizeRel(FileLst, 0, 0, Dx, Dy);
	
	OldPos = NewPos;
}

void GFileSelectDlg::OnFile(char *f)
{
	if (d->Type != TypeOpenFolder)
	{
		FileNameEdit->Name(f ? f : (char*)"");
		SaveBtn->Enabled(ValidStr(f));
	}
}

void GFileSelectDlg::SetFolder(char *f)
{
	char *CurPath = GetCtrlName(IDC_PATH);
	if (CurPath)
	{
		d->History.Insert(NewStr(CurPath));
		SetCtrlEnabled(IDC_BACK, true);
	}
	
	SetCtrlName(IDC_PATH, f);
}

void GFileSelectDlg::OnFolder()
{
	Ctrl3->OnFolder();
	FileLst->OnFolder();

	char *CurPath = GetCtrlName(IDC_PATH);
	if (CurPath)
	{
		UpBtn->Enabled(strlen(CurPath)>3);
	}
}

void GFileSelectDlg::OnUpFolder()
{
	char *Cur = GetCtrlName(IDC_PATH);
	if (Cur)
	{
		char Dir[256];
		strcpy(Dir, Cur);
		if (strlen(Dir) > 3)
		{
			LgiTrimDir(Dir);
			if (!strchr(Dir, DIR_CHAR)) strcat(Dir, DIR_STR);
			SetFolder(Dir);
			OnFolder();
		}
	}
}

int GFileSelectDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_PATH:
		{
			if (Flags == VK_RETURN)
			{
				OnFolder();
			}
			break;
		}
		case IDC_VIEW:
		{
			if (FileLst)
			{
				/*	These functions are handled by the list 
					control's OnKey implementation
					
				if (Flags == GLIST_NOTIFY_RETURN)
				{
					List<GListItem> s;
					if (FileLst->GetSelection(s))
					{
						GFolderItem *i = dynamic_cast<GFolderItem*>(s.First());
						if (i)
						{
							i->OnActivate();
						}
					}
				}
				else if (Flags == GLIST_NOTIFY_BACKSPACE)
				{
					OnUpFolder();
				}
				*/
			}
			break;
		}
		case IDC_FILE:
		{
			if (Flags == VK_RETURN)
			{
				char *f = Ctrl->Name();
				if (f)
				{
					// allow user to insert new type by typing the pattern into the file name edit box and
					// hitting enter
					if (strchr(f, '?') OR strchr(f, '*'))
					{
						// it's a mask, push the new type on the the type stack and
						// refilter the content
						int TypeIndex = -1;
						
						int n = 0;
						for (GFileType *t = d->Types.First(); t; t = d->Types.Next(), n++)
						{
							if (t->Extension() AND
								stricmp(t->Extension(), f) == 0)
							{
								TypeIndex = n;
								break;
							}
						}
						
						// insert the new type if not already there
						if (TypeIndex < 0)
						{
							GFileType *n = new GFileType;
							if (n)
							{
								n->Description(f);
								n->Extension(f);							
								TypeIndex = d->Types.Length();
								d->Types.Insert(n);
								FileTypeCbo->Insert(f);
							}
						}
						
						// select the new type
						if (TypeIndex >= 0)
						{
							FileTypeCbo->Value(d->CurrentType = TypeIndex);
						}
						
						// clear the edit box
						Ctrl->Name("");
						
						// Update and don't do normal save btn processing.
						OnFolder();
						
						// Skip the IDOK message generated by the default button
						d->EatClose = true;
						break;
					}
				}
			}
			
			bool HasFile = ValidStr(Ctrl->Name());
			bool BtnEnabled = SaveBtn->Enabled();
			if (HasFile ^ BtnEnabled)
			{
				SaveBtn->Enabled(HasFile);
			}
			break;
		}
		case IDC_BACK:
		{
			char *Dir = d->History.Last();
			if (Dir)
			{
				d->History.Delete(Dir);
				SetCtrlName(IDC_PATH, Dir);
				OnFolder();
				DeleteArray(Dir);

				if (!d->History.First())
				{
					SetCtrlEnabled(IDC_BACK, false);
				}
			}
			break;
		}
		case IDC_SHOWHIDDEN:
		{
			FileLst->OnFolder();
			break;
		}
		case IDC_TYPE:
		{
			d->CurrentType = FileTypeCbo->Value();
			FileLst->OnFolder();

			if (d->Type == TypeSaveFile)
			{
				// change extension of current file
				GFileType *Type = d->Types.ItemAt(d->CurrentType);
				char *File = FileNameEdit->Name();
				if (Type AND File)
				{
					char *Ext = strchr(File, '.');
					if (Ext)
					{
						char *DefExt = Type->DefaultExtension();
						if (DefExt)
						{
							Ext++;
							char s[256];
							ZeroObj(s);
							memcpy(s, File, (int)Ext-(int)File);
							strcat(s, DefExt);

							OnFile(s);

							DeleteArray(DefExt);
						}
					}
				}
			}
			break;
		}
		case IDC_UP:
		{
			OnUpFolder();
			break;
		}
		case IDC_NEW:
		{
			GInput Dlg(this, "", "Create new folder:", "New Folder");
			if (Dlg.DoModal())
			{
				char New[256];
				strcpy(New, GetCtrlName(IDC_PATH));
				if (New[strlen(New)-1] != DIR_CHAR) strcat(New, DIR_STR);
				strcat(New, Dlg.Str);

				FileDev->CreateFolder(New);
				OnFolder();
			}
			break;
		}
		case IDOK:
		{
			if (d->EatClose)
			{
				d->EatClose = false;
				break;
			}
			
			char *Path = GetCtrlName(IDC_PATH);
			char *File = GetCtrlName(IDC_FILE);
			if (Path)
			{
				char f[256];
				d->Files.DeleteArrays();

				if (d->Type == TypeOpenFolder)
				{
					d->Files.Insert(NewStr(Path));
				}
				else
				{
					List<GListItem> Sel;
					if (d->Type != TypeSaveFile AND
						FileLst AND
						FileLst->GetSelection(Sel) AND
						Sel.Length() > 1)
					{
						for (GListItem *i=Sel.First(); i; i=Sel.Next())
						{
							LgiMakePath(f, sizeof(f), Path, i->GetText(0));
							d->Files.Insert(NewStr(f));
						}
					}
					else if (ValidStr(File))
					{
						LgiMakePath(f, sizeof(f), Path, File);
						d->Files.Insert(NewStr(f));
					}
				}
			}

			// fall thru
		}
		case IDCANCEL:
		{
			EndModal(Ctrl->GetId());
			break;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
class GFileSystemItem : public GTreeItem
{
	class GFileSystemPopup *Popup;
	char *Path;

public:
	GFileSystemItem(GFileSystemPopup *popup, GVolume *vol, char *path = 0);

	char *GetPath()
	{
		return Path;
	}

	void OnPath(char *p);
	void OnMouseClick(GMouse &m);
	bool OnKey(GKey &k);
};

#define IDC_TREE 100
class GFileSystemPopup : public GPopup
{
	friend class GFileSystemItem;

	GFileSelectDlg *Dlg;
	GTree *Tree;
	GFileSystemItem *Root;

public:
	GFileSystemPopup(GView *owner, GFileSelectDlg *dlg, int x) : GPopup(owner)
	{
		Dlg = dlg;
		GRect r(0, 0, x, 150);
		SetPos(r);
		Children.Insert(Tree = new GTree(IDC_TREE, 1, 1, X()-3, Y()-3));
		if (Tree)
		{
			Tree->Sunken(false);

			GVolume *v = FileDev->GetRootVolume();
			if (v)
			{
				Tree->SetImageList(Dlg->d->Icons, false);
				Tree->Insert(Root = new GFileSystemItem(this, v));
			}
		}
	}

	~GFileSystemPopup()
	{
	}

	void Visible(bool i)
	{
		if (i AND Root)
		{
			Root->OnPath(Dlg->GetCtrlName(IDC_PATH));
		}

		GPopup::Visible(i);
	}

	void OnPaint(GSurface *pDC)
	{
		// Draw border
		GRect c = GetClient();
		c.Offset(-c.x1, -c.y1);
		pDC->Colour(LC_BLACK, 24);
		pDC->Box(&c);
		c.Size(1, 1);
	}

	void OnActivate(GFileSystemItem *i)
	{
		if (i)
		{
			Dlg->SetFolder(i->GetPath());
			Dlg->OnFolder();
			Visible(false);
		}
	}
};

GFileSystemItem::GFileSystemItem(GFileSystemPopup *popup, GVolume *Vol, char *path)
{
	Popup = popup;
	Expanded(true);

	if (Vol)
	{
		Path = NewStr(Vol->Path());
		SetText(Vol->Name());
		switch (Vol->Type())
		{
			case VT_3_5FLOPPY:
			case VT_5_25FLOPPY:
			case VT_REMOVABLE:
				SetImage(FSI_FLOPPY);
				break;
			case VT_HARDDISK:
			case VT_RAMDISK:
				SetImage(FSI_HARDDISK);
				break;
			case VT_CDROM:
				SetImage(FSI_CDROM);
				break;
			case VT_NETWORK_SHARE:
				SetImage(FSI_NETWORK);
				break;
			case VT_DESKTOP:
				SetImage(FSI_DESKTOP);
				break;
			default:
				SetImage(FSI_DIRECTORY);
				break;
		}

		for (GVolume *v=Vol->First(); v; v=Vol->Next())
		{
			Insert(new GFileSystemItem(Popup, v));
		}
	}
	else
	{
		Path = NewStr(path);
		SetText(strrchr(Path, DIR_CHAR)+1);
		SetImage(FSI_DIRECTORY);
	}
}

void GFileSystemItem::OnPath(char *p)
{
	switch (GetImage())
	{
		case FSI_DESKTOP:
		{
			if (p AND
				Path AND
				stricmp(Path, p) == 0)
			{
				Select(true);
				p = 0;
			}
			break;
		}
		case FSI_DIRECTORY:
		{
			return;
		}
		default:
		{
			GTreeItem *Old = Items.First();
			if (Old)
			{
				Old->Remove();
				DeleteObj(Old);
			}
			break;
		}
	}

	if (p)
	{
		int PathLen = strlen(Path);
		if (Path AND
			strnicmp(Path, p, PathLen) == 0 AND
			(p[PathLen] == DIR_CHAR OR p[PathLen] == 0)
			#ifdef LINUX
			AND strcmp(Path, "/") != 0
			#endif
			)
		{
			GTreeItem *Item = this;

			if (GetImage() != FSI_DESKTOP AND
				strlen(p) > 3)
			{
				char *Start = p + strlen(Path);
				if (Start)
				{
					char s[256];
					strcpy(s, Path);

					GToken T(Start, DIR_STR);
					for (int i=0; i<T.Length(); i++)
					{
						if (s[strlen(s)-1] != DIR_CHAR) strcat(s, DIR_STR);
						strcat(s, T[i]);

						GFileSystemItem *New = new GFileSystemItem(Popup, 0, s);
						if (New)
						{
							Item->Insert(New);
							Item = New;
						}
					}
				}
			}

			if (Item)
			{
				Item->Select(true);
			}
		}
	}

	for (GFileSystemItem *i=dynamic_cast<GFileSystemItem*>(Items.First()); i;
						  i=dynamic_cast<GFileSystemItem*>(Items.Next()))
	{
		i->OnPath(p);
	}
}

void GFileSystemItem::OnMouseClick(GMouse &m)
{
	if (m.Left() AND m.Down())
	{
		Popup->OnActivate(this);
	}
}

bool GFileSystemItem::OnKey(GKey &k)
{
	if ((k.c16 == ' ' OR k.c16 == VK_RETURN))
	{
		if (k.Down() AND k.IsChar)
		{
			Popup->OnActivate(this);
		}
		
		return true;
	}
	
	return false;
}

GFolderDrop::GFolderDrop(GFileSelectDlg *dlg, int Id, int x, int y, int cx, int cy) :
	GDropDown(Id, x, y, cx, cy, 0),
	GFolderView(dlg)
{
	SetPopup(new GFileSystemPopup(this, dlg, cx+dlg->Ctrl2->X()));
}

void GFolderDrop::OnFolder()
{
}

//////////////////////////////////////////////////////////////////////////
#define IDM_OPEN				1000
#define IDM_CUT					1001
#define IDM_COPY				1002
#define IDM_RENAME				1003
#define IDM_PROPERTIES			1004
#define IDM_CREATE_SHORTCUT		1005
#define IDM_DELETE				1006

GFolderItem::GFolderItem(GFileSelectDlg *dlg, char *FullPath, GDirectory *Dir)
{
	Dlg = dlg;
	Path = NewStr(FullPath);
	File = strrchr(Path, DIR_CHAR);
	if (File) File++;
	IsDir = Dir->IsDir();
}

GFolderItem::~GFolderItem()
{
	DeleteArray(Path);
}

char *GFolderItem::GetText(int i)
{
	return File;
}

int GFolderItem::GetImage(int Flags)
{
	return IsDir ? 1 : 0;
}

void GFolderItem::OnSelect()
{
	if (!IsDir AND File)
	{
		Dlg->OnFile(Select() ? File : 0);
	}
}

void GFolderItem::OnDelete(bool Ask)
{
	if (!Ask OR LgiMsg(Parent, "Do you want to delete '%s'?", ModuleName, MB_YESNO, Path) == IDYES)
	{
		bool Status = false;

		if (IsDir)
		{
			Status = FileDev->RemoveFolder(Path, true);
		}
		else
		{
			Status = FileDev->Delete(Path);
		}

		if (Status)
		{
			Parent->Remove(this);
			delete this;
		}
	}
}

void GFolderItem::OnRename()
{
	GInput Inp(Dlg, File, "New name:", Dlg->Name());
	if (Inp.DoModal())
	{
		char Old[256];
		strcpy(Old, Path);

		char New[256];
		File[0] = 0;
		LgiMakePath(New, sizeof(New), Path, Inp.Str);
		
		if (FileDev->Move(Old, New))
		{
			DeleteArray(Path);
			Path = NewStr(New);
			File = strrchr(Path, DIR_CHAR);
			if (File) File++;
			Update();
		}
		else
		{
			LgiMsg(Dlg, "Renaming '%s' failed.", Dlg->Name(), MB_OK);
		}
	}
}

void GFolderItem::OnActivate()
{
	if (File)
	{
		if (IsDir)
		{
			char Dir[256];
			strcpy(Dir, Dlg->GetCtrlName(IDC_PATH));
			if (Dir[strlen(Dir)-1] != DIR_CHAR) strcat(Dir, DIR_STR);
			strcat(Dir, File);
			Dlg->SetFolder(Dir);
			Dlg->OnFolder();
		}
		else // Is file
		{
			Dlg->OnNotify(Dlg->SaveBtn, 0);
		}
	}
}

void GFolderItem::OnMouseClick(GMouse &m)
{
	if (m.Down())
	{
		if (m.Left())
		{
			if (m.Double())
			{
				OnActivate();
			}
		}
		else if (m.Right())
		{
			GSubMenu *RClick = new GSubMenu;
			if (RClick)
			{
				RClick->AppendItem("Select", IDM_OPEN, true);
				RClick->AppendSeparator();
				RClick->AppendItem("Cut", IDM_CUT, false);
				RClick->AppendItem("Copy", IDM_COPY, false);
				RClick->AppendSeparator();
				RClick->AppendItem("Create Shortcut", IDM_CREATE_SHORTCUT, false);
				RClick->AppendItem("Delete", IDM_DELETE, true);
				RClick->AppendItem("Rename", IDM_RENAME, true);
				RClick->AppendSeparator();
				RClick->AppendItem("Properties", IDM_PROPERTIES, false);

				if (Parent->GetMouse(m, true))
				{
					switch (RClick->Float(Parent, m.x, m.y))
					{
						case IDM_OPEN:
						{
							break;
						}
						case IDM_DELETE:
						{
							OnDelete();
							break;
						}
						case IDM_RENAME:
						{
							OnRename();
							break;
						}
					}
				}

				DeleteObj(RClick);
			}
		}
	}
}

int GFolderItemCompare(GFolderItem *a, GFolderItem *b, int Data)
{
	if (a AND b)
	{
		if (a->IsDir ^ b->IsDir)
		{
			if (a->IsDir) return -1;
			else return 1;
		}
		else if (a->File AND b->File)
		{
			return stricmp(a->File, b->File);
		}
	}

	return 0;
}

GFolderList::GFolderList(GFileSelectDlg *dlg, int Id, int x, int y, int cx, int cy) :
	GList(Id, x, y, cx, cy),
	GFolderView(dlg)
{
	SetImageList(Dlg->d->Icons, false);
	ShowColumnHeader(false);
	AddColumn("Name", cx-20);
	SetMode(GListColumns);
}

bool GFolderList::OnKey(GKey &k)
{
	bool Status = GList::OnKey(k);
	
	switch (k.vkey)
	{
		case VK_BACKSPACE:
		{
			if (k.Down() && !k.IsChar && GetWindow())
			{
				// Go up a directory
				GViewI *v = GetWindow()->FindControl(IDC_UP);
				if (v)
				{
					GetWindow()->OnNotify(v, 0);
				}
			}
			Status = true;
			break;
		}
		case VK_RETURN:
		#ifdef VK_KP_ENTER
		case VK_KP_ENTER:
		#endif
		{
			if (k.Down() && !k.IsChar && GetWindow())
			{
				GFolderItem *Sel = dynamic_cast<GFolderItem*>(GetSelected());
				if (Sel)
				{
					if (Sel->IsDir)
					{
						char *Cur = GetWindow()->GetCtrlName(IDC_PATH);
						if (Cur)
						{
							char Path[256];
							LgiMakePath(Path, sizeof(Path), Cur, Sel->GetText(0));
							if (DirExists(Path))
							{
								GetWindow()->SetCtrlName(IDC_PATH, Path);
								Dlg->OnFolder();
							}
						}
					}
					else
					{
						GViewI *Ok = GetWindow()->FindControl(IDOK);
						if (Ok)
						{
							GetWindow()->SetCtrlName(IDC_FILE, Sel->GetText(0));
							GetWindow()->OnNotify(Ok, 0);
						}
					}
				}
			}
			Status = true;
			break;
		}
		case VK_DELETE:
		{
			if (k.Down() && !k.IsChar && GetWindow())
			{
				List<GListItem> Sel;
				if (GetSelection(Sel))
				{
					GStringPipe Msg;
					Msg.Push("Do you want to delete:\n\n");
					
					List<GFolderItem> Delete;
					for (GListItem *i=Sel.First(); i; i=Sel.Next())
					{
						GFolderItem *s = dynamic_cast<GFolderItem*>(i);
						if (s)
						{
							Delete.Insert(s);
							Msg.Push("\t");
							Msg.Push(s->GetText(0));
							Msg.Push("\n");
						}
					}
					
					char *Mem = Msg.NewStr();
					if (Mem)
					{
						if (LgiMsg(this, Mem, ModuleName, MB_YESNO) == IDYES)
						{
							for (GFolderItem *d=Delete.First(); d; d=Delete.Next())
							{
								d->OnDelete(false);
							}
						}
						DeleteArray(Mem);
					}
				}
			}
			Status = true;
			break;
		}
	}

	// LgiTrace("%s:%i GFolderList::OnKey, key=%i down=%i status=%i\n", _FL, k.vkey, k.Down(), Status);
	
	return Status;
}

void GFolderList::OnFolder()
{
	Empty();

	GDirectory *Dir = FileDev->GetDir();
	if (Dir)
	{
		List<GFolderItem> New;

		// Get current type
		GFileType *Type = Dlg->d->Types.ItemAt(Dlg->d->CurrentType);
		List<char> Ext;
		if (Type)
		{
			GToken T(Type->Extension(), ";");
			for (int i=0; i<T.Length(); i++)
			{
				Ext.Insert(NewStr(T[i]));
			}
		}

		// Get items
		bool ShowHiddenFiles = Dlg->ShowHidden->Value();
		for (bool Found = Dir->First(Dlg->Ctrl2->Name()); Found; Found = Dir->Next())
		{
			char Name[256];
			Dir->Path(Name, sizeof(Name));

			bool Match = true;
			if (!ShowHiddenFiles AND Dir->IsHidden())
			{
				Match = false;
			}
			else if (!Dir->IsDir() AND
					Ext.Length() > 0)
			{
				Match = false;
				for (char *e=Ext.First(); e AND !Match; e=Ext.Next())
				{
					if (e[0] == '*' AND e[1] == '.')
					{
						char *Ext = LgiGetExtension(Name);
						if (Ext)
						{
							Match = stricmp(Ext, e + 2) == 0;
						}
					}
					else
					{
						bool m = MatchStr(e, Name);
						if (m)
						{
							Match = true;
						}
					}
				}
			}

			if (Match)
			{
				New.Insert(new GFolderItem(Dlg, Name, Dir));
			}
		}

		// Sort items...
		New.Sort(GFolderItemCompare, 0);

		// Display items...
		Insert((List<GListItem>&) New);

		DeleteObj(Dir);
	}
}

//////////////////////////////////////////////////////////////////////////
GFileSelect::GFileSelect()
{
	d = new GFileSelectPrivate(this);
}

GFileSelect::~GFileSelect()
{
	DeleteObj(d);
}

void GFileSelect::ShowReadOnly(bool b)
{
	d->ShowReadOnly = b;;
}

bool GFileSelect::ReadOnly()
{
	return d->ReadOnly;
}

char *GFileSelect::Name()
{
	return d->Files.First();
}

bool GFileSelect::Name(const char *n)
{
	d->Files.DeleteArrays();
	if (n)
	{
		d->Files.Insert(NewStr(n));
	}

	return true;
}

char *GFileSelect::operator [](int i)
{
	return d->Files.ItemAt(i);
}

int GFileSelect::Length()
{
	return d->Files.Length();
}

int GFileSelect::Types()
{
	return d->Types.Length();
}

void GFileSelect::ClearTypes()
{
	d->Types.DeleteObjects();
}

GFileType *GFileSelect::TypeAt(int n)
{
	return d->Types.ItemAt(n);
}

bool GFileSelect::Type(const char *Description, const char *Extension, int Data)
{
	GFileType *Type = new GFileType;
	if (Type)
	{
		Type->Description(Description);
		Type->Extension(Extension);
		d->Types.Insert(Type);
	}

	return Type != 0;
}

int GFileSelect::SelectedType()
{
	return d->CurrentType;
}

GViewI *GFileSelect::Parent()
{
	return d->Parent;
}

void GFileSelect::Parent(GViewI *Window)
{
	d->Parent = dynamic_cast<GView*>(Window);
}

bool GFileSelect::MultiSelect()
{
	return d->MultiSelect;
}

void GFileSelect::MultiSelect(bool Multi)
{
	d->MultiSelect = Multi;
}

#define CharPropImpl(Func, Var)				\
	char *GFileSelect::Func()				\
	{										\
		return Var;							\
	}										\
	void GFileSelect::Func(const char *i)	\
	{										\
		DeleteArray(Var);					\
		if (i)								\
		{									\
			Var = NewStr(i);				\
		}									\
	}

CharPropImpl(InitialDir, d->InitPath);
CharPropImpl(Title, d->Title);
CharPropImpl(DefaultExtension, d->DefExt);

bool GFileSelect::Open()
{
	GFileSelectDlg Dlg(d);

	d->Type = TypeOpenFile;
	Dlg.Name("Open");
	Dlg.SaveBtn->Name("Open");

	return Dlg.DoModal() == IDOK;
}

bool GFileSelect::OpenFolder()
{
	GFileSelectDlg Dlg(d);

	d->Type = TypeOpenFolder;
	Dlg.SaveBtn->Enabled(true);
	Dlg.FileNameEdit->Enabled(false);
	Dlg.Name("Open Folder");
	Dlg.SaveBtn->Name("Open");

	return Dlg.DoModal() == IDOK;
}

bool GFileSelect::Save()
{
	GFileSelectDlg Dlg(d);
	
	d->Type = TypeSaveFile;
	Dlg.Name("Save As");
	Dlg.SaveBtn->Name("Save As");

	return Dlg.DoModal() == IDOK;
}

