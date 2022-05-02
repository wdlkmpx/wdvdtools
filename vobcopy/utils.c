/* vobcopy 1.3.0
 *
 * Copyright (c) 2001 - 2009 robos@muon.de
 * Lots of contribution and cleanup from rosenauer@users.sourceforge.net
 * Critical bug-fix from Bas van den Heuvel
 * Takeshi HIYAMA made lots of changes to get it to run on FreeBSD
 * Erik Hovland made changes for solaris
 *  This file is part of vobcopy.
 *
 *  vobcopy is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  vobcopy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with vobcopy; if not, see <http://www.gnu.org/licenses/>.
 */

#include "vobcopy.h"

const long long BLOCK_SIZE          = 512LL; /*I believe this has more to do with POSIX*/
const long long DVD_SECTOR_SIZE     = 2048LL;
/*The first data block in the dvd is unused by the filesystem and reserved for the system*/
const long long DVD_DATA_BLOCK_SIZE = 32768LL; /*16*DVD_SECTOR_SIZE or 2^15 or 32768*/

const long long KILO = 1024LL;
const long long MEGA = (1024LL * 1024LL);
const long long GIGA = (1024LL * 1024LL * 1024LL);

const char *QUIET_LOG_FILE = "vobcopy.bla";

/*Confident this breaks some preprocessor magic*/
#include <stdarg.h>

void die(const char *cause_of_death, ...)
{
	va_list args;

	va_start(args, cause_of_death);
	vfprintf(stderr, cause_of_death, args);
	va_end(args);

	exit(1);
}

/*printf for stderr*/
void printe(char *str, ...)
{
	va_list args;

	va_start(args, str);
	if (vfprintf(stderr, str, args) < 0)
		die("vfprintf failed\n");
	va_end(args);
}

/*A lot of magic numbers involve sectors*/
off_t get_sector_offset(long long unsigned int sector)
{
	/*Not sure if this is subject to integer overflow*/
	return (off_t)(sector * DVD_SECTOR_SIZE);
}

void *palloc(size_t element, size_t elements)
{
	void *ret;

	ret = calloc(element, elements);
	if (!ret)
		die("[Error] Failed to allocate memory!\n");

	return ret;
}

/*Replace characters in str, that are orig, with new*/
void strrepl(char *str, char orig, char new)
{
	/* B. Watson, aka Urchlay on freenode
	 * wrote this function
	 */
	while (*str) {
		if ((*str) == orig)
			(*str) = new;
		str++;
	}
}

void capitalize(char *str, size_t len)
{
	char c;
	size_t i;

	for (i = 0; (i < len) && (c = str[i]); i++)
		if (islower(c))
			str[i] = toupper(c);
}

/*GNU SOURCE has this function*/
#if !defined( _GNU_SOURCE )
char *strcasestr(const char *haystack, const char *needle)
{
	size_t i, j, haystacklen, needlelen;

	needlelen   = strlen(needle);
	haystacklen = strlen(haystack);
	for (i = 0; i < haystacklen; i++) {
		for (j = 0;; j++) {
			/*If we've gotten to the end of haystack, there is no match.*/
			if ((i+j+1) > haystacklen)
				return NULL;
			/*Check if the two strings differ, if so, check another part of haystack*/
			else if (toupper(haystack[i+j]) != toupper(needle[j]))
				break;
			/*If we've gotten to the end of needle, we're done*/
			else if ((j+1) == needlelen)
				return ((char*)haystack)+i;
		}
	}

	return NULL;
}
#endif

/*This was implemented five different times in a few functions*/
/* options_str: Informs the user of options to take
 * opts:        Valid replies (For function use only)
 */
char get_option(char *options_str, const char *opts)
{
	int c;

	printe(options_str);
	while ((c = fgetc(stdin))) {
		if (strchr(opts, c))
			return c;
		else
			printe(options_str);

		/*Sleeps for a 10th of a second for each iteration*/
		usleep(100000);
	}

	return c;
}



/*Zero's string, and then copies the source with one space for null*/
char *safestrncpy(char *dest, const char *src, size_t n)
{
	memset(dest, 0, n);

	if (n <= 1)
		return dest;

	return strncpy(dest, src, n-1);
}

