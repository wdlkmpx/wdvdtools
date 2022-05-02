/*  This file is part of vobcopy.
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
 *  You should have received a copy of the GNU General Public License
 *  along with vobcopy; if not,  see <http://www.gnu.org/licenses/>.
 */

#include "vobcopy.h"
#include <dvdread/ifo_read.h>

/* included from cdtitle.c (thx nils *g)
 * get the Title
 * returns 0 on success or a value < 0 on error
 */
#if defined(__sun)
bool get_dvd_name(const char *device, char *title)
{
	/* title is actually in the device name */
	char *new_title;

	new_title = strstr(device, "d0/");
	if (new_title)
		return false;
	new_title += strlen("d0/");

	strncpy(title, new_title, sizeof(title)-1);

	/*Replace the spaces with underscores*/
	strrepl(title, ' ', '_');

	return true;
}
#else /* !defined(__sun) */
bool get_dvd_name(const char *device, char *title)
{
	int fd;
	size_t last;
	int bytes_read;
	off_t sector_offset;
	char file_buf[DVD_SECTOR_SIZE];

	/* open the device */
	if ((fd = open(device, O_RDONLY, 0)) < 0)
		goto read_error;

	/* seek to title of first track, which is at (track_no * 32768) + 40 */
	sector_offset = get_sector_offset(16);
	if (sector_offset != lseek(fd, sector_offset, SEEK_SET))
		goto read_error;

	/* read title */
	if ((bytes_read = read(fd, file_buf, DVD_SECTOR_SIZE)) != DVD_SECTOR_SIZE)
		goto read_error;
	/*The fd is no longer needed*/
	close(fd);

	/*+40 must be the title offset*/
	safestrncpy(title, file_buf+40, 32);
	/*32 = maximum title length*/

	/*Remove any trailing spaces*/
	last = strrchr(title, ' ') - title;

	/*removes trailing spaces*/
	for (;last && (title[last] == ' '); last--)
		title[last] = '\0';

	if (!strnlen(title, 32)) {
		printe("[Hint] The dvd has no name, will choose a nice one ;-), else use -t\n");
		strcpy( title, "insert_name_here\0" );
	}

	/*Replace the spaces with underscores*/
	strrepl(title, ' ', '_');

	return true;

	read_error:
		if (fd > 0)
			close(fd);
		printe("[Error] something wrong in dvd_name getting - please specify path as /cdrom or /dvd (mount point) or use -t\n");
		printe("[Error] only read %d bytes instead of 2048\n", bytes_read);
		printe("[Error] Error: %s\n", strerror(errno));
		return false;
}
#endif /* !defined(__sun) */


/* function to get the device of a given pathname
 * returns 0 if successful and NOT mounted
 *         1 if successful and mounted
 * returns <0 if error
 */
