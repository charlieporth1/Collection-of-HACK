/*
 *	$Id: extfs_macosx.mm,v 1.9 2007/01/24 02:37:06 asvitkine Exp $
 *
 *	extfs_macosx.mm - Access Mac OS X Finder and resource information (using Carbon calls).
 *                    Based on:
 *
 *  extfs_unix.cpp - MacOS file system for access native file system access, Unix specific stuff
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "sysdeps.h"
#include "extfs_macosx.h"
#include "extfs.h"

#define DEBUG 0
#include "debug.h"


// Default Finder flags
const uint16 DEFAULT_FINDER_FLAGS = kHasBeenInited;


/*
 *  Initialization
 */

void extfs_init(void)
{
}


/*
 *  Deinitialization
 */

void extfs_exit(void)
{
}


/*
 *  Add component to path name
 */

void add_path_component(char *path, const char *component)
{
	int l = strlen(path);
	if (l < MAX_PATH_LENGTH-1 && path[l-1] != '/') {
		path[l] = '/';
		path[l+1] = 0;
	}
	strncat(path, component, MAX_PATH_LENGTH-1);
}


/*
 *  Add /rsrc to path name. Note that the 'correct' way to do this is to
 *  append '/..namedfork/rsrc', but I use this short form to save chars.
 */

void add_rsrc(const char *path, char *dest)
{
	int l = strlen(path);

	if ( l > MAX_PATH_LENGTH-5 )		// If there is no room to add "rsrc\0"
		return;

	*dest = '\0';
	strncat(dest, path, l);

	if (l < MAX_PATH_LENGTH-1 && path[l-1] != '/') {
		dest[l] = '/';
		dest[l+1] = 0;
	}
	strcat(dest, "rsrc");
}


/*
 * Resource forks on Mac OS X are kept on a UFS volume as /path/file/rsrc
 *
 * On HFS volumes, there is of course the data and rsrc fork, but the Darwin
 * filesystem layer presents the resource fork using the above pseudo path
 */

static int open_rsrc(const char *path, int flag)
{
	char path_rsrc[MAX_PATH_LENGTH];

	add_rsrc(path, path_rsrc);
	return open(path_rsrc, flag);
}


/*
 * Finder info is a little bit harder. We need to use Carbon library calls to access them
 */


/*
 *  Get/set finder info for file/directory specified by full path
 */

struct ext2type {
	const char *ext;
	uint32 type;
	uint32 creator;
};

static const ext2type e2t_translation[] = {
	{".Z", FOURCC('Z','I','V','M'), FOURCC('L','Z','I','V')},
	{".gz", FOURCC('G','z','i','p'), FOURCC('G','z','i','p')},
	{".hqx", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".bin", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".pdf", FOURCC('P','D','F',' '), FOURCC('C','A','R','O')},
	{".ps", FOURCC('T','E','X','T'), FOURCC('t','t','x','t')},
	{".sit", FOURCC('S','I','T','!'), FOURCC('S','I','T','x')},
	{".tar", FOURCC('T','A','R','F'), FOURCC('T','A','R',' ')},
	{".uu", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".uue", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".zip", FOURCC('Z','I','P',' '), FOURCC('Z','I','P',' ')},
	{".8svx", FOURCC('8','S','V','X'), FOURCC('S','N','D','M')},
	{".aifc", FOURCC('A','I','F','C'), FOURCC('T','V','O','D')},
	{".aiff", FOURCC('A','I','F','F'), FOURCC('T','V','O','D')},
	{".au", FOURCC('U','L','A','W'), FOURCC('T','V','O','D')},
	{".mid", FOURCC('M','I','D','I'), FOURCC('T','V','O','D')},
	{".midi", FOURCC('M','I','D','I'), FOURCC('T','V','O','D')},
	{".mp2", FOURCC('M','P','G',' '), FOURCC('T','V','O','D')},
	{".mp3", FOURCC('M','P','G',' '), FOURCC('T','V','O','D')},
	{".wav", FOURCC('W','A','V','E'), FOURCC('T','V','O','D')},
	{".bmp", FOURCC('B','M','P','f'), FOURCC('o','g','l','e')},
	{".gif", FOURCC('G','I','F','f'), FOURCC('o','g','l','e')},
	{".lbm", FOURCC('I','L','B','M'), FOURCC('G','K','O','N')},
	{".ilbm", FOURCC('I','L','B','M'), FOURCC('G','K','O','N')},
	{".jpg", FOURCC('J','P','E','G'), FOURCC('o','g','l','e')},
	{".jpeg", FOURCC('J','P','E','G'), FOURCC('o','g','l','e')},
	{".pict", FOURCC('P','I','C','T'), FOURCC('o','g','l','e')},
	{".png", FOURCC('P','N','G','f'), FOURCC('o','g','l','e')},
	{".sgi", FOURCC('.','S','G','I'), FOURCC('o','g','l','e')},
	{".tga", FOURCC('T','P','I','C'), FOURCC('o','g','l','e')},
	{".tif", FOURCC('T','I','F','F'), FOURCC('o','g','l','e')},
	{".tiff", FOURCC('T','I','F','F'), FOURCC('o','g','l','e')},
	{".htm", FOURCC('T','E','X','T'), FOURCC('M','O','S','S')},
	{".html", FOURCC('T','E','X','T'), FOURCC('M','O','S','S')},
	{".txt", FOURCC('T','E','X','T'), FOURCC('t','t','x','t')},
	{".rtf", FOURCC('T','E','X','T'), FOURCC('M','S','W','D')},
	{".c", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".C", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cc", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cpp", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cxx", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".h", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hh", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hpp", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hxx", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".s", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".S", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".i", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".mpg", FOURCC('M','P','E','G'), FOURCC('T','V','O','D')},
	{".mpeg", FOURCC('M','P','E','G'), FOURCC('T','V','O','D')},
	{".mov", FOURCC('M','o','o','V'), FOURCC('T','V','O','D')},
	{".fli", FOURCC('F','L','I',' '), FOURCC('T','V','O','D')},
	{".avi", FOURCC('V','f','W',' '), FOURCC('T','V','O','D')},
	{".qxd", FOURCC('X','D','O','C'), FOURCC('X','P','R','3')},
	{".hfv", FOURCC('D','D','i','m'), FOURCC('d','d','s','k')},
	{".dsk", FOURCC('D','D','i','m'), FOURCC('d','d','s','k')},
	{".img", FOURCC('r','o','h','d'), FOURCC('d','d','s','k')},
	{NULL, 0, 0}	// End marker
};

