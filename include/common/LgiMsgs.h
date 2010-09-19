#ifndef _LGI_MSGS_H_
#define _LGI_MSGS_H_

// Base
#define L_SUCCESS							0
#define L_ERROR								-1
#define L_ERROR_NO_MEMORY					-2
#define L_ERROR_BAD_VALUE					-3

#define L_BTN_OK							-50
#define L_BTN_CANCEL						-51

// Mail lib error message ID's
#define L_ERROR_ESMTP_NO_AUTHS				-100
#define L_ERROR_ESMTP_UNSUPPORTED_AUTHS		-101

// Text control strings
#define L_TEXTCTRL_AUTO_INDENT				-200
#define L_TEXTCTRL_COPY						-201
#define L_TEXTCTRL_COPYLINK					-202
#define L_TEXTCTRL_CUT						-203
#define L_TEXTCTRL_EMAIL_TO					-204
#define L_TEXTCTRL_FIXED					-205
#define L_TEXTCTRL_GOTO_LINE				-206
#define L_TEXTCTRL_OPENURL					-207
#define L_TEXTCTRL_PASTE					-208
#define L_TEXTCTRL_REDO						-209
#define L_TEXTCTRL_UNDO						-210
#define L_TEXTCTRL_SHOW_WHITESPACE			-211
#define L_TEXTCTRL_HARD_TABS				-212
#define L_TEXTCTRL_INDENT_SIZE				-213
#define L_TEXTCTRL_TAB_SIZE					-214
#define L_TEXTCTRL_RTL						-215

#define L_TEXTCTRL_ASK_SAVE					-220
#define L_TEXTCTRL_SAVE						-221

// Resource sub system
#define L_ERROR_RES_NO_EXE_PATH				-300
#define L_ERROR_RES_NO_LR8_FILE				-301
#define L_ERROR_RES_CREATE_OBJECT_FAILED	-302
#define L_ERROR_RES_RESOURCE_READ_FAILED	-303

// Font selection dialog
#define L_FONTUI_BOLD						-400
#define L_FONTUI_FACE						-401
#define L_FONTUI_ITALIC						-402
#define L_FONTUI_PREVIEW					-403
#define L_FONTUI_PTSIZE						-404
#define L_FONTUI_STYLE						-405
#define L_FONTUI_TITLE						-406
#define L_FONTUI_UNDERLINE					-407

// Html control
#define L_COPY_LINK_LOCATION				-500
#define L_VIEW_SOURCE						-501
#define L_COPY_SOURCE						-502
#define L_VIEW_IN_DEFAULT_BROWSER			-503
#define L_VIEW_IMAGES						-504
#define L_CHANGE_CHARSET					-505

// Colour control
#define L_COLOUR_NONE						-550

// Find/Replay windows
#define L_FR_FIND							-600
#define L_FR_FIND_WHAT						-601
#define L_FR_FIND_NEXT						-602
#define L_FR_MATCH_WORD						-603
#define L_FR_MATCH_CASE						-604
#define L_FR_REPLACE						-605
#define L_FR_REPLACE_ALL					-606
#define L_FR_REPLACE_WITH					-607
#define L_FR_SELECTION_ONLY					-608

// Misc
#define L_TOOLBAR_SHOW_TEXT					-700

// Storage lib
#define L_STORE_WRITE_ERR					-800
#define L_STORE_OS_ERR						-801
#define L_STORE_MISMATCH					-802
#define L_STORE_RESTART						-803

// Filter UI
#define L_FUI_NEW_CONDITION					-900
#define L_FUI_NEW_AND						-901
#define L_FUI_NEW_OR						-902
#define L_FUI_AND							-903
#define L_FUI_OR							-904
#define L_FUI_DELETE						-905
#define L_FUI_MOVE_UP						-906
#define L_FUI_MOVE_DOWN						-907
#define L_FUI_CONFIGURE						-908
#define L_FUI_OPTIONS						-909
#define L_FUI_NOT							-910
#define L_FUI_LEGEND						-911
#define L_FUI_NEW							-912
#define L_FUI_CONDITION						-913

// Software update class
#define L_SOFTUP_CHECKING					-1000
#define L_SOFTUP_DOWNLOADING				-1001

#define L_ERROR_UNEXPECTED_XML				-1020
#define L_ERROR_XML_PARSE					-1021
#define L_ERROR_HTTP_FAILED					-1022
#define L_ERROR_CONNECT_FAILED				-1023
#define L_ERROR_NO_URI						-1024
#define L_ERROR_URI_ERROR					-1025
#define L_ERROR_UPDATE						-1026
#define L_ERROR_COPY_FAILED					-1027
#define L_ERROR_OPENING_TEMP_FILE			-1028

// GDocApp
#define L_DOCAPP_SAVE_CHANGE				-1100
#define L_DOCAPP_RESTART_APP				-1101

#endif