int get_device(char *path, char *device)
{
#if (defined(__linux__))
	struct mntent* lmount_entry;
#elif ( defined( __sun ) )
	FILE  *mnttab_fp;
	char *mnt_special;
	int mntcheck;
	char *new_device;
#endif

#if ( !defined( __sun ) )
	FILE *tmp_streamin;
	char  tmp_bufferin[ PATH_MAX ];
	char  tmp_path[ PATH_MAX ];
	int l = 0;
#endif

	char  *pointer;
	char  *k;
	bool  mounted = false;

#ifdef USE_STATFS_FOR_DEV
	struct statfs buf;
#endif
#ifdef USE_STATVFS_FOR_DEV
	struct statvfs buf;
#endif

	/* the string should have no trailing / */
	if(path[(strlen(path) - 1)] == '/')
		path[(strlen(path) - 1)] = '\0' ;

	/* remove video_ts if given */
	if( (pointer = strstr( path, "/video_ts" )) || 
	    (pointer = strstr( path, "/VIDEO_TS" )) )
		*pointer = '\0';

	/* check if it is given as /dev/xxx already */
	if( strstr( path, "/dev/" ) )
		strcpy( device, path );
	else {
		/* look through /etc/mtab to see if it's actually mounted*/

#if ( defined( USE_STATFS_FOR_DEV ) || defined( USE_STATVFS_FOR_DEV ) ) 
#	ifdef USE_STATFS_FOR_DEV
		if(!statfs(path, &buf))
#	else
		if(!statvfs(path, &buf))
#	endif
		{
			if(!strcmp(path, buf.f_mntonname)) {
				mounted = true;
#	if defined(__FreeBSD__)
				strcpy(device, buf.f_mntfromname);
#	else
				strcpy(device, "/dev/r");
				strcat(device, buf.f_mntfromname + 5);
#	endif
				return mounted;
			}

			strcpy(device, buf.f_mntfromname);
		} else {
			printe("[Error] Error while reading filesystem info");
			return -1;
		}
#elif ( defined( __sun ) )

		int mntcheck;
		struct mnttab mount_entry;

		if ((mnttab_fp = fopen("/etc/mnttab", "r")) == NULL) {
			printe(" [Error] Could not open mnttab for searching!\n");
			printe(" [Error] error: %s\n", strerror(errno));
			return -1;
		}

		while (!(mntcheck = getmntent(mnttab_fp, &mount_entry))) {
			/* check to see if our path is this entry */
			if (!strcmp(path, mount_entry.mnt_mountp)) {
				char *new_device, *mnt_special;
				if ( strstr( mount_entry.mnt_special, "/dsk/" ) == NULL ) {
					printe("[Error] %s doesn't look like a disk device to me", mount_entry.mnt_special);
					return -1;
				}
				/* we actually want the raw device name */

				mnt_special = malloc( strlen( mount_entry.mnt_special ) + 1 );
				new_device = malloc( strlen( mount_entry.mnt_special ) + 2 );
				strcpy( mnt_special, mount_entry.mnt_special );
				strcpy( new_device, mnt_special );
				strcpy( strstr( new_device, "/dsk/" ), "" );
				strcat( new_device, "/rdsk/" );
				strcat( new_device,
				strstr( mnt_special, "/dsk/" ) + strlen( "/dsk/" ) );
				strncpy( device, new_device, sizeof(device)-1 );
				free( mnt_special );
				free( new_device );
				mounted = true;
				break;
			}
		}

		if (mntcheck > 0) {
			printe("[Error] Encountered error in reading mnttab file\n");
			printe("[Error] error: %s\n"), strerror(errno));
			return -1;
		} else if (mntcheck == -1) {
			printe("[Error] Did not find mount %s in mnttab!\n", path );
			return -1;
		}
	}
#else
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * this is the code for the other-OSs, not solaris
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#	if (defined(__linux__)) 
		if ((tmp_streamin = setmntent("/etc/mtab", "r"))) {
			while ((lmount_entry = getmntent(tmp_streamin))) {
				if (!strcmp(lmount_entry->mnt_dir, path)){
					/* Found the mount point */
					printe("[Info] Device %s mounted on %s\n", lmount_entry->mnt_fsname, lmount_entry->mnt_dir);
					strcpy(device, lmount_entry->mnt_fsname);
					mounted = true;
					break;
				}
			}

			endmntent(tmp_streamin);
			if (mounted)
				/* device was set from /etc/mtab, no need to further check
				 * /etc/fstab */
				return mounted;
		}
#	endif


		if ((tmp_streamin = fopen("/etc/mtab", "r"))) {
			printe("[Error] Could not read /etc/mtab!\n");
			printe("[Error] error: %s\n", strerror(errno));
			return -1;
		}

		strcpy( tmp_path, path );
		strcat( tmp_path, " " ); /* otherwise it would detect that e.g. 
					  * /cdrom is mounted even if only/cdrom1 is 
					  * mounted
					  */

		memset(tmp_bufferin, 0, sizeof(tmp_bufferin));
		while(fgets( tmp_bufferin, PATH_MAX, tmp_streamin)) {
			if(strstr(tmp_bufferin, tmp_path))
				mounted = true;
			fclose(tmp_streamin);
		}

		/*read the device out of /etc/fstab*/
		if (!(tmp_streamin = fopen( "/etc/fstab", "r" ))) {
			printe("[Error] Could not read /etc/fstab!");
			printe("[Error] error: %s\n", strerror(errno));
			device[0] = '\0';
			return -1;
		}

		strcpy(tmp_path, path);

		memset(tmp_bufferin, 0, sizeof(tmp_bufferin));

		while(fgets( tmp_bufferin, PATH_MAX, tmp_streamin )) {
			if ((pointer = strstr( tmp_bufferin, tmp_path))) {
				if (isgraph((int) * (pointer + strlen(tmp_path))))
					break; /* there is something behind the path name, 
						* for instance like it would find /cdrom but 
						* also /cdrom1 since /cdrom is a subset of /cdrom1
						*/
						/* isblank should work too but how do you do that with 
						* the "gnu extension"? (man isblank)
						*/

				if ((k = strstr(tmp_bufferin, "/dev/")) == NULL ) {
					printe("[Error] Weird, no /dev/ entry found in the line where iso9660 or udf gets mentioned in /etc/fstab\n");
					return -1;
				}

				for (l = 0; isgraph((int) *(k)); k++, l++) {
					device[l] = *(k);
					if( device[l] == ',' )
						break;
				}

				if(isdigit((int) device[l-1])) {
					/*Is this supposed to check for hd?*/
					if(strstr(device, "hd"))
						printe("[Hint] Hmm, the last char in the device path (%s) that gets mounted to %s is a number.\n",
							device,
							path);
				}

				device[l] = '\0';
			}

			memset(tmp_bufferin, 0, sizeof(tmp_bufferin));
		}

		fclose(tmp_streamin);
		if(!strstr( device, "/dev")) {
			printe("[Error] Could not find the provided path (%s), typo?\n",path);
			device[0] = '\0';
			return -1;
		}
	}
