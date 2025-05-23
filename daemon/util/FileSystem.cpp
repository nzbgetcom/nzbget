/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "FileSystem.h"
#include "Util.h"
#include "Log.h"

#ifdef WIN32
#include "utf8.h"
#endif

const char* RESERVED_DEVICE_NAMES[] = { "CON", "PRN", "AUX", "NUL",
	"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
	"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", NULL };

CString FileSystem::GetLastErrorMessage()
{
	BString<1024> msg;
	strerror_r(errno, msg, msg.Capacity());

#ifdef WIN32
	if (!errno)
	{
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			msg, 1024, nullptr);
	}
#endif

	return *msg;
}

void FileSystem::NormalizePathSeparators(char* path)
{
	for (char* p = path; *p; p++)
	{
		if (*p == ALT_PATH_SEPARATOR)
		{
			*p = PATH_SEPARATOR;
		}
	}
}

std::optional<std::string> FileSystem::GetRealPath(const std::string& path)
{
	if (path.empty()) return std::nullopt;

#ifdef WIN32
	auto res = Utf8::Utf8ToWide(path);
	if (!res.has_value()) return std::nullopt;

	std::wstring wpath = std::move(res.value());

	if (wpath.size() > MAX_DIR_PATH && std::wcsncmp(wpath.c_str(), L"\\\\", 2) == 0)
	{
		wpath = L"\\\\?\\UNC" + wpath;
	}
	else if (wpath.size() > MAX_DIR_PATH)
	{
		wpath = L"\\\\?\\" + wpath;
	}

	DWORD len = GetFullPathNameW(wpath.c_str(), 0, nullptr, nullptr);
	if (len == 0) return std::nullopt;

	std::wstring buffer(len, '\0');
	len = GetFullPathNameW(wpath.c_str(), len, &buffer[0], nullptr);
	if (len == 0) return std::nullopt;

	return Utf8::WideToUtf8(buffer);
#else
	if (char* realPath = realpath(path.c_str(), nullptr))
	{
		std::string res = realPath;
		free(realPath);
		return res;
	}
#endif

	return std::nullopt;
}

#ifdef WIN32

const size_t FileSystem::MAX_DIR_PATH = 248;

bool FileSystem::ForceDirectories(const char* path, CString& errmsg)
{
	errmsg.Clear();
	BString<1024> normPath = path;
	NormalizePathSeparators(normPath);
	int len = strlen(normPath);
	if (len > 3 && normPath[len - 1] == PATH_SEPARATOR)
	{
		normPath[len - 1] = '\0';
	}

	if (DirectoryExists(normPath))
	{
		return true;
	}
		
	if (FileExists(normPath))
	{
		errmsg.Format("path %s is not a directory", *normPath);
		return false;
	}

	if (strlen(normPath) > 2)
	{
		BString<1024> parentPath = *normPath;
		char* p = (char*)strrchr(parentPath, PATH_SEPARATOR);
		if (p)
		{
			if (p - parentPath == 2 && parentPath[1] == ':' && strlen(parentPath) > 2)
			{
				parentPath[3] = '\0';
			}
			else
			{
				*p = '\0';
			}
			if (strlen(parentPath) != strlen(path) && !ForceDirectories(parentPath, errmsg))
			{
				return false;
			}
		}

		if (_wmkdir(UtfPathToWidePath(normPath)) != 0 && errno != EEXIST)
		{
			errmsg.Format("could not create directory %s: %s", *normPath, *GetLastErrorMessage());
			return false;
		}

		if (DirectoryExists(normPath))
		{
			return true;
		}

		if (FileExists(normPath))
		{
			errmsg.Format("path %s is not a directory", *normPath);
			return false;
		}
	}

	errmsg.Format("path %s does not exist and could not be created", *normPath);
	return false;
}
#else
bool FileSystem::ForceDirectories(const char* path, CString& errmsg)
{
	errmsg.Clear();
	BString<1024> normPath = path;
	NormalizePathSeparators(normPath);
	int len = strlen(normPath);
	if ((len > 0) && normPath[len - 1] == PATH_SEPARATOR)
	{
		normPath[len - 1] = '\0';
	}

	struct stat buffer;
	bool ok = !stat(normPath, &buffer);
	if (!ok && errno != ENOENT)
	{
		errmsg.Format("could not read information for directory %s: errno %i, %s",
			*normPath, errno, *GetLastErrorMessage());
		return false;
	}

	if (ok && !S_ISDIR(buffer.st_mode))
	{
		errmsg.Format("path %s is not a directory", *normPath);
		return false;
	}

	if (!ok)
	{
		BString<1024> parentPath = *normPath;
		char* p = (char*)strrchr(parentPath, PATH_SEPARATOR);
		if (p)
		{
			*p = '\0';
			if (strlen(parentPath) != strlen(path) && !ForceDirectories(parentPath, errmsg))
			{
				return false;
			}
		}

		if (mkdir(normPath, S_DIRMODE) != 0 && errno != EEXIST)
		{
			errmsg.Format("could not create directory %s: %s", *normPath, *GetLastErrorMessage());
			return false;
		}

		if (stat(normPath, &buffer) != 0)
		{
			errmsg.Format("could not read information for directory %s: %s",
				*normPath, *GetLastErrorMessage());
			return false;
		}

		if (!S_ISDIR(buffer.st_mode))
		{
			errmsg.Format("path %s is not a directory", *normPath);
			return false;
		}
	}

	return true;
}
#endif