// Mac OS X way of doing the above

#import <Foundation/NSString.h>

extern "C"
{
	NSString	*NSHFSTypeOfFile			(const NSString *);
	uint32		NSHFSTypeCodeFromFileType	(const NSString *);		// Actually returns an OSType
}

uint32 fileType (const char *cPath)
{
	NSString	*copy = [NSString stringWithCString: cPath],
				*type = NSHFSTypeOfFile(copy);

	if ( type == nil )
	{
		D(NSLog(@"No type for file %s", cPath));
		return 0;								// Should this be '????' or '    ' ?
	}

	D(NSLog(@"Got type %@ for %s", type, cPath));
	return NSHFSTypeCodeFromFileType(type);
}


void get_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
    FSRef	fsRef;
    int32	status;
	// Set default finder info
	Mac_memset(finfo, 0, SIZEOF_FInfo);
	if (fxinfo)
		Mac_memset(fxinfo, 0, SIZEOF_FXInfo);
	WriteMacInt16(finfo + fdFlags, DEFAULT_FINDER_FLAGS);
	WriteMacInt32(finfo + fdLocation, (uint32)-1);

	status = FSPathMakeRef((const uint8 *)path, &fsRef, NULL);
	if ( status == noErr )
	{
		FSCatalogInfo	cInfo;
		uint32			AllFinderInfo = kFSCatInfoFinderInfo;

		if (fxinfo)
			AllFinderInfo |= kFSCatInfoFinderXInfo;

		status = FSGetCatalogInfo(&fsRef, AllFinderInfo, &cInfo, NULL, NULL, NULL);
		if ( status == noErr )
		{
			// byte-swap them to big endian (network order) if necessary
			((FileInfo *)&cInfo.finderInfo)->fileType = htonl(((FileInfo *)&cInfo.finderInfo)->fileType);
			((FileInfo *)&cInfo.finderInfo)->fileCreator = htonl(((FileInfo *)&cInfo.finderInfo)->fileCreator);
			((FileInfo *)&cInfo.finderInfo)->finderFlags = htons(((FileInfo *)&cInfo.finderInfo)->finderFlags);
			D(printf("get_finfo(%s,...) - Got info of '%16.16s'\n", path, cInfo.finderInfo));
			Host2Mac_memcpy(finfo, &cInfo.finderInfo, SIZEOF_FInfo);
			if (fxinfo)
				Host2Mac_memcpy(fxinfo, &cInfo.extFinderInfo, SIZEOF_FXInfo);
			return;
		}
		else
			printf("get_finfo(%s,...) failed to get catalog info\n", path);
	}
	else
		printf("get_finfo(%s,...) failed to get FSRef\n", path);


	// No Finder info file, translate file name extension to MacOS type/creator
	if (!is_dir) {
		int path_len = strlen(path);
		for (int i=0; e2t_translation[i].ext; i++) {
			int ext_len = strlen(e2t_translation[i].ext);
			if (path_len < ext_len)
				continue;
			if (!strcmp(path + path_len - ext_len, e2t_translation[i].ext)) {
				WriteMacInt32(finfo + fdType, e2t_translation[i].type);
				WriteMacInt32(finfo + fdCreator, e2t_translation[i].creator);
				break;
			}
		}
	}


	// Use alternate code to get type
	uint32	type = fileType(path);

	if ( type )
		WriteMacInt32(finfo + fdType, type);
}