#endif
	return mounted;
}


/* function to get the device WITHOUT a given pathname
 * int get_device_on_your_own( char *path, char *device );
 * therefore called oyo - on your own
 * 	returns 0 if successful and NOT mounted
 *      	1 if successful and mounted
 *	returns <0 if error
 */

#ifdef USE_GETMNTINFO
int get_device_on_your_own( char *path, char *device )
{
	int i, n, dvd_count = 0;
#	ifdef GETMNTINFO_USES_STATFS
		struct statfs *mntbuf;
#	else
		struct statvfs *mntbuf;
#	endif

	if((n = getmntinfo(&mntbuf, 0 )) > 0) {
		for(i = 0; i < n; i++) {
			if( strstr( mntbuf[i].f_fstypename, "cd9660" ) || strstr( mntbuf[i].f_fstypename, "udf" ) ) {
				dvd_count++;
				strcpy( path, mntbuf[i].f_mntonname );
#				if defined(__FreeBSD__) && (__FreeBSD_Version > 500000)
					strcat(device, mntbuf[i].f_mntfromname);
#				else
					strcpy(device, "/dev/r");
					strcat(device, mntbuf[i].f_mntfromname + 5);
#				endif
			}
		}

		/* no cd found? Then user should mount it */
		if(dvd_count == 0) {
			printe("[Error] There seems to be no cd/dvd mounted. Please do that..\n");
			return -1;
		}
	} else {
		printe("[Error] An error occured while getting mounted file system information\n");
		return -1;
	}

	return dvd_count;
}

#elif ( defined( __sun ) )
int get_device_on_your_own( char *path, char *device )
{
	/* Completely rewritten search of a mounted DVD on Solaris, much
	cleaner. No more parsing of strings -- lb */

	/* Number of found entries */
	int dvd_count = 0;
	/* Result of the mount search */
	int result    = 0;
	/* the mnttab file */
	FILE *mnttab_fp;
	/* The mount entry found */
	struct mnttab mountEntry;
	/* The mount search fields */
	struct mnttab udfsMount;


	/* We're looking for any udfs-mounted FS
	Note that this is not always a DVD */
	udfsMount.mnt_mountp  = NULL;
	udfsMount.mnt_special = NULL;
	udfsMount.mnt_fstype  = "udfs";
	udfsMount.mnt_mntopts = NULL;
	udfsMount.mnt_time    = NULL;

	/* Try to open the mnttab info */
	if ((mnttab_fp = fopen("/etc/mnttab", "r")) == NULL) {
		printe("[Error] Could not open mnttab for searching!\n");
		printe("[Error] error: %s\n", strerror(errno));
		return -1;
	}

	/* Get the matching mount points */
	/* If there is a match, handle it */
	while ((result = getmntany(mnttab_fp, &mountEntry, &udfsMount)) == 0) {
		dvd_count++;
	}

	/* If the last result is not -1, there was a problem -- exit */
	if (result != -1)
		return -1;
   
	/* no cd found? Then user should mount it */
	if (dvd_count == 0) {
		printe("[Error] There seems to be no cd/dvd mounted. Please do that..\n");
		return -1;
	}

	/* Copy the device name and mount points to the output variables */
	strcpy(path,   mountEntry.mnt_mountp);
	strcpy(device, mountEntry.mnt_special);

	return dvd_count;
}