CString FileSystem::GetCurrentDirectory()
{
#ifdef WIN32
	wchar_t unistr[1024];
	::GetCurrentDirectoryW(1024, unistr);
	return WidePathToUtfPath(unistr);
#else
	char str[1024];
	getcwd(str, 1024);
	return str;
#endif
}

bool FileSystem::SetCurrentDirectory(const char* dirFilename)
{
#ifdef WIN32
	return ::SetCurrentDirectoryW(UtfPathToWidePath(dirFilename));
#else
	return chdir(dirFilename) == 0;
#endif
}

bool FileSystem::DirEmpty(const char* dirFilename)
{
	DirBrowser dir(dirFilename);
	return dir.Next() == nullptr;
}

bool FileSystem::LoadFileIntoBuffer(const char* filename, CharBuffer& buffer, bool addTrailingNull)
{
	DiskFile file;
	if (!file.Open(filename, DiskFile::omRead))
	{
		return false;
	}

	// obtain file size.
	file.Seek(0, DiskFile::soEnd);
	int size  = (int)file.Position();
	file.Seek(0);

	// allocate memory to contain the whole file.
	buffer.Reserve(size + (addTrailingNull ? 1 : 0));

	// copy the file into the buffer.
	file.Read(buffer, size);
	file.Close();

	if (addTrailingNull)
	{
		buffer[size] = 0;
	}

	return true;
}

bool FileSystem::SaveBufferIntoFile(const char* filename, const char* buffer, int bufLen)
{
	DiskFile file;
	if (!file.Open(filename, DiskFile::omWrite))
	{
		return false;
	}

	int writtenBytes = (int)file.Write(buffer, bufLen);
	file.Close();

	return writtenBytes == bufLen;
}

bool FileSystem::AllocateFile(const char* filename, int64 size, [[maybe_unused]] bool sparse, CString& errmsg)
{
	errmsg.Clear();
	bool ok = false;
#ifdef WIN32
	HANDLE hFile = CreateFileW(UtfPathToWidePath(filename), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_NEW, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		errno = 0; // wanting error message from WinAPI instead of C-lib 
		errmsg = GetLastErrorMessage();
		return false;
	}

	if (sparse)
	{
		// try to create sparse file (supported only on NTFS partitions); it may fail but that's OK.
		DWORD dwBytesReturned;
		DeviceIoControl(hFile, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &dwBytesReturned, nullptr);
	}

	LARGE_INTEGER size64;
	size64.QuadPart = size;
	SetFilePointerEx(hFile, size64, nullptr, FILE_END);
	SetEndOfFile(hFile);
	CloseHandle(hFile);
	ok = true;
#else
	// create file
	FILE* file = fopen(filename, FOPEN_AB);
	if (!file)
	{
		errmsg = GetLastErrorMessage();
		return false;
	}
	fclose(file);

	// there are no reliable function to expand file on POSIX, so we must try different approaches,
	// starting with the fastest one and hoping it will work
	// 1) set file size using function "truncate" (this is fast, if it works)
	truncate(filename, size);
	// check if it worked
	ok = FileSize(filename) == size;
	if (!ok)
	{
		// 2) truncate did not work, expanding the file by writing to it (that's slow)
		truncate(filename, 0);
		file = fopen(filename, FOPEN_AB);
		if (!file)
		{
			errmsg = GetLastErrorMessage();
			return false;
		}

		// write zeros in 16K chunks
		CharBuffer zeros(16 * 1024);
		memset(zeros, 0, zeros.Size());
		for (int64 remaining = size; remaining > 0;)
		{
			int64 needbytes = std::min(remaining, (int64)zeros.Size());
			int64 written = fwrite(zeros, 1, needbytes, file);
			if (written != needbytes)
			{
				errmsg = GetLastErrorMessage();
				fclose(file);
				return false;
			}
			remaining -= written;
		}
		fclose(file);

		ok = FileSize(filename) == size;
		if (!ok)
		{
			errmsg = "created file has wrong size";
		}
	}
#endif
	return ok;
}

