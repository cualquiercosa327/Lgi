#ifndef _RTF_HTML_
#define _RTF_HTML_

// This decodes the aweful mess that is
// M$ RTF back into the HTML it came from
// and then back to RTF.
char *MsRtfToHtml(char *s);
char *MsHtmlToRtf(char *s);

#endif