#else /* End ifdef(__sun) */
int get_device_on_your_own( char *path, char *device )
{
	FILE *tmp_streamin;
	char  tmp_bufferin[PATH_MAX];
/*	char  tmp_path[20];*/
	int l         = 0;
	int dvd_count = 0;
	char *k;
	/* read the device out of /etc/mtab */

	if (!(tmp_streamin = fopen("/etc/mtab", "r"))) {
		printe("[Error] Could not read /etc/mtab!");
		printe("[Error] error: %s\n", strerror(errno));
		return -1;
	}

	/* strcpy(tmp_path, "iso9660"); */
	memset(tmp_bufferin, 0, sizeof(tmp_bufferin));
	while(fgets( tmp_bufferin, PATH_MAX, tmp_streamin))  {
		/*if(strstr( tmp_bufferin, tmp_path)) */
		if (strstr( tmp_bufferin, "iso9660") ||
		    strstr( tmp_bufferin, "udf")     ||
		    strstr( tmp_bufferin, "cdrom")   ||
		    strstr( tmp_bufferin, "dvd")) {
			dvd_count++; /*count every cd we find */
			/* extract the device */

			if( ( k = strstr( tmp_bufferin, "/dev/" ) ) == NULL ) {
				printe("[Error] Weird, no /dev/ entry found in the line where iso9660, udf or cdrom gets mentioned in /etc/mtab\n");
				dvd_count--;
				continue;
			}

			while(isgraph( (int) *(k) )) {
				device[l] = *(k);
				if( device[l] == ',' )
					break;
				l++;
				k++;
			}

			device[l] = '\0';
			if (isdigit((int)device[l-1])) {
				if(strstr( device, "hd" ))
					printe("[Hint] Hmm, the last char in the device path %s is a number.\n", device );
			}

			/*The syntax of /etc/fstab and mtab seems to be something like this:
			 * Either 
			 *	/dev/hdc /cdrom
			 * or, in case of supermount
			 * none /mnt/cdrom supermount ro,dev=/dev/hdc,
			 * Therefore I parse for /dev/ first and take everything up to a blank for the device
			 * and then I locate the first space in this line and after that comes the mount-point
			*/

			k = strstr( tmp_bufferin, " " );
			/*traverse the gap*/

			if( isgraph( (int) *(k) ))
				k++;
			while(!(isgraph( (int) *(k) ))) 
				k++;

			/* extract the path the device has been mounted to */
			l=0;

			while(isgraph( (int) *(k) )) {
				/* replace escaped ASCII space ... */
				if (!strncmp(k, "\\040", 4)) {
                                                  path[l++] = ' '; /* ... with literal space */
                                                  k+=4;
				} else {
					path[l] = *(k);
					k++;
					l++;
				}
			}

			path[l] = '\0';
		}

		memset(tmp_bufferin, 0, sizeof(tmp_bufferin));
		l = 0; /* for the next run
			* we take the last entry in /etc/mtab since that has been 
			* mounted the last and we assume that this is the
			* dvd. 
			*/
	}

	fclose( tmp_streamin );

	if (!dvd_count) {
		printe("[Error] There seems to be no cd/dvd mounted. Please do that..\n");
		return -1;
	}

	return dvd_count;
}
#endif



/******************* get the whole vob size via stat() ******************/
/*Case isn't always consistent, which is why a lot in this function doesn't look like it's needed but actually is.*/
off_t get_vob_size(int title, char *dvd_path)
{
	char vob_name[PATH_MAX];
	char vob_path[PATH_MAX];
	char stat_path[PATH_MAX];

	char *name;
	int vob_part;
	off_t  vob_size;

	name = find_listing(dvd_path, "VIDEO_TS");
	if (!name)
		die("[Error] Couldn't find listing with dvd_path \"%s\"!\n", dvd_path);
	snprintf(vob_path, PATH_MAX, "%s/%s/", dvd_path, name);

	vob_size = 0;
	/*0 is where vob parts start*/
	/*9 is the highest part number for a vob*/
	for (vob_part = 1; vob_part < 10 ;vob_part++) {
		snprintf(vob_name, PATH_MAX, "VTS_%.2d_%d.VOB", title, vob_part);
		if (!(name = find_listing(vob_path, vob_name)))
			break;

		/*Now stat the result*/
		snprintf(stat_path, PATH_MAX, "%s/%s", vob_path, name);
		vob_size += filesizeof(stat_path);
	}

	return vob_size;
}