bool FileSystem::TruncateFile(const char* filename, int size)
{
#ifdef WIN32
	FILE* file = _wfopen(UtfPathToWidePath(filename), WString(FOPEN_RBP));
	fseek(file, size, SEEK_SET);
	bool ok = SetEndOfFile((HANDLE)_get_osfhandle(_fileno(file))) != 0;
	fclose(file);
	return ok;
#else
	return truncate(filename, size) == 0;
#endif
}

char* FileSystem::BaseFileName(const char* filename)
{
	char* p = (char*)strrchr(filename, PATH_SEPARATOR);
	char* p1 = (char*)strrchr(filename, ALT_PATH_SEPARATOR);
	if (p1)
	{
		if ((p && p < p1) || !p)
		{
			p = p1;
		}
	}
	if (p)
	{
		return p + 1;
	}
	else
	{
		return (char*)filename;
	}
}

std::pair<std::string, std::string> FileSystem::SplitPathAndFilename(const std::string& fullPath)
{
	size_t lastSlashPos = fullPath.find_last_of("/\\");

	if (lastSlashPos == std::string::npos)
	{
		return std::make_pair(fullPath, "");
	}

	std::string path = fullPath.substr(0, lastSlashPos);
	std::string filename = fullPath.substr(lastSlashPos + 1);

	return std::make_pair(std::move(path), std::move(filename));
}

bool FileSystem::ReservedChar(char ch)
{
	if ((unsigned char)ch < 32)
	{
		return true;
	}
	else
	{
		switch (ch)
		{
			case '"':
			case '*':
			case '/':
			case ':':
			case '<':
			case '>':
			case '?':
			case '\\':
			case '|':
				return true;
		}
	}

	return false;
}

//replace bad chars in filename
CString FileSystem::MakeValidFilename(const char* filename, bool allowSlashes)
{
	CString result = filename;

	for (char* p = result; *p; p++)
	{
		if (ReservedChar(*p))
		{
			if (allowSlashes && (*p == PATH_SEPARATOR || *p == ALT_PATH_SEPARATOR))
			{
				*p = PATH_SEPARATOR;
				continue;
			}
			*p = '_';
		}
	}

	// remove trailing dots and spaces. they are not allowed in directory names on windows,
	// but we remove them on posix too, in a case the directory is accessed from windows via samba.
	for (int len = strlen(result); len > 0 && (result[len - 1] == '.' || result[len - 1] == ' '); len--)
	{
		result[len - 1] = '\0';
	}

	// check if the filename starts with a reserved device name.
	// although these names are reserved only on Windows we adjust them on posix too,
	// in a case the directory is accessed from windows via samba.
	for (const char** ptr = RESERVED_DEVICE_NAMES; const char* reserved = *ptr; ptr++)
	{
		int len = strlen(reserved);
		if (!strncasecmp(result, reserved, len) && (result[len] == '.' || result[len] == '\0'))
		{
			result = CString::FormatStr("_%s", *result);
			break;
		}
	}			

	return result;
}

CString FileSystem::MakeUniqueFilename(const char* destDir, const char* basename)
{
	CString result;
	result.Format("%s%c%s", destDir, PATH_SEPARATOR, basename);

	int dupeNumber = 0;
	while (FileExists(result))
	{
		dupeNumber++;

		const char* extension = strrchr(basename, '.');
		if (extension && extension != basename)
		{
			BString<1024> filenameWithoutExt = basename;
			int end = (int)(extension - basename);
			filenameWithoutExt[end < 1024 ? end : 1024-1] = '\0';

			if (!strcasecmp(extension, ".par2"))
			{
				char* volExtension = strrchr(filenameWithoutExt, '.');
				if (volExtension && volExtension != filenameWithoutExt &&
					!strncasecmp(volExtension, ".vol", 4))
				{
					*volExtension = '\0';
					extension = basename + (volExtension - filenameWithoutExt);
				}
			}

			result.Format("%s%c%s.duplicate%d%s", destDir, PATH_SEPARATOR,
				*filenameWithoutExt, dupeNumber, extension);
		}
		else
		{
			result.Format("%s%c%s.duplicate%d", destDir, PATH_SEPARATOR,
				basename, dupeNumber);
		}
	}

	return result;
}