/*Found this copied and pasted twice in the code*/
long long unsigned int suffix2llu(char input)
{
	switch (input) {
	case 'b':
	case 'B':
		return BLOCK_SIZE; /*blocks (normal, not dvd) */
	case 'k':
	case 'K':
		return KILO;
	case 'm':
	case 'M':
		return MEGA;
	case 'g':
	case 'G':
		return GIGA;
	case '?':
	default:
		die("[Error] Wrong suffix behind -b, only b,k,m or g \n");
	}

	/*impossible return*/
	return 0;
}

long long unsigned int opt2llu(char *opt, char optchar)
{
	char* suffix;
	long long int intarg;
	long long unsigned int ret;

	if (!isdigit(opt[0]))
		die("[Error] The thing behind -%c has to be a number! \n", optchar);

	intarg = atoll(optarg);
	suffix = strpbrk(optarg, "bkmgBKMG");
	if (!(*suffix))
		die("[Error] Wrong suffix behind -b, only b,k,m or g \n");

	intarg *= suffix2llu(*suffix);

	ret = intarg / DVD_SECTOR_SIZE;
	if (ret < 0)
		ret *= -1;

	return ret;
}

/*
 * get available space on target filesystem
 */

off_t get_free_space(char *path)
{

#ifdef USE_STATFS
	struct statfs fsinfo;
#else
	struct statvfs fsinfo;
#endif
	/*   ssize_t temp1, temp2; */
	long free_blocks, fsblock_size;
	off_t free_bytes;
#ifdef USE_STATFS
	statfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used the linux statfs\n");
#else
	statvfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used statvfs\n");
#endif
	free_blocks = fsinfo.f_bavail;
	/* On Solaris at least, f_bsize is not the actual block size -- lb */
	/* There wasn't this ifdef below, strange! How could the linux statfs
	 * handle this since there seems to be no frsize??! */
#ifdef USE_STATFS
	fsblock_size = fsinfo.f_bsize;
#else
	fsblock_size = fsinfo.f_frsize;
#endif
	free_bytes = ((off_t)free_blocks * (off_t)fsblock_size);
	if (verbosity_level >= 1) {
		printe("[Info] In freespace_getter:for %s : %.0f free\n", path,
			(float)free_bytes);
		printe("[Info] In freespace_getter:bavail %ld * bsize %ld = above\n",
			free_blocks, fsblock_size);
	}

	/*return ( fsinfo.f_bavail * fsinfo.f_bsize ); */
	return free_bytes;
}

/*
 * get used space on dvd (for mirroring)
 */

off_t get_used_space(char *path)
{

#ifdef USE_STATFS
	struct statfs fsinfo;
#else
	struct statvfs fsinfo;
#endif
	/*   ssize_t temp1, temp2; */
	long used_blocks, fsblock_size;
	off_t used_bytes;
#ifdef USE_STATFS
	statfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used the linux statfs\n");
#else
	statvfs(path, &fsinfo);
	if (verbosity_level >= 1)
		printe("[Info] Used statvfs\n");
#endif
	used_blocks = fsinfo.f_blocks;
	/* On Solaris at least, f_bsize is not the actual block size -- lb */
	/* There wasn't this ifdef below, strange! How could the linux statfs
	 * handle this since there seems to be no frsize??! */
#ifdef USE_STATFS
	fsblock_size = fsinfo.f_bsize;
#else
	fsblock_size = fsinfo.f_frsize;
#endif
	used_bytes = ((off_t)used_blocks * (off_t)fsblock_size);
	if (verbosity_level >= 1) {
		printe("[Info] In usedspace_getter:for %s : %.0f used\n", path,
			(float)used_bytes);
		printe("[Info] In usedspace_getter:part1 %ld, part2 %ld\n",
			used_blocks, fsblock_size);
	}
	/*   return ( buf1.f_blocks * buf1.f_bsize ); */
	return used_blocks;
}

/*Checks for read and write access to path*/
bool have_access(char *pathname, bool prompt)
{
	int op;

	if (access(pathname, R_OK | W_OK) < 0) {
		switch (errno) {
		case EACCES:
			printe("[Error] No access to %s\n", pathname);
			return false;
		case ENOENT:
			break;
		default:
			die("[Error] REPORT have_access path %s error: %s\n", pathname, strerror(errno));
		}
	} else if (!overwrite_all_flag && prompt) {
		printe("\n[Error] %s exists....\n", pathname);
		op = get_option("[Hint] Please choose [o]verwrite, "
				"[x]overwrite all, or [q]uit: ", "oxq");
		switch (op) {
		case 'o':
		case 'x':
			overwrite_all_flag = true;
			break;
		case 'q':
		case 's':
			return false;
		default:
			die("[Error] REPORT have_access\n");
		}
	}

	return true;
}