/*returns sectors written*/
size_t rip_vob_file(dvd_file_t *dvd_file, unsigned int start_sector, unsigned int sectors, unsigned int retries, int outfd)
{
	size_t sector;
	ssize_t written;
	time_t starttime;
	int total_sectors;
	unsigned int tries;
	size_t skipped_sectors;
	unsigned char buffer[DVD_SECTOR_SIZE];

	if (sectors)
		total_sectors  = sectors;
	else
		total_sectors = DVDFileSize(dvd_file);

	skipped_sectors = 0;
	starttime       = time(NULL);
	memset(&buffer, 0, sizeof(buffer));
	/*!sectors translates to, continue the loop till the end.*/
	for (sector = start_sector; (sector < total_sectors); sector++) {
		tries = 0;

		/*dvd_file, current_sector, sectors at once, buffer*/
		while ((DVDReadBlocks(dvd_file, sector, 1, buffer) < 1) && (tries < retries))
			tries++;

		if (tries == retries) {
			skipped_sectors++;
			printe("\n[Error] Skipped sector %d\n", sector);
			continue;
		}

		written = write(outfd, buffer, DVD_SECTOR_SIZE);
		if (written < 0) {
			printe("\n[Error] write failed: %s\n", strerror(errno));
			break;
		} else if (written != DVD_SECTOR_SIZE) {
			printe("\n[Error] Failed to write the full sector because: %s\n", strerror(errno));
			break;
		}

		/*starttime, current sector relative to last, last sector, force printing of status bar*/
		progressUpdate(starttime, (sector - start_sector), total_sectors, false);
	}

	/*Calculate the actual amount of data wrote*/
	return (sectors - skipped_sectors);
}

/*dvdtime2msec and get_longest_title are copy-paste'd from lsdvd*/

long dvdtime2msec(dvd_time_t *dt)
{
	long   ms;
	/*PAL and NTSC frame rates are spelled out here for framerate detection*/
	double frames_per_s[4] = {-1.0, 25.00, -1.0, 29.97};
	double fps = frames_per_s[(dt->frame_u & 0xc0) >> 6];

	/*What are all of these magic numbers are for?*/
	ms  = (((dt->hour &   0xf0) >> 3) * 5 + (dt->hour   & 0x0f)) * 3600000;
	ms += (((dt->minute & 0xf0) >> 3) * 5 + (dt->minute & 0x0f)) * 60000;
	ms += (((dt->second & 0xf0) >> 3) * 5 + (dt->second & 0x0f)) * 1000;

	if(fps > 0)
		ms += ((dt->frame_u & 0x30) >> 3) * 5 + (dt->frame_u & 0x0f) * 1000.0 / fps;

	return ms;
}

/*It gets the ID for the title with the longest time*/
int get_longest_title(dvd_reader_t *dvd)
{
	int i;
	long longest_time;
	int longest_title;

	/*intermediary variables for readability*/
	pgc_t *pgc;
	int vts_ttn;
	int title_count;
	int title_set_nr;
	long current_time;
	ifo_handle_t *ifo;
	int pgci_srp_offset;
	ifo_handle_t *master_ifo;

	master_ifo = ifoOpen(dvd, 0);
	if (!master_ifo) {
		printe("Can't open main ifo!\n");
		return -3;
	}

	longest_time  = 0;
	longest_title = 0;
	title_count   = master_ifo->tt_srpt->nr_of_srpts;
	for (i = 0; i < title_count; i++) {
		title_set_nr = master_ifo->tt_srpt->title[i].title_set_nr;

		ifo = ifoOpen(dvd, title_set_nr);
		if (!ifo) {
			ifoClose(master_ifo);
			printe("Can't open ifo %d!\n", i);
			return -4;
		}
		/*GENERAL*/
		if (ifo->vtsi_mat) {
			vts_ttn         = master_ifo->tt_srpt->title[i].vts_ttn;
			pgci_srp_offset = ifo->vts_ptt_srpt->title[vts_ttn - 1].ptt[0].pgcn - 1;
			pgc             = ifo->vts_pgcit->pgci_srp[pgci_srp_offset].pgc;

			current_time = dvdtime2msec(&pgc->playback_time);
			if (current_time > longest_time) {
				longest_time  = current_time;
				longest_title = i+1;
			}
		}
		ifoClose(ifo);
	}

	ifoClose(master_ifo);
	return longest_title;
}