bool FileSystem::MoveFile(const char* srcFilename, const char* dstFilename)
{
#ifdef WIN32
	return _wrename(UtfPathToWidePath(srcFilename), UtfPathToWidePath(dstFilename)) == 0;
#else
	bool ok = rename(srcFilename, dstFilename) == 0;
	if (!ok && errno == EXDEV)
	{
		ok = CopyFile(srcFilename, dstFilename) && DeleteFile(srcFilename);
	}
	return ok;
#endif
}

bool FileSystem::CopyFile(const char* srcFilename, const char* dstFilename)
{
	DiskFile infile;
	if (!infile.Open(srcFilename, DiskFile::omRead))
	{
		return false;
	}

	DiskFile outfile;
	if (!outfile.Open(dstFilename, DiskFile::omWrite))
	{
		return false;
	}

	CharBuffer buffer(1024 * 50);

	int cnt = buffer.Size();
	while (cnt == buffer.Size())
	{
		cnt = (int)infile.Read(buffer, buffer.Size());
		outfile.Write(buffer, cnt);
	}

	infile.Close();
	outfile.Close();

	return true;
}

bool FileSystem::DeleteFile(const char* filename)
{
#ifdef WIN32
	SetFileAttributes(filename, FILE_ATTRIBUTE_NORMAL);
	return _wremove(UtfPathToWidePath(filename)) == 0;
#else
	return remove(filename) == 0;
#endif
}

bool FileSystem::FileExists(const char* filename)
{
#ifdef WIN32
	// we use a native windows call because c-lib function "stat" fails on windows if file date is invalid
	WIN32_FIND_DATAW findData;
	HANDLE handle = FindFirstFileW(UtfPathToWidePath(filename), &findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		bool exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
		FindClose(handle);
		return exists;
	}
	return false;
#else
	struct stat buffer;
	bool exists = !stat(filename, &buffer) && S_ISREG(buffer.st_mode);
	return exists;
#endif
}

bool FileSystem::DirectoryExists(const char* dirFilename)
{
#ifdef WIN32
	WIN32_FIND_DATAW findData;
	HANDLE handle = FindFirstFileW(UtfPathToWidePath(
		BString<1024>(dirFilename && dirFilename[strlen(dirFilename) - 1] == PATH_SEPARATOR ? "%s*" : "%s\\*", dirFilename)),
		&findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		bool exists = ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
			(dirFilename[0] != '\0' && dirFilename[1] == ':' && (dirFilename[2] == '\0' || dirFilename[3] == '\0'));
		FindClose(handle);
		return exists;
	}
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		// path exists but doesn't have any file/directory entries - possible only for root paths (e. g. "C:\")
		return true;
	}
	return false;
#else
	struct stat buffer;
	bool exists = !stat(dirFilename, &buffer) && S_ISDIR(buffer.st_mode);
	return exists;
#endif
}

bool FileSystem::CreateDirectory(const char* dirFilename)
{
#ifdef WIN32
	_wmkdir(UtfPathToWidePath(dirFilename));
#else
	mkdir(dirFilename, S_DIRMODE);
#endif
	return DirectoryExists(dirFilename);
}

bool FileSystem::RemoveDirectory(const char* dirFilename)
{
#ifdef WIN32
	return _wrmdir(UtfPathToWidePath(dirFilename)) == 0;
#else
	return rmdir(dirFilename) == 0;
#endif
}


std::string FileSystem::ExtractFilePathFromCmd(const std::string& path)
{
	if (path.empty())
	{
		return path;
	}

	size_t lastSeparatorPos = path.find_last_of(PATH_SEPARATOR);
	if (lastSeparatorPos != std::string::npos)
	{
		size_t possibleKeysPos = path.find(" ", lastSeparatorPos);

		if (possibleKeysPos != std::string::npos)
		{
			return path.substr(0, possibleKeysPos);
		}
	}

	return path;
}