/*Finds a given file in a case insensitive way*/
char *find_listing(char *path, char *name)
{
	DIR *dir;
	struct dirent *entry;
	static char ret[PATH_MAX];

	dir = opendir(path);
	if (!dir)
		return NULL;
	memset(&ret, 0, sizeof(ret));
	while ((entry = readdir(dir))) {
		if (strcasestr(entry->d_name, name)) {
			safestrncpy(ret, entry->d_name, PATH_MAX);
			closedir(dir);
			return ret;
		}
	}

	closedir(dir);
	return NULL;
}

/*
*Rename: move file name from bla.partial to bla or mark as dublicate 
*/

void rename_partial(char *name)
{
	char output[PATH_MAX];

	safestrncpy(output, name, PATH_MAX);
	strncat(output, ".partial", PATH_MAX);

	if (!have_access(name, true) || !have_access(output, false))
		die("[Error] Failed to move \"%s\" to \"%s\"\n", output, name);

	if (rename(output, name) < 0) {
		die("[Error] REPORT: errno %s\n", strerror(errno));
	}
}

/*
* Creates a directory with the given name, checking permissions and reacts accordingly (bails out or asks user)
*/

int makedir(char *name)
{
	if (!have_access(name, false))
		die("[Error] Don't have access to create %s\n", name);

	if (mkdir(name, 0750)) {
		if (errno == EEXIST)
			return 0;
		die("[Error] Failed to create directory: %s\n", name);
	}

	return 0;
}

void redirectlog(char *filename)
{
	if (!have_access(filename, true) && !force_flag)
		die("[Error] Can't overwrite \"%s\" without -f\n");

	if (!freopen(filename, "w+", stderr))
		die("[Error] freopen\n");
}

int open_partial(char *filename)
{
	int fd;
	char output[PATH_MAX];

	memset(output, 0, PATH_MAX);

	if (strstr(filename, ";?")) {
		printe("\n[Hint] File on dvd ends in \";?\" (%s)\n", filename);
		assert(strlen(filename) > 2);
		safestrncpy(output, filename, strlen(filename)-2);
	} else
		safestrncpy(output, filename, PATH_MAX);

	printe("[Info] Writing to %s \n", output);

	if (!have_access(filename, false))
		return -1;

	/*append .partial*/
	strncat(output, ".partial", PATH_MAX);

	if (!have_access(output, true))
		return -1;

	fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		die("\n[Error] Error opening file %s\n" 
		    "[Error] Error: %s\n",
		    filename,
		    strerror(errno));

	return fd;
}


off_t filesizeof(char *path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		die("getfilesize (via stat): %s\n", strerror(errno));

	return buf.st_size;
}

/*Used to access the Video ManaGer data*/
/*according to https://en.wikibooks.org/wiki/Inside_DVD-Video/Directory_Structure
 *	It's kept in following files VIDEO_TS.{IFO,BUP,VOB}
 *	It contains the jump codes to access menus and titles in a given title set.
 *	The vmg_ctx however contains a list of all the titles and their information respectively.
 */

void list_chapters_by_title(ifo_handle_t *vmg_ctx)
{
	/*Title Table SectoR PoinTer*/
	tt_srpt_t* tt_srpt;

	/*grab Title Table SectoR PoinTer*/
	tt_srpt = vmg_ctx->tt_srpt;

	/* chapters = nr_of_ptts (NumbeR OF PoinTer TableS)
	 *
	 * In the libdvdread library and on the physical disk
	 * the size of nr_of_ptts is two ocets or 16bits.
	 */
	uint16_t chapters;

	/* title = nr_of_srpts (NumbeR OF SectoR PoinTeRS)
	 *
	 * The size of the variable nr_of_srpts in the
	 * libdvdread library is exactly two octets or 16bits,
	 * which is also the size on the physical disk.
	 */
	uint16_t title;


	/* nr_of_srpts = NumbeR OF SectoR PoinTeRS*/
	for (title = 0; title < tt_srpt->nr_of_srpts; title++) {
		/*"nr_of_ptts" = NumbeR OF PoinTer TableS*/
		chapters = tt_srpt->title[title].nr_of_ptts;
		/* Most humans do not count starting from zero like most programmers;
		 * thus it is necessary that one must add one, in order to align memory
		 * position with the counting numbers.
		 */
		printe("[Info] Title %hu has %hu chapers\n", title + 1, chapters);
	}
}
















