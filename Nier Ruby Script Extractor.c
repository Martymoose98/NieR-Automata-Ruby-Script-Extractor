// Nier Ruby Script Extractor.c : Defines the entry point for the console application.
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned char bool;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

#define TRUE 1
#define FALSE 0

#define ROTATE16_LEFT(x, r) (((x) << (r)) | ((x) >> (16 - (r))))
#define EIGHTCC(a, b, c, d, e, f, g, h) ( (uint64)(((uint64)((byte)a)) | (((uint64)((byte)b)) << 8) | (((uint64)((byte)c)) << 16) | (((uint64)((byte)d)) << 24) | (((uint64)((byte)e)) << 32) | (((uint64)((byte)f)) << 40) | (((uint64)((byte)g)) << 48) | (((uint64)((byte)h)) << 56)) )

#define MRUBY_COMPILED_MAGIC EIGHTCC('R', 'I', 'T', 'E', '0', '0', '0', '3')
#define MRUBY_COMPILED_EOF_MAGIC EIGHTCC('E', 'N', 'D', 0, 0, 0, 0, 8)

typedef struct _MRUBY_COMPILED_HEADER
{
	union
	{
		char signature[8];
		uint64 magic;
	};
	char unknown8[4];
	uint16 size;		//	stored in big endian

} MRUBY_COMPILED_HEADER, *PMRUBY_COMPILED_HEADER;

typedef struct _SCRIPT_LIST_ENTRY
{
	MRUBY_COMPILED_HEADER hdr;
	fpos_t fileoffset;
	void* pScript;
	struct _SCRIPT_LIST_ENTRY* pNext;
} SCRIPT_LIST_ENTRY, *PSCRIPT_LIST_ENTRY;

PSCRIPT_LIST_ENTRY add_entry(PSCRIPT_LIST_ENTRY pHead, void* pScript)
{
	PSCRIPT_LIST_ENTRY pLast = pHead;

	while (pLast->pNext)
		pLast = pLast->pNext;

	 PSCRIPT_LIST_ENTRY pEntry = malloc(sizeof(SCRIPT_LIST_ENTRY));

	 if (!pEntry)
		 return NULL;

	 pLast->pNext = pEntry;
	 pEntry->pScript = pScript;
	 pEntry->pNext = NULL;

	 return pEntry;
}

size_t get_list_size(PSCRIPT_LIST_ENTRY pHead)
{
	size_t index = 0;

	if (!pHead)
		return 0;

	for (PSCRIPT_LIST_ENTRY it = pHead; it; it = it->pNext, ++index);

	return index;
}

void free_list(PSCRIPT_LIST_ENTRY pHead)
{
	PSCRIPT_LIST_ENTRY pEntry = pHead;
	PSCRIPT_LIST_ENTRY pNext;
	
	if (!pEntry)
		return;

	while (pEntry->pNext)
	{
		pNext = pEntry->pNext;
		free(pEntry->pScript);
		free(pEntry);
		pEntry = pNext;
	}
}

errno_t open_file(const char* szFilename, FILE** ppFile, size_t* pSize)
{
	if (!szFilename || !ppFile || !pSize)
		return -1;

	errno_t err = fopen_s(ppFile, szFilename, "rb");
	
	if (err != 0)
		return 1;
	  
	if (fseek(*ppFile, 0, SEEK_END))
		return 2;

	*pSize = ftell(*ppFile);

	if (*pSize == 0xFFFFFFFF)
		return errno;

	rewind(*ppFile);

	return 0;
}
	

inline size_t fprobe(void* pBuffer, size_t size, size_t count, int nBytesAdvance, FILE* pStream)
{
	fpos_t pos;
	fpos_t new_pos;
	size_t nElementsRead;

	fgetpos(pStream, &pos);
	new_pos = pos + nBytesAdvance;
	nElementsRead = fread(pBuffer, size, count, pStream);
	fsetpos(pStream, &new_pos);
	
	return nElementsRead;
}

inline void fsetposrel(int nBytesAdvance, FILE* pStream)
{
	fpos_t pos;
	fpos_t new_pos;

	fgetpos(pStream, &pos);
	new_pos = pos + nBytesAdvance;
	fsetpos(pStream, &new_pos);
}

PSCRIPT_LIST_ENTRY parse_ruby_binary(FILE* pFile, PSCRIPT_LIST_ENTRY pLastEntry)
{
	PSCRIPT_LIST_ENTRY pEntry = NULL;
	
	if (!pFile)
		return NULL;

	pEntry = malloc(sizeof(SCRIPT_LIST_ENTRY));
	
	if (!pEntry)
		return NULL;
	
	pEntry->pNext = NULL;
	
	if (pLastEntry)
		pLastEntry->pNext = pEntry;

	fprobe(&pEntry->hdr, 1, sizeof(MRUBY_COMPILED_HEADER), 0, pFile);
	pEntry->hdr.size = ROTATE16_LEFT(pEntry->hdr.size, 8);
	pEntry->fileoffset = ftell(pFile);
	pEntry->pScript = malloc(pEntry->hdr.size);
	size_t nBytesRead = fread(pEntry->pScript, 1, pEntry->hdr.size, pFile);

	if (nBytesRead < pEntry->hdr.size)
	{
		printf("Partial Read! Read %llx bytes out of %x", nBytesRead, pEntry->hdr.size);
	}
	return pEntry;
}