std::string FileSystem::EscapePathForShell(const std::string& path)
{
	if (path.empty())
	{
		return path;
	}

	return "\"" + path + "\"";
}

std::optional<std::string> FileSystem::GetFileExtension(const std::string& filename)
{
	size_t extIdx = filename.rfind(".");
	if (extIdx == std::string::npos)
		return std::nullopt;

	return filename.substr(extIdx);
}

/* Delete directory which is empty or contains only hidden files or directories (whose names start with dot) */
bool FileSystem::DeleteDirectory(const char* dirFilename)
{
	if (RemoveDirectory(dirFilename))
	{
		return true;
	}

	// check if directory contains only hidden files (whose names start with dot)
	{
		DirBrowser dir(dirFilename);
		while (const char* filename = dir.Next())
		{
			if (*filename != '.')
			{
				// calling RemoveDirectory to set correct errno
				return RemoveDirectory(dirFilename);
			}
		}
	} // make sure "DirBrowser dir" is destroyed (and has closed its handle) before we trying to delete the directory

	CString errmsg;
	if (!DeleteDirectoryWithContent(dirFilename, errmsg))
	{
		// calling RemoveDirectory to set correct errno
		return RemoveDirectory(dirFilename);
	}

	return true;
}

bool FileSystem::DeleteDirectoryWithContent(const char* dirFilename, CString& errmsg)
{
	errmsg.Clear();

	bool del = false;
	bool ok = true;

	{
		DirBrowser dir(dirFilename);
		while (const char* filename = dir.Next())
		{
			BString<1024> fullFilename("%s%c%s", dirFilename, PATH_SEPARATOR, filename);

			if (FileSystem::DirectoryExists(fullFilename))
			{
				del = DeleteDirectoryWithContent(fullFilename, errmsg);
			}
			else
			{
				del = DeleteFile(fullFilename);
			}
			ok &= del;
			if (!del && errmsg.Empty())
			{
				errmsg.Format("could not delete %s: %s", *fullFilename, *GetLastErrorMessage());
			}
		}
	} // make sure "DirBrowser dir" is destroyed (and has closed its handle) before we trying to delete the directory

	del = RemoveDirectory(dirFilename);
	ok &= del;
	if (!del && errmsg.Empty())
	{
		errmsg = GetLastErrorMessage();
	}
	return ok;
}

int64 FileSystem::FileSize(const char* filename)
{
#ifdef WIN32
	// we use a native windows call because c-lib function "stat" fails on windows if file date is invalid
	WIN32_FIND_DATAW findData;
	HANDLE handle = FindFirstFileW(UtfPathToWidePath(filename), &findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		int64 size = ((int64)(findData.nFileSizeHigh) << 32) + findData.nFileSizeLow;
		FindClose(handle);
		return size;
	}
	return -1;
#else
	struct stat buffer;
	buffer.st_size = -1;
	stat(filename, &buffer);
	return buffer.st_size;
#endif
}

std::optional<FileSystem::DiskState> FileSystem::GetDiskState(const char* path)
{
#ifdef WIN32
	ULARGE_INTEGER freeBytesAvailable;
	ULARGE_INTEGER totalNumberOfBytes;

	if (GetDiskFreeSpaceEx(path, &freeBytesAvailable, &totalNumberOfBytes, nullptr))
	{
		int64 available = Util::SafeIntCast<uint64, int64>(freeBytesAvailable.QuadPart);
		int64 total = Util::SafeIntCast<uint64, int64>(totalNumberOfBytes.QuadPart);
		return FileSystem::DiskState{ available, total };
	}
#else
	struct statvfs diskdata;
	if (!statvfs(path, &diskdata))
	{
		int64 available = Util::SafeIntCast<uint64, int64>(diskdata.f_bavail * diskdata.f_frsize);
		int64 total = Util::SafeIntCast<uint64, int64>(diskdata.f_blocks * diskdata.f_frsize);
		return FileSystem::DiskState{ available, total };
	}
#endif
	return std::nullopt;
}

bool FileSystem::RenameBak(const char* filename, const char* bakPart, bool removeOldExtension, CString& newName)
{
	BString<1024> changedFilename;

	if (removeOldExtension)
	{
		changedFilename = filename;
		char* extension = strrchr(changedFilename, '.');
		if (extension)
		{
			*extension = '\0';
		}
	}

	newName.Format("%s.%s", removeOldExtension ? *changedFilename : filename, bakPart);

	int i = 2;
	while (FileExists(newName) || DirectoryExists(newName))
	{
		newName.Format("%s.%i.%s", removeOldExtension ? *changedFilename : filename, i++, bakPart);
	}

	return MoveFile(filename, newName);
}

