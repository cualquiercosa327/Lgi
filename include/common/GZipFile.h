#ifndef _GZIPFILE_H_
#define _GZIPFILE_H_

class GZipFile
{
	class GZipFilePrivate *d;

public:
	GZipFile(char *zip = 0);
	~GZipFile();
	
	bool IsOpen();
	bool Open(char *Zip, int Mode);
	void Close();
	
	GDirectory *List();
	bool Decompress(char *File, GStream *To);
	bool Compress(char *File, GStream *From);
	bool Delete(char *File);
};

#endif