void set_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
    FSRef	fsRef;
    OSErr	status;

	status = FSPathMakeRef((const uint8 *)path, &fsRef, NULL);
	if ( status == noErr )
	{
		FSCatalogInfo	cInfo;

		status = FSGetCatalogInfo(&fsRef, kFSCatInfoFinderInfo, &cInfo, NULL, NULL, NULL);
		if ( status == noErr )
		{
			FSCatalogInfoBitmap whichInfo = kFSCatInfoNone;
			if (finfo) {
				whichInfo |= kFSCatInfoFinderInfo;
				/*
				  FIXME: Some users reported that directories could
				  mysteriously disappear from a MacOS X 10.4 Finder
				  point of view.

				  An alternative is to use the ".finfo/" technique
				  on MacOS X provided that attributes are first
				  initialised from an FSGetCatalogInfo() call.
				*/
				uint8 oldFlags1, oldFlags2;
				if (is_dir) {
					oldFlags1 = cInfo.finderInfo[fdFlags];
					oldFlags2 = cInfo.finderInfo[fdFlags + 1];
				}
				Mac2Host_memcpy(&cInfo.finderInfo,    finfo,  SIZEOF_FInfo);
				if (is_dir) {
					cInfo.finderInfo[fdFlags] = oldFlags1;
					cInfo.finderInfo[fdFlags + 1] = oldFlags2;
				}
			}
			if (fxinfo) {
				whichInfo |= kFSCatInfoFinderXInfo;
				Mac2Host_memcpy(&cInfo.extFinderInfo, fxinfo, SIZEOF_FXInfo);
			}
			// byte-swap them to system byte order from big endian (network order) if necessary
			((FileInfo *)&cInfo.finderInfo)->fileType = ntohl(((FileInfo *)&cInfo.finderInfo)->fileType);
			((FileInfo *)&cInfo.finderInfo)->fileCreator = ntohl(((FileInfo *)&cInfo.finderInfo)->fileCreator);
			((FileInfo *)&cInfo.finderInfo)->finderFlags = ntohs(((FileInfo *)&cInfo.finderInfo)->finderFlags);
			FSSetCatalogInfo(&fsRef, whichInfo, &cInfo);
		}
    }
}


/*
 *  Resource fork emulation functions
 */

uint32 get_rfork_size(const char *path)
{
	// Open resource file
	int fd = open_rsrc(path, O_RDONLY);
	if (fd < 0)
		return 0;

	// Get size
	off_t size = lseek(fd, 0, SEEK_END);
	
	// Close file and return size
	close(fd);
	return size < 0 ? 0 : size;
}

int open_rfork(const char *path, int flag)
{
	return open_rsrc(path, flag);
}

void close_rfork(const char *path, int fd)
{
	close(fd);
}


/*
 *  Read "length" bytes from file to "buffer",
 *  returns number of bytes read (or -1 on error)
 */

ssize_t extfs_read(int fd, void *buffer, size_t length)
{
	return read(fd, buffer, length);
}


/*
 *  Write "length" bytes from "buffer" to file,
 *  returns number of bytes written (or -1 on error)
 */

ssize_t extfs_write(int fd, void *buffer, size_t length)
{
	return write(fd, buffer, length);
}


/*
 *  Remove file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_remove(const char *path)
{
	// Remove helpers first, don't complain if this fails
	char helper_path[MAX_PATH_LENGTH];
	add_rsrc(path, helper_path);
	remove(helper_path);

	// Now remove file or directory (and helper directories in the directory)
	if (remove(path) < 0) {
		if (errno == EISDIR || errno == ENOTEMPTY) {
			return rmdir(path) == 0;
		} else
			return false;
	}
	return true;
}


/*
 *  Rename/move file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_rename(const char *old_path, const char *new_path)
{
	// Rename helpers first, don't complain if this fails
	char old_helper_path[MAX_PATH_LENGTH], new_helper_path[MAX_PATH_LENGTH];
	add_rsrc(old_path, old_helper_path);
	add_rsrc(new_path, new_helper_path);
	rename(old_helper_path, new_helper_path);

	// Now rename file
	return rename(old_path, new_path) == 0;
}


// Convert from the host OS filename encoding to MacRoman
const char *host_encoding_to_macroman(const char *filename)
{
	static char filename_mr[64];
	CFStringRef sref = CFStringCreateWithCString(0, filename, kCFStringEncodingUTF8);
	if (sref) {
		memset(filename_mr, 0, sizeof(filename_mr));
		if (CFStringGetCString(sref, filename_mr, sizeof(filename_mr), kCFStringEncodingMacRoman)) {
			return filename_mr;
		}
		CFRelease(sref);
	}
	return filename;
}


// Convert from MacRoman to the host OS filename encoding
const char *macroman_to_host_encoding(const char *filename)
{
	static char filename_mr[64];
	CFStringRef sref = CFStringCreateWithCString(0, filename, kCFStringEncodingMacRoman);
	if (sref) {
		memset(filename_mr, 0, sizeof(filename_mr));
		if (CFStringGetCString(sref, filename_mr, sizeof(filename_mr), kCFStringEncodingUTF8)) {
			return filename_mr;
		}
		CFRelease(sref);
	}
	return filename;
}

