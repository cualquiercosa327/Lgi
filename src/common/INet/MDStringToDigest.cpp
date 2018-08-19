#include "Lgi.h"
#include "INet.h"
#include "../Hash/md5/md5.h"

/////////////////////////////////
void MDStringToDigest(unsigned char digest[16], char *Str, int Len)
{
	md5_state_t ms;
	md5_init(&ms);
	md5_append(&ms, (md5_byte_t*)Str, Len < 0 ? strlen(Str) : Len);
	md5_finish(&ms, (char*)digest);
}
