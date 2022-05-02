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

/*B. Watson*/
#include "vobcopy.h"

extern char *name;
extern time_t starttime;
//extern int verbosity_level;
extern off_t disk_vob_size;
//extern bool overwrite_flag;
//extern bool overwrite_all_flag;
extern int overall_skipped_blocks;

/*=========================================================================*/
/*----------Why exactly does mirroring the disk take so much code?---------*/
/*=========================================================================*/
void mirror(char *dvd_name, char *cwd, off_t pwd_free,
	    bool stdout_flag, char *onefile, char *provided_input_dir, dvd_reader_t *dvd)
{
	struct dirent *directory;
	//struct stat fileinfo;

	int streamout;

	dvd_file_t *dvd_file = NULL;
	unsigned char bufferin[DVD_SECTOR_SIZE];

	/* no dirs behind -1, -2 ... since its all in one dir */
	DIR* dir;
	char video_ts_dir[PATH_MAX];
	char number[8];
	char input_file[PATH_MAX];
	char output_file[PATH_MAX];
	int i, start = 0, title_nr = 0;
	off_t file_size;
	char *name;
	char dvd_path[PATH_MAX];
	char d_name[PATH_MAX];

	memset(&dvd_path, 0, sizeof(dvd_path));
	safestrncpy(dvd_path, cwd, PATH_MAX - 34);
	strncat(dvd_path, dvd_name, 33);

	if (!stdout_flag) {
		makedir(dvd_path);
		strcat(dvd_path, "/VIDEO_TS/");
		makedir(dvd_path);
		printe("[Info] Writing files to this dir: %s\n", dvd_path);
	}

	name = find_listing(provided_input_dir, "VIDEO_TS");
	if (!name) {
		printe("[Error] Could not find VIDEO_TS directory!\n");
		return;
	}
	snprintf(video_ts_dir, PATH_MAX, "%s/%s", provided_input_dir, name);
	dir = opendir(video_ts_dir);
	if (!dir) {
		printe("[Error] Could not open VIDEO_TS directory at %s for reason: %s\n", video_ts_dir, strerror(errno));
		return;
	}

	directory = readdir(dir);	/* thats the . entry */
	directory = readdir(dir);	/* thats the .. entry */
	/* according to the file type (vob, ifo, bup) the file gets copied */
	while ((directory = readdir(dir)) != NULL) {	/*main mirror loop */
		dvd_file = NULL;

		/*in dvd specs it says it must be uppercase VIDEO_TS/VTS...
		   but iso9660 mounted dvd's sometimes have it lowercase */
		safestrncpy(d_name, directory->d_name, PATH_MAX);
		capitalize(d_name, PATH_MAX);
		snprintf(output_file, PATH_MAX, "%s/%s", cwd, d_name);

		if (stdout_flag) /*this writes to stdout */
			streamout = STDOUT_FILENO;	/*in other words: 1, see "man stdout" */
		else {
			streamout = open_partial(output_file);
			if (streamout < 0) {
				printe("[Error] Either didn't have access or couldn't open %s\n", output_file);
				return;
			}
		}

		/* get the size of that file */
		snprintf(input_file, sizeof(input_file), "%s/%s/%s", cwd, video_ts_dir, directory->d_name);
		printe("input_file: %s\n");
		file_size = filesizeof(input_file);

		memset(bufferin, 0, DVD_SECTOR_SIZE * sizeof(unsigned char));

		/*this here gets the title number */
		for (i = 1; i <= 99; i++) {	/*there are 100 titles, but 0 is
						   named video_ts, the others are 
						   vts_number_0.bup */
			sprintf(number, "_%.2i", i);

			if (strstr(directory->d_name, number)) {
				title_nr = i;

				break;	/*number found, is in i now */
			}
			/*no number -> video_ts is the name -> title_nr = 0 */
		}

		/*which file type is it */

		if (strcasestr(directory->d_name, ".IFO") || strcasestr(directory->d_name, ".BUP")) {
			dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_INFO_BACKUP_FILE);

			/*this copies the data to the new file */
			for (i = 0; i * DVD_SECTOR_SIZE < file_size; i++) {
				DVDReadBytes(dvd_file, bufferin, DVD_SECTOR_SIZE);
				if (write(streamout, bufferin, DVD_SECTOR_SIZE) < 0) {
					printe("\n[Error] Error writing to %s \n"
					       "[Error] Error: %s\n",
					       output_file,
					       strerror(errno)
					);
					if (dvd_file)
						DVDCloseFile(dvd_file);
					closedir(dir);
					return;
				}

				/* progress indicator */
				printe("%4.0fkB of %4.0fkB written\r", (i + 1) * (DVD_SECTOR_SIZE / KILO), file_size / KILO);
			}
			fputc('\n', stderr);
			if (!stdout_flag) {
				if (fdatasync(streamout) < 0) {
					printe("\n[Error] error writing to %s \n"
					       "[Error] error: %s\n",
						output_file,
						strerror(errno));
					if (dvd_file)
						DVDCloseFile(dvd_file);
					closedir(dir);
					return;
				}

				close(streamout);
				rename_partial(output_file);
			}
		} else if (strcasestr(directory->d_name, ".VOB")) {
			if (directory->d_name[7] == '0' || title_nr == 0) {
				/*this is vts_xx_0.vob or video_ts.vob, a menu vob */
				dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_MENU_VOBS);
				start = 0;
			} else
				dvd_file = DVDOpenFile(dvd, title_nr, DVD_READ_TITLE_VOBS);

			if (directory->d_name[7] == '1' || directory->d_name[7] == '0')
				/* reset start when at beginning of Title */
				start = 0;
			else if (directory->d_name[7] > '1' && directory->d_name[7] <= '9') {
				int a, subvob;
				off_t culm_single_vob_size = 0;

				subvob = (directory->d_name[7] - '0');

				for (a = 1; a < subvob; a++) {
					if (strstr(input_file, ";?"))
						input_file[strlen(input_file) - 7] = (a + '0');
					else
						input_file[strlen(input_file) - 5] = (a + '0');

					/* input_file[ strlen( input_file ) - 5 ] = ( a + 48 ); */
					if (!have_access(input_file, false)) {
						printe("[Info] Can't stat() %s.\n", input_file);
						if (dvd_file)
							DVDCloseFile(dvd_file);
						closedir(dir);
						return;
					}

					culm_single_vob_size += filesizeof(input_file);
					if (verbosity_level > 1)
						printe("[Info] Vob %d %d (%s) has a size of %lli\n", title_nr, subvob, input_file, filesizeof(input_file));
				}

				start = (culm_single_vob_size / DVD_SECTOR_SIZE);
				/*start = ( ( ( directory->d_name[7] - 49 ) * 512 * KILO ) - ( directory->d_name[7] - 49 ) );  */
				/* this here seeks d_name[7] 
				 *  (which is the 3 in vts_01_3.vob) Gigabyte (which is equivalent to 512 * KILO blocks 
				 *   (a block is 2kb) in the dvd stream in order to reach the 3 in the above example.
				 * NOT! the sizes of the "1GB" files aren't 1GB... 
				 */
			}

			/*this copies the data to the new file */
			if (verbosity_level > 1)
				printe("[Info] Start of %s at %d blocks \n", output_file, start);

			rip_vob_file(dvd_file, start, 0, 10, streamout);

			fputc('\n', stderr);
			if (!stdout_flag) {
				if (fdatasync(streamout) < 0) {
					printe("\n[Error] error writing to %s \n"
					    "[Error] error: %s\n",
					    output_file,
					    strerror(errno)
					);
					if (dvd_file)
						DVDCloseFile(dvd_file);
					closedir(dir);
					return;
				}

				close(streamout);
				rename_partial(output_file);
			}

			if (dvd_file)
				DVDCloseFile(dvd_file);
		}
	}
	closedir(dir);

	if (overall_skipped_blocks > 0)
		printe("[Info] %d blocks had to be skipped, be warned.\n", overall_skipped_blocks);
	/*end of mirror block */
}
