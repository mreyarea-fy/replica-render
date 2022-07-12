
#include "FileMemMap.h"

#include <filesystem>

#ifdef _WIN32
	#include <strsafe.h>
	#include <atlstr.h>
	#include <algorithm>
#elif __linux__
	#include <fcntl.h>
	#include <sys/mman.h>
	#include <unistd.h>
#endif

#ifdef _WIN32

char *FileMemMap::mapfile(const std::string &filename)
{
	// 0)open file  // TODO support wchar
	//TCHAR szName[512];
	//_tcscpy(szName, A2T(filename.c_str()));
	//HANDLE hFile;
	hFile = CreateFile(filename.c_str(),							 // file to open
					   GENERIC_READ,								 // open for reading
					   FILE_SHARE_READ,								 // share for reading
					   NULL,										 // default security
					   OPEN_EXISTING,								 // existing file only
					   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
					   NULL);										 // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		_tprintf(TEXT("Terminal failure: Unable to open file \"%s\" for read.\n"), filename);
		return nullptr;
	}
	// 1) memory map file
	//HANDLE hFileMap;
	hFileMap = CreateFileMapping(hFile,
								 NULL,
								 PAGE_READONLY,
								 0,
								 0,
								 NULL);

	if (hFileMap == NULL)
	{
		_tprintf(TEXT("hMapFile is NULL: last error: %d\n"), GetLastError());
		return nullptr;
	}
	// 2) create view memory-mapped region
	//LPVOID
	lpMapAddress = MapViewOfFile(hFileMap,		// handle to  mapping object
								 FILE_MAP_READ, // read/write
								 0,				// high-order 32 bits of file  offset
								 0,				// low-order 32 bits of file  offset
								 0);			// number of bytes to map
	if (lpMapAddress == NULL)
	{
		_tprintf(TEXT("lpMapAddress is NULL: last error: %d\n"), GetLastError());
		return nullptr;
	}

	return reinterpret_cast<char *>(lpMapAddress);
}

void FileMemMap::release()
{
	BOOL bFlag; // a result holder
	bFlag = UnmapViewOfFile(this->lpMapAddress);
	bFlag = CloseHandle(this->hFileMap); // close the file mapping object

	if (!bFlag)
	{
		_tprintf(TEXT("\nError %ld occurred closing the mapping object!"), GetLastError());
	}
	bFlag = CloseHandle(this->hFile); // close the file itself
	if (!bFlag)
	{
		_tprintf(TEXT("\nError %ld occurred closing the file!"), GetLastError());
	}
}

#elif __linux__

char *FileMemMap::mapfile(const std::string &filename)
{
	this->fileSize = std::filesystem::file_size(filename);
	int fd = open(filename.c_str(), O_RDONLY, 0);
	this->mmappedData = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	// Parse each vertex packet and unpack
	close(fd);
	return reinterpret_cast<char *>(mmappedData);
}

void FileMemMap::release()
{
	munmap(mmappedData, fileSize);
}

#endif