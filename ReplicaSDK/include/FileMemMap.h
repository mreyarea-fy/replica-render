#pragma once

#include <string>

#ifdef _WIN32
	#define NOMINMAX
	#include <windows.h>
#endif

class FileMemMap
{
public:
	FileMemMap() {}

	// map file to memory
	char *mapfile(const std::string &filename);

	// release file and mapped resource
	void release();

private:
	// file path
	std::string filename;

#ifdef _WIN32
	// file handle
	HANDLE hFile;

	// mapped file handle
	HANDLE hFileMap;

	// file view address
	LPVOID lpMapAddress;
#elif __linux__
	// file handle linux
	void *mmappedData = nullptr;

	size_t fileSize;

#endif
};