#ifndef WIN32
CString FileSystem::ExpandHomePath(const char* filename)
{
	CString result;

	if (filename && (filename[0] == '~') && (filename[1] == '/'))
	{
		// expand home-dir

		char* home = getenv("HOME");
		if (!home)
		{
			struct passwd *pw = getpwuid(getuid());
			if (pw)
			{
				home = pw->pw_dir;
			}
		}

		if (!home)
		{
			return filename;
		}

		if (home[strlen(home)-1] == '/')
		{
			result.Format("%s%s", home, filename + 2);
		}
		else
		{
			result.Format("%s/%s", home, filename + 2);
		}
	}
	else
	{
		result.Append(filename ? filename : "");
	}

	return result;
}
#endif

CString FileSystem::ExpandFileName(const char* filename)
{
#ifdef WIN32
	wchar_t unistr[1024];
	_wfullpath(unistr, UtfPathToWidePath(filename), 1024);
	return WidePathToUtfPath(unistr);
#else
	CString result;
	result.Reserve(1024 - 1);
	if (filename[0] != '\0' && filename[0] != '/')
	{
		char curDir[MAX_PATH + 1];
		getcwd(curDir, sizeof(curDir) - 1); // 1 char reserved for adding backslash
		int offset = 0;
		if (filename[0] == '.' && filename[1] == '/')
		{
			offset += 2;
		}
		result.Format("%s/%s", curDir, filename + offset);
	}
	else
	{
		result = filename;
	}
	return result;
#endif
}

CString FileSystem::GetExeFileName(const char* argv0)
{
	CString exename;
	exename.Reserve(1024 - 1);
	exename[1024 - 1] = '\0';

#ifdef WIN32
	GetModuleFileName(nullptr, exename, 1024);
#else
	// Linux
	int r = readlink("/proc/self/exe", exename, 1024 - 1);
	if (r > 0)
	{
		exename[r] = '\0';
		return exename;
	}
	// FreeBSD
	r = readlink("/proc/curproc/file", exename, 1024 - 1);
	if (r > 0)
	{
		exename[r] = '\0';
		return exename;
	}

	exename = ExpandFileName(argv0);
#endif

	return exename;
}

bool FileSystem::SameFilename(const char* filename1, const char* filename2)
{
#ifdef WIN32
	return strcasecmp(filename1, filename2) == 0;
#else
	return strcmp(filename1, filename2) == 0;
#endif
}

#ifdef WIN32
CString FileSystem::MakeCanonicalPath(const char* path)
{
	int len = strlen(path);

	if (!strncmp("\\\\?\\", path, 4) || len == 0)
	{
		return path;
	}

	std::vector<CString> components = Util::SplitStr(path, "\\/");
	for (uint32 i = 1; i < components.size(); i++)
	{
		if (!strcmp(components[i], ".."))
		{
			components.erase(components.begin() + i - 1, components.begin() + i + 1);
			i -= 2;
		}
		else if (!strcmp(components[i], "."))
		{
			components.erase(components.begin() + i);
			i--;
		}
	}

	StringBuilder result;
	result.Reserve(strlen(path));

	if (!strncmp("\\\\", path, 2))
	{
		result.Append("\\\\");
	}

	bool first = true;
	for (CString& comp : components)
	{
		if (comp.Length() > 0)
		{
			if (!first)
			{
				result.Append("\\");
			}
			result.Append(comp);
			first = false;
		}
	}

	if ((path[len - 1] == '\\' || path[len - 1] == '/' ||
		(len > 3 && !strcmp(path + len - 3, "\\..")) ||
		(len > 2 && !strcmp(path + len - 2, "\\.")))
		&&
		result[result.Length() - 1] != '\\')
	{
		result.Append("\\");
	}

	return *result;
}
#endif