PSCRIPT_LIST_ENTRY extract_ruby_scripts(FILE* pFile, size_t file_size)
{
	uint64 magic;
	PSCRIPT_LIST_ENTRY pHead = NULL;
	PSCRIPT_LIST_ENTRY pLast = NULL;
	bool bHeadMade = FALSE;

	if (!pFile)
		return NULL;

	while (file_size >= (size_t)ftell(pFile))
	{
		fprobe(&magic, 1, sizeof(uint64), 1, pFile);

		if (feof(pFile) || ferror(pFile))
			break;

		if (magic == MRUBY_COMPILED_MAGIC)
		{
			fsetposrel(-1, pFile);

			if (bHeadMade)
			{
				pLast = parse_ruby_binary(pFile, pLast);
			}
			else
			{
				pHead = parse_ruby_binary(pFile, NULL);
				pLast = pHead;
				bHeadMade = TRUE;
			}
		}
	}

	return pHead;
}

void save_scripts(const char* szSavePath, const char* szPrefix, PSCRIPT_LIST_ENTRY pHead)
{
	char szFilename[_MAX_PATH];
	FILE* pFile;

	for (PSCRIPT_LIST_ENTRY it = pHead; it; it = it->pNext)
	{
		if (szPrefix)
			sprintf_s(szFilename, _MAX_PATH, "%s\\%s_script_%llx.mrb", (szSavePath) ? szSavePath : "", szPrefix, it->fileoffset);
		else
			sprintf_s(szFilename, _MAX_PATH, "%s\\script_%llx.mrb", (szSavePath) ? szSavePath : "", it->fileoffset);

		errno_t err = fopen_s(&pFile, szFilename, "wb");
		fwrite(it->pScript, 1, it->hdr.size, pFile);
		fclose(pFile);
	}
}

void print_welcome() 
{
#if WIN32
	printf("Nier Automata Script Extractor v0.04 by Martino\n\n");
#else
	printf("Nier Automata Script Extractor (x64) v0.04 by Martino\n\n");
#endif
}

void print_help()
{
	printf("Usage: -d [filename] [dumppath] | -help\n"
		"-d  Dumps all compiled ruby srcipts in the file designated [filename] to the [dumppath].\n"
		"	 [dumppath] is optional and if ommited the scripts will be dumped to the extractor path\n\n"
		"-p  Appends the filename as a prefix to the dumpfile. To be used with -d\n");
}

int main(int argc, char** argv)
{
	FILE* pFile;
	size_t file_size;
	PSCRIPT_LIST_ENTRY pHead;
	char* szFilename = NULL;
	char* szFilenameCopy = NULL;
	char* szDumpPath = NULL;
	char* szRelativeFilename = NULL;
	bool bPrefix = FALSE;

	print_welcome();

	if (argc > 1)
	{
		if (!_stricmp(argv[1], "-help"))
		{
			print_help();
			return 1;
		}
		else if (!_stricmp(argv[1], "-d"))
		{
			if (!_stricmp(argv[2], "-p"))
			{
				szFilename = argv[3];

				if (argc >= 5)
					szDumpPath = argv[4];

				bPrefix = TRUE;
			}
			else 
			{
				szFilename = argv[2];

				if (argc >= 4)
					szDumpPath = argv[3];
			}
		}
	}
	else
	{
		print_help();
		return 2;
	}

	if (szFilename && bPrefix)
	{
		size_t size = strlen(szFilename) + 1;
		szFilenameCopy = malloc(size);
		strcpy_s(szFilenameCopy, size, szFilename);

		szRelativeFilename = strrchr(szFilenameCopy, '\\');
		char* szDumpFilePrefix = strstr(szRelativeFilename++, ".");
		*szDumpFilePrefix = 0;
	}

	if (open_file(szFilename, &pFile, &file_size))
	{
		printf("Could not open the file: %s\nPlease check the file path and try again\n", szFilename);
		return 3;
	}

	printf("file size %llx\n", file_size);

	clock_t start = clock();
	pHead = extract_ruby_scripts(pFile, file_size);
	fclose(pFile);
	
	if (pHead)
	{
		printf("Extracted %lld scripts in %.3f seconds\n", get_list_size(pHead), ((float)(clock() - start) / (float)CLOCKS_PER_SEC));
		save_scripts(szDumpPath, szRelativeFilename, pHead);
		free_list(pHead);
	}

	if (szFilenameCopy)
		free(szFilenameCopy);

	return 0;
}