bool FileSystem::FlushFileBuffers(int fileDescriptor, CString& errmsg)
{
#ifdef WIN32
	BOOL ok = ::FlushFileBuffers((HANDLE)_get_osfhandle(fileDescriptor));
	if (!ok)
	{
		errno = 0; // wanting error message from WinAPI instead of C-lib 
		errmsg = GetLastErrorMessage();
	}
	return ok;
#else
#ifdef HAVE_FULLFSYNC
	int ret = fcntl(fileDescriptor, F_FULLFSYNC) == -1 ? 1 : 0;
#elif HAVE_FDATASYNC
	int ret = fdatasync(fileDescriptor);
#else
	int ret = fsync(fileDescriptor);
#endif
	if (ret != 0)
	{
		errmsg = GetLastErrorMessage();
	}
	return ret == 0;
#endif
}

bool FileSystem::FlushDirBuffers(const char* filename, CString& errmsg)
{
#ifdef WIN32
	FILE* file = _wfopen(UtfPathToWidePath(filename), WString(FOPEN_RBP));
#else
	BString<1024> parentPath = filename;
	char* p = (char*)strrchr(parentPath, PATH_SEPARATOR);
	if (p)
	{
		*p = '\0';
	}
	FILE* file = fopen(parentPath, FOPEN_RB);
#endif

	if (!file)
	{
		errmsg = GetLastErrorMessage();
		return false;
	}
	bool ok = FlushFileBuffers(fileno(file), errmsg);
	fclose(file);
	return ok;
}

#ifndef WIN32

mode_t FileSystem::uMask;

void FileSystem::FixExecPermission(const char* filename)
{
	struct stat buffer;
	bool ok = !stat(filename, &buffer);
	if (ok)
	{
		buffer.st_mode = buffer.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
		chmod(filename, buffer.st_mode);
	}
}

bool FileSystem::RestoreFileOrDirPermissions(const char* filename) 
{
	struct stat buffer;
	int ec = stat(filename, &buffer);
	if (ec != 0) 
	{

#ifdef DEBUG
		debug("Failed to read information for %s. Errno: %i", filename, ec);
#endif

		return false;
	}

	if (S_ISDIR(buffer.st_mode)) 
	{
		return RestoreDirPermissions(filename);
	} 

	return RestoreFilePermissions(filename);
}

bool FileSystem::RestoreFilePermissions(const char* filepath)
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; // 0666
	return RestorePermissions(filepath, mode);
}

bool FileSystem::RestoreDirPermissions(const char* filename)
{
	mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; // 0777
	return RestorePermissions(filename, mode);
}

bool FileSystem::RestorePermissions(const char* filename, mode_t mode) 
{
	mode_t permissions = mode & ~FileSystem::uMask;
	int ec = chmod(filename, permissions);
	if (ec == 0)
	{

#ifdef DEBUG
		debug("Permissions %o was set for %s", permissions, filename);
#endif

		return true;
	} 

#ifdef DEBUG
	debug("Failed to set permissions for %s. Errno %i", filename, ec);
#endif

	return false;
}

#endif

#ifdef WIN32
bool FileSystem::NeedLongPath(const char* path)
{
	bool alreadyLongPath = !strncmp(path, "\\\\?\\", 4);
	if (alreadyLongPath)
	{
		return false;
	}

	if (strlen(path) > 260 - 14)
	{
		return true;
	}

	Tokenizer tok(path, "\\/");
	for (int partNo = 0; const char* part = tok.Next(); partNo++)
	{
		// check if the file part starts with a reserved device name
		for (const char** ptr = RESERVED_DEVICE_NAMES; const char* reserved = *ptr; ptr++)
		{
			int len = strlen(reserved);
			if (!strncasecmp(part, reserved, len) && (part[len] == '.' || part[len] == '\0'))
			{
				return true;
			}
		}			

		// check if the file part contains reserved characters
		for (const char* p = part; *p; p++)
		{
			if (ReservedChar(*p) && !(partNo == 0 && p == part + 1 && *p == ':'))
			{
				return true;
			}
		}
	}

	return false;
}
#endif

CString FileSystem::MakeExtendedPath(const char* path, [[maybe_unused]] bool force)
{
#ifdef WIN32
	if (force || NeedLongPath(path))
	{
		CString canonicalPath = MakeCanonicalPath(path);
		BString<1024> longpath;
		if (canonicalPath[0] == '\\' && canonicalPath[1] == '\\')
		{
			// UNC path
			longpath.Format("\\\\?\\UNC\\%s", canonicalPath + 2);
		}
		else
		{
			// local path
			longpath.Format("\\\\?\\%s", std::move(canonicalPath));
		}
		return *longpath;
	}
	else
#endif
	{
		return path;
	}
}

#ifdef WIN32
WString FileSystem::UtfPathToWidePath(const char* utfpath)
{
	return *FileSystem::MakeExtendedPath(utfpath, false);
}

CString FileSystem::WidePathToUtfPath(const wchar_t* wpath)
{
	char utfstr[1024];
	int copied = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utfstr, 1024, nullptr, nullptr);
	return utfstr;
}
#endif


DirBrowser::DirBrowser(const char* path, bool snapshot) :
	m_snapshot(snapshot)
{
	if (m_snapshot)
	{
		DirBrowser dir(path, false);
		while (const char* filename = dir.Next())
		{
			m_snapshotFiles.emplace_back(filename);
		}
		m_snapshotIter = m_snapshotFiles.begin();
	}
	else
	{
#ifdef WIN32
		BString<1024> mask("%s%c*.*", path, PATH_SEPARATOR);
		m_file = FindFirstFileW(FileSystem::UtfPathToWidePath(mask), &m_findData);
		m_first = true;
#else
		m_dir = opendir(path);
#endif
	}
}

DirBrowser::~DirBrowser()
{
#ifdef WIN32
	if (m_file != INVALID_HANDLE_VALUE)
	{
		FindClose(m_file);
	}
#else
	if (m_dir)
	{
		closedir(m_dir);
	}
#endif
}

const char* DirBrowser::InternNext()
{
	if (m_snapshot)
	{
		return m_snapshotIter == m_snapshotFiles.end() ? nullptr : **m_snapshotIter++;
	}
	else
	{
#ifdef WIN32
		bool ok = false;
		if (m_first)
		{
			ok = m_file != INVALID_HANDLE_VALUE;
			m_first = false;
		}
		else
		{
			ok = FindNextFileW(m_file, &m_findData) != 0;
		}
		if (ok)
		{
			m_filename = FileSystem::WidePathToUtfPath(m_findData.cFileName);
			return m_filename;
		}
#else
		if (m_dir)
		{
			m_findData = readdir(m_dir);
			if (m_findData)
			{
				return m_findData->d_name;
			}
		}
#endif
		return nullptr;
	}
}

const char* DirBrowser::Next()
{
	const char* filename = nullptr;
	for (filename = InternNext(); filename && (!strcmp(filename, ".") || !strcmp(filename, "..")); )
	{
		filename = InternNext();
	}
	return filename;
}


DiskFile::~DiskFile()
{
	if (m_file)
	{
		Close();
	}
}

bool DiskFile::Open(const char* filename, EOpenMode mode)
{
	const char* strmode = mode == omRead ? FOPEN_RB : mode == omReadWrite ?
		FOPEN_RBP : mode == omWrite ? FOPEN_WB : FOPEN_AB;
#ifdef WIN32
	m_file = _wfopen(FileSystem::UtfPathToWidePath(filename), WString(strmode));
#else
	m_file = fopen(filename, strmode);
#endif
	return m_file;
}

bool DiskFile::Close()
{
	if (m_file)
	{
		int ret = fclose(m_file);
		m_file = nullptr;
		return ret;
	}
	else
	{
		return false;
	}
}

int64 DiskFile::Read(void* buffer, int64 size)
{
	return fread(buffer, 1, (size_t)size, m_file);
}

int64 DiskFile::Write(const void* buffer, int64 size)
{
	return fwrite(buffer, 1, (size_t)size, m_file);
}

int64 DiskFile::Print(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	int ret = vfprintf(m_file, format, ap);
	va_end(ap);
	return ret;
}

char* DiskFile::ReadLine(char* buffer, int64 size)
{
	return fgets(buffer, (int)size, m_file);
}

int64 DiskFile::Position()
{
	return ftell(m_file);
}

bool DiskFile::Seek(int64 position, ESeekOrigin origin)
{
	return fseek(m_file, position,
		origin == soCur ? SEEK_CUR :
		origin == soEnd ? SEEK_END : SEEK_SET) == 0;
}

bool DiskFile::Eof()
{
	return feof(m_file) != 0;
}

bool DiskFile::Error()
{
	return ferror(m_file) != 0;
}

bool DiskFile::SetWriteBuffer(int size)
{
	return setvbuf(m_file, nullptr, _IOFBF, size) == 0;
}

bool DiskFile::Flush()
{
	return fflush(m_file) == 0;
}

bool DiskFile::Sync(CString& errmsg)
{
	return FileSystem::FlushFileBuffers(fileno(m_file), errmsg);
}

