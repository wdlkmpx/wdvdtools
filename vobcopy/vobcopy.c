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

/* CONTRIBUTORS (direct or through source "borrowing")
 * rosenauer@users.sf.net - helped me a lot! 
 * Billy Biggs <vektor@dumbterm.net> - took some of his play_title.c code 
 * and implemeted it here 
 * Håkan Hjort <d95hjort@dtek.chalmers.se> and Billy Biggs - libdvdread
 * Stephen Birch <sgbirch@imsmail.org> - debian packaging
 */

/*TODO:
 * mnttab reading is "wrong" on solaris
 * - error handling with the errno and strerror function
 * with -t still numbers get added to the name, -T with no numbers or so.
 */

/*THOUGHT-ABOUT FEATURES
 * - being able to specify
     - which title
     - which chapter
     - which angle 
     - which language(s)
     - which subtitle(s)
     to copy
 * - print the total playing time of that title
 */

/*If you find a bug, contact me at robos@muon.de*/

#include "vobcopy.h"
#include <math.h>

char *name;
time_t starttime;
bool force_flag = false;
off_t disk_vob_size = 0;
int verbosity_level = 0;
bool overwrite_flag = false;
bool overwrite_all_flag = false;
int overall_skipped_blocks = 0;

/* --------------------------------------------------------------------------*/
/* MAIN */
/* --------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, optopt;

	int op, i, num_of_files, partcount;
	int streamout;

	unsigned char *bufferin = NULL;
	long long unsigned int end_sector   = 0;
	long long unsigned int start_sector = 0;

	char *pwd                   = NULL;
	char *onefile               = NULL;
	char *dvd_path              = NULL;
	char *vobcopy_call          = NULL;
	char *logfile_path          = NULL;
	char *provided_input_dir    = NULL;
	char *provided_output_dir   = NULL;
	char **alternate_output_dir = NULL;

	char dvd_name[35];
	char provided_dvd_name[35];

	dvd_reader_t *dvd      = NULL;
	dvd_file_t   *dvd_file = NULL;

	int dvd_count           = 0;
	int paths_taken         = 0;
	int options_char        = 0;
	int watchdog_minutes    = 0;
	int alternate_dir_count = 0;

	off_t offset                        = 0;
	off_t pwd_free                      = 0;
	off_t vob_size                      = 0;
	off_t free_space                    = 0;
	ssize_t file_size_in_blocks         = 0;

	/*Boolean flags*/
	bool mounted                  = false;
	bool info_flag                = false;
	bool quiet_flag               = false;
	bool stdout_flag              = false;
	bool mirror_flag              = false;
	bool verbose_flag             = false;
	bool onefile_flag             = false;
	bool titleid_flag             = false;
	bool longest_title_flag       = true;	/*default behavior*/
	bool provided_dvd_name_flag   = false;
	bool provided_input_dir_flag  = false;
	bool provided_output_dir_flag = false;

	/*########################################################################
	 * The new part taken from play-title.c
	 *########################################################################*/
	int chapid        = 0;
	int titleid       = 0;
	int angle         = 0;
	int sum_angles    = 0;
	int sum_chapters  = 0;
	int most_chapters = 0;

	/*Title Table SectoR PoinTer*/
	tt_srpt_t    *tt_srpt  = NULL;
	/*ifo file handle*/
	ifo_handle_t *vmg_file = NULL;

	/*Zero buffers*/
	memset(dvd_name, 0, sizeof(dvd_name));
	memset(provided_dvd_name, 0, sizeof(provided_dvd_name));

	/*Allocating buffers*/
	pwd                 = palloc(sizeof(char), PATH_MAX);
	name                = palloc(sizeof(char), PATH_MAX);
	onefile             = palloc(sizeof(char), PATH_MAX);
	dvd_path            = palloc(sizeof(char), PATH_MAX);
	vobcopy_call        = palloc(sizeof(char), PATH_MAX);
	logfile_path        = palloc(sizeof(char), PATH_MAX);
	provided_input_dir  = palloc(sizeof(char), PATH_MAX);
	provided_output_dir = palloc(sizeof(char), PATH_MAX);
	bufferin            = palloc(sizeof(unsigned char), (DVD_SECTOR_SIZE));

	alternate_output_dir = palloc(sizeof(char*), PATH_MAX);
	for (i = 0; i < 4; i++)
		alternate_output_dir[i] = palloc(sizeof(char), PATH_MAX);
	

	/**
	 *getopt-long
	 */
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;
	static struct option long_options[] = {
		{"angle", 1, 0, 'a'},
		{"begin", 1, 0, 'b'},
		{"chapter", 1, 0, 'c'},
		{"end", 1, 0, 'e'},
		{"force", 0, 0, 'f'},
		{"fast", 1, 0, 'F'},
		{"help", 0, 0, 'h'},
		{"input-dir", 1, 0, 'i'},
		{"info", 0, 0, 'I'},
		{"longest", 0, 0, 'M'},
		{"mirror", 0, 0, 'm'},
		{"title-number", 1, 0, 'n'},
		{"output-dir", 1, 0, 'o'},
		{"quiet", 0, 0, 'q'},
		{"onefile", 1, 0, 'O'},
		{"name", 1, 0, 't'},
		{"verbose", 0, 0, 'v'},
		{"version", 0, 0, 'V'},
		{"watchdog", 1, 0, 'w'},
		{"overwrite-all", 0, 0, 'x'},
		{0, 0, 0, 0}
	};
#endif

	/*for gettext - i18n */
#if defined( __gettext__ )
	setlocale(LC_ALL, "");
	textdomain("vobcopy");
	bindtextdomain("vobcopy", "/usr/share/locale");
#endif

	/*
	 * the getopt part (getting the options from command line)
	 */
	while (1) {
#ifdef HAVE_GETOPT_LONG
		options_char = getopt_long(argc, argv,
					   "a:b:c:e:i:n:o:qO:t:vfF:mMhL:Vw:Ix",
					   long_options, &option_index);
#else
		options_char = getopt(argc, argv,
				      "a:b:c:e:i:n:o:qO:t:vfF:mMhL:Vw:Ix-");
#endif

		if (options_char == -1)
			break;

		switch (options_char) {

		case 'a':	/*angle */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -a has to be a number! \n");

			sscanf(optarg, "%i", &angle);
			angle--;	/*in the ifo they start at zero */
			if (angle < 0)
				die("[Hint] Um, you set angle to 0, try 1 instead ;-)\n");
			break;

		case 'b':	/*size to skip from the beginning (beginning-offset) */
			printe("\"-b\" is currently not supported\n");
			start_sector = opt2llu(optarg, 'b');
			break;

		case 'c':	/*chapter *//*NOT WORKING!!*/
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -c has to be a number! \n");

			chapid = atoi(optarg);
			if (chapid < 1)
				die("Not a valid chapter number");

			chapid--;	/*in the ifo they start at zero */
			break;

		case 'e':	/*size to stop from the end (end-offset) */
			printe("\"-e\" is currently not supported\n");
			end_sector = opt2llu(optarg, 'e');
			break;

		case 'f':	/*force flag, some options like -o, -1..-4 set this themselves */
			force_flag = true;
			break;

		case 'h':	/* good 'ol help */
			usage(argv[0]);
			goto end;
			break;

		case 'i':	/*input dir, if the automatic needs to be overridden */
			printe("[Hint] You use -i. Normally this is not necessary, vobcopy finds the input dir by itself." 
			       "This option is only there if vobcopy makes trouble.\n");
			printe("[Hint] If vobcopy makes trouble, please mail me so that I can fix this (robos@muon.de). Thanks\n");

			safestrncpy(provided_input_dir, optarg, PATH_MAX);
			if (strstr(provided_input_dir, "/dev")) {
				printe("[Error] Please don't use -i /dev/something in this version, only the next version will support this again.\n");
				printe("[Hint] Please use the mount point instead (/cdrom, /dvd, /mnt/dvd or something)\n");
			}
			provided_input_dir_flag = true;
			break;

		case 'm':	/*mirrors the dvd to harddrive completly */
			mirror_flag = true;
			info_flag = true;
			break;

		case 'M':	/*Main track - i.e. the longest track on the dvd */
			titleid_flag = true;
			longest_title_flag = true;
			break;

		case 'n':	/*title number */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -n has to be a number! \n");

			longest_title_flag = false;
			sscanf(optarg, "%i", &titleid);
			titleid_flag = true;
			break;

		case 'o':	/*output destination */
			safestrncpy(provided_output_dir, optarg, PATH_MAX);

			if (!strcasecmp(provided_output_dir, "stdout") || !strcasecmp(provided_output_dir, "-")) {
				stdout_flag = true;
				force_flag = true;
			} else {
				provided_output_dir_flag = true;
				alternate_dir_count++;
			}
			/*      force_flag = true; */
			break;

		case 'q':	/*quiet flag* - meaning no progress and such output */
			quiet_flag = true;
			break;

		case '1':	/*alternate output destination */
		case '2':
		case '3':
		case '4':
			if (alternate_dir_count < options_char - 48)
				die("[Error] Please specify output dirs in this order: -o -1 -2 -3 -4 \n");

			if (isdigit((int)*optarg))
				printe("[Hint] Erm, the number comes behind -n ... \n");

			safestrncpy(alternate_output_dir[options_char - 49], optarg, sizeof(alternate_output_dir[options_char - 49]));
			provided_output_dir_flag = true;
			alternate_dir_count++;
			force_flag = true;
			break;

		case 't':	/*provided title instead of the one from dvd,
				   maybe even stdout output */
			if (strlen(optarg) > 33)
				printf("[Hint] The max title-name length is 33, the remainder got discarded");
			safestrncpy(provided_dvd_name, optarg, sizeof(provided_dvd_name));
			provided_dvd_name_flag = true;

			strrepl(provided_dvd_name, ' ', '_');
			break;

		case 'v':	/*verbosity level, can be called multiple times */
			verbose_flag = true;
			verbosity_level++;
			break;

		case 'w':	/*sets a watchdog timer to cap the amount of time spent
				   grunging about on a heavily protected disc */
			if (!isdigit((int)*optarg))
				die("[Error] The thing behind -w has to be a number!\n");
			sscanf(optarg, "%i", &watchdog_minutes);
			if (watchdog_minutes < 1) {
				printe("[Hint] Negative minutes aren't allowed - disabling watchdog.\n");
				watchdog_minutes = 0;
			}
			break;

		case 'x':	/*overwrite all existing files without (further) question */
			overwrite_all_flag = true;
			break;

		case 'I':	/*info, doesn't do anything, but simply prints some infos
				   ( about the dvd ) */
			info_flag = true;
			break;

		case 'L':	/*logfile-path (in case your system crashes every time you
				   call vobcopy (and since the normal logs get written to 
				   /tmp and that gets cleared at every reboot... ) */
			safestrncpy(logfile_path, optarg, sizeof(logfile_path) - 1); /* -s so room for '/' */
			strcat(logfile_path, "/");
			logfile_path[sizeof(logfile_path) - 1] = '\0';

			verbose_flag = true;
			verbosity_level = 2;
			break;

		case 'O':	/*only one file will get copied */
			onefile_flag = true;
			safestrncpy(onefile, optarg, PATH_MAX);

			for (i = 0; onefile[i]; i++) {
				onefile[i] = toupper(onefile[i]);

				if (onefile[i - 1] == ',')
					onefile[i] = 0;
				else {
					onefile[i] = ',';
					onefile[i + 1] = 0;
				}
			}

			mirror_flag = true;
			break;
		case 'V':	/*version number output */
			fprintf(stderr, "Vobcopy " VERSION " - GPL Copyright (c) 2001 - 2009 robos@muon.de\n");
			goto end;
			break;

		case '?':	/*probably never gets here, the others should catch it */
			printe("[Error] Wrong option.\n");
			usage(argv[0]);
			goto end;
			break;

#ifndef HAVE_GETOPT_LONG
		case '-':	/* no getopt, complain */
			printe("[Error] %s was compiled without support for long options.\n", argv[0]);
			usage(argv[0]);
			goto end;
			break;
#endif

		default: /*probably never gets here, the others should catch it */
			printe("[Error] Wrong option.\n");
			usage(argv[0]);
			goto end;
		}
	} /*End of optopt while loop*/

	printe("Vobcopy " VERSION " - GPL Copyright (c) 2001 - 2009 robos@muon.de\n");
	printe("[Hint] All lines starting with \"libdvdread:\" are not from vobcopy but from the libdvdread-library\n");

	if (quiet_flag) {
		char tmp_path[PATH_MAX];

		safestrncpy(tmp_path, pwd, PATH_MAX - strlen(QUIET_LOG_FILE));
		strcat(tmp_path, QUIET_LOG_FILE);
		printe("[Hint] Quiet mode - All messages will now end up in %s\n", tmp_path);
		redirectlog(tmp_path);
	}

	if (verbosity_level > 1) {	/* this here starts writing the logfile */
		printe("[Info] High level of verbosity\n");

		if (strlen(logfile_path) < 3)
			strcpy(logfile_path, pwd);
		/*This concatenates "vobcopy_", the version, and then ".log"*/
		strcat(logfile_path, "vobcopy_" VERSION ".log");

		redirectlog(logfile_path);

		strcpy(vobcopy_call, argv[0]);
		for (i = 1; i != argc; i++) {
			strcat(vobcopy_call, " ");
			strcat(vobcopy_call, argv[i]);
		}

		printe("--------------------------------------------------------------------------------\n");
		printe("[Info] Called: %s\n", vobcopy_call);
	}

	/*sanity check: -m and -O do not go together*/
	if (mirror_flag && onefile_flag) {
		printe("\n[Error] -m and -O can not be used together.\n");
		goto end;
	}

	/*sanity check: -m and -n are mutually exclusive... */
	if (titleid_flag && mirror_flag) {
		printe("\n[Error] There can be only one: either -m or -n...\n");
		goto end;
	}

	/*
	 * Check if the provided path is too long
	 */
	if (optind < argc) {	/* there is still the path as in vobcopy-0.2.0 */
		provided_input_dir_flag = true;
		if (strlen(argv[optind]) >= PATH_MAX) {
			/*Seriously? "_Bloody_ path"?*/
			printe("\n[Error] Bloody path to long '%s'\n", argv[optind]);
			goto end;
		}
		safestrncpy(provided_input_dir, argv[optind], PATH_MAX);
	}

	if (provided_input_dir_flag) {	/*the path has been given to us */
		int result;
		/* 'mounted' is an enum, it should not get assigned an int -- lb */
		if ((result = get_device(provided_input_dir, dvd_path)) < 0) {
			printe("[Error] Could not get the device which belongs to the given path!\n");
			printe("[Hint] Will try to open it as a directory/image file\n");
			/*exit(1);*/
		}
		if (result == 0)
			mounted = false;
		if (result == 1)
			mounted = true;
	} else {/*need to get the path and device ourselves ( oyo = on your own ) */
		if ((dvd_count = get_device_on_your_own(provided_input_dir, dvd_path)) <= 0) {
			printe("[Warning] Could not get the device and path! Maybe not mounted the dvd?\n");
			printe("[Hint] Will try to open it as a directory/image file\n");
			goto end;
		}
		if (dvd_count > 0)
			mounted = true;
		else
			mounted = false;
	}

	if (!mounted) {
		if (!provided_input_dir_flag) {
			printe("[REPORT] This is simply not possible!\n");
			goto end;
		}
		/*see if the path given is a iso file or a VIDEO_TS dir */
		safestrncpy(dvd_path, provided_input_dir, PATH_MAX);
	}

	/*
	 * Is the path correct
	 */
	printe("\n[Info] Path to dvd: %s\n", dvd_path);

	if (!(dvd = DVDOpen(dvd_path))) {
		printe("\n[Error] Path thingy didn't work '%s'\n", dvd_path);
		printe("[Error] Try something like -i /cdrom, /dvd  or /mnt/dvd \n");
		if (dvd_count > 1)
			printe("[Hint] By the way, you have %i cdroms|dvds mounted, that probably caused the problem\n", dvd_count);
		goto end;
	}

	/*Get space used*/
	disk_vob_size = get_used_space(dvd_path);

	/*
	 * this here gets the dvd name
	 */
	if (provided_dvd_name_flag) {
		printe("\n[Info] Your name for the dvd: %s\n", provided_dvd_name);
		safestrncpy(dvd_name, provided_dvd_name, sizeof(dvd_name));
	}
	else /*Error message is printed out by the function.*/
		if (!get_dvd_name(dvd_path, dvd_name))
			goto end;

	printe("[Info] Name of the dvd: %s\n", dvd_name);

	if (provided_output_dir_flag)
		strcpy(pwd, provided_output_dir);
	else {
		if (!getcwd(pwd, PATH_MAX)) {
			printe("\n[Error] getcwd failed with: %s\n", strerror(errno));
			goto end;
		}

		if (!stdout_flag) {
			/*before adding more junk to the end of pwd, one must add a forward slash.*/
			add_end_slash(pwd);

			/*Adds the dvd title to the working directory*/
			strncat(pwd, dvd_name, PATH_MAX - strnlen(pwd, PATH_MAX));

			/*Make sure the directory exists*/
			makedir(pwd);
		}
	}
	/*Before using path to the working directoy, one must add a forward to avoid mangling the path*/
	add_end_slash(pwd);

	/*########################################################################
	 * The new part taken from play-title.c
	 *########################################################################*/

	/**
	 * Load the video manager to find out the information about the titles on
	 * this disc.
	 */
	vmg_file = ifoOpen(dvd, 0);
	if (!vmg_file) {
		printe("[Error] Can't open VMG info.\n");
		goto end;
	}
	tt_srpt = vmg_file->tt_srpt;

	/**
	 * Get the title with the most chapters since this is probably the main part
	 */
	for (i = 0; i < tt_srpt->nr_of_srpts; i++) {
		sum_chapters += tt_srpt->title[i].nr_of_ptts;

		if (i > 0 && (tt_srpt->title[i].nr_of_ptts > tt_srpt->title[most_chapters].nr_of_ptts))
			most_chapters = i;
	}

	if (longest_title_flag) /*no title specified (-n ) */
		titleid = get_longest_title(dvd);

	if (!titleid_flag) /*no title specified (-n ) */
		titleid = most_chapters + 1;	/*they start counting with 0, I with 1... */

	/**
	 * Make sure our title number is valid.
	 */
	printe("[Info] There are %d titles on this DVD.\n", tt_srpt->nr_of_srpts);
	if (titleid <= 0 || (titleid - 1) >= tt_srpt->nr_of_srpts) {
		printe("[Error] Invalid title %d.\n", titleid);
		goto end;
	}

	/**
	 * Make sure the chapter number is valid for this title.
	 */

	printe("[Info] There are %i chapters on the dvd.\n", sum_chapters);
	printe("[Info] Most chapters has title %i with %d chapters.\n", (most_chapters + 1), tt_srpt->title[most_chapters].nr_of_ptts);

	if (info_flag) {
		printe("[Info] All titles:\n");
		for (i = 0; i < tt_srpt->nr_of_srpts; i++) {
			int chapters = tt_srpt->title[i].nr_of_ptts;
			printe("[Info] Title %i has %d chapter(s).\n", (i + 1), chapters);
		}
	}

	if (chapid < 0 || chapid >= tt_srpt->title[titleid - 1].nr_of_ptts) {
		printe("[Error] Invalid chapter %d\n", chapid + 1);
		goto end;
	}

	/**
	 * Make sure the angle number is valid for this title.
	 */
	for (i = 0; i < tt_srpt->nr_of_srpts; i++)
		sum_angles += tt_srpt->title[i].nr_of_angles;

	printe("\n[Info] There are %d angles on this dvd.\n", sum_angles);
	if (angle < 0 || angle >= tt_srpt->title[titleid - 1].nr_of_angles) {
		printe("[Error] Invalid angle %d\n", angle + 1);
		goto end;
	}

	if (info_flag) {
		printe("[Info] All titles:\n");
		for (i = 0; i < tt_srpt->nr_of_srpts; i++) {
			int angles = tt_srpt->title[i].nr_of_angles;

			printe("[Info] Title %i has %d angle(s).\n", (i + 1), angles);
		}

		disk_vob_size = get_used_space(provided_input_dir);
	}

	/*
	 * get the whole vob size via stat( ) manually
	 */
	if (mirror_flag) {	/* this gets the size of the whole dvd */
		add_end_slash(provided_input_dir);
		disk_vob_size = get_used_space(provided_input_dir);
	} else if (mounted && !mirror_flag) {
		printe("[Info] Checking title %d\n", titleid);
		vob_size = get_vob_size(titleid, provided_input_dir) - (get_sector_offset(start_sector) + get_sector_offset(end_sector));

		if ((!vob_size) || (vob_size > (9*GIGA))) {
			printe("\n[Error] Something went wrong during the size detection of the");
			printe("\n[Error] vobs, size check at the end won't work (probably), but I continue anyway\n\n");
			vob_size = 0;
		}
	}

	/*
	 * check if on the target directory is enough space free
	 * see man statfs for more info
	 */

	pwd_free = get_free_space(pwd);

	set_signal_handlers();

	if (watchdog_minutes) {
		printe("\n[Info] Setting watchdog timer to %d minutes\n", watchdog_minutes);
		alarm(watchdog_minutes * 60);
	}

	/*Let the user know the amount of space available*/
	printe("[Info]  Disk free: %.0lf MB\n", (double)pwd_free / (double)MEGA);
	printe("[Info]  Vobs size: %.0lf MB\n", (double)vob_size / (double)MEGA);

	/* now the actual check if enough space is free */
	if (pwd_free < vob_size) {
		if (!pwd_free)
			printe("\n[Error] statfs (statvfs) seems not to work on the output directory.\n");
		if (!force_flag) {
			printe("\n[Error] There appears not to be enough space in the output directory to continue.\n");

			op = get_option("[Error] Please choose [y] to continue or [n] to quit: ", "yn");
			switch (op) {
			case 'y':
				force_flag = true;
				if (verbosity_level >= 1)
					printe("[Info] y pressed - force write\n");
			case 'n':
				if (verbosity_level >= 1)
					printe("[Info] n pressed\n");
				goto end;
			}
		}
	}

	if (mirror_flag) {
		mirror(dvd_name, pwd, pwd_free, stdout_flag, onefile, provided_input_dir, dvd);
		goto end;
	}

	/*
	 * Open now up the actual files for reading
	 * they come from libdvdread merged together under the given title number
	 * (thx again for the great library)
	 */
	printe("[Info] Using Title: %i\n", titleid);
	printe("[Info] Title has %d chapters and %d angles\n", tt_srpt->title[titleid - 1].nr_of_ptts, tt_srpt->title[titleid - 1].nr_of_angles);
	printe("[Info] Using Chapter: %i\n", chapid + 1);
	printe("[Info] Using Angle: %i\n", angle + 1);

	/*print dvd name*/
	printe("\n[Info] DVD-name: %s\n", dvd_name);
	/*if the user has given a name for the file */
	if (provided_dvd_name_flag && !stdout_flag) {
		printe("\n[Info] Your name for the dvd: %s\n", provided_dvd_name);
		safestrncpy(dvd_name, provided_dvd_name, sizeof(dvd_name));
	}

	/**
	 * We've got enough info, time to open the title set data.
	 */
	dvd_file = DVDOpenFile(dvd, tt_srpt->title[titleid - 1].title_set_nr, DVD_READ_TITLE_VOBS);
	if (!dvd_file) {
		printe("[Error] Can't open title VOBS (VTS_%02d_1.VOB).\n", tt_srpt->title[titleid - 1].title_set_nr);
		goto end;
	}

	file_size_in_blocks = DVDFileSize(dvd_file);

	if (vob_size == (-get_sector_offset(start_sector) - get_sector_offset(end_sector))) {
		vob_size = ((off_t) (file_size_in_blocks) * (off_t) DVD_SECTOR_SIZE) -
			   get_sector_offset(start_sector) - get_sector_offset(end_sector);
		if (verbosity_level >= 1)
			printe("[Info] Vob_size was 0\n");
	}

	/*debug-output: difference between vobsize read from cd and size returned from libdvdread */
	if (mounted && verbose_flag) {
		printe("\n[Info] Difference between vobsize read from cd and size returned from libdvdread:\n");
		/*printe("vob_size (stat) = %lu\nlibdvdsize      = %lu\ndiff            = %lu\n", TODO:the diff returns only crap...
		   vob_size, 
		   ( off_t ) ( file_size_in_blocks ) * ( off_t ) DVD_SECTOR_SIZE, 
		   ( off_t ) vob_size - ( off_t ) ( ( off_t )( file_size_in_blocks ) * ( off_t ) ( DVD_SECTOR_SIZE ) ) ); */
		printe("[Info] Vob_size (stat) = %lu\n[Info] libdvdsize      = %lu\n",
			(long unsigned int)vob_size,
			(long unsigned int)((off_t) (file_size_in_blocks) *
					    (off_t) DVD_SECTOR_SIZE));
	}

	/*********************
	 * this is the main read and copy loop
	 *********************/

	partcount    = 0;
	num_of_files = 0;
	while (offset < (file_size_in_blocks - start_sector - end_sector)) {
		partcount++;

		/*if the stream doesn't get written to stdout */
		if (!stdout_flag) {
			/*part to distribute the files over different directories */
			if (paths_taken == 0) {
				add_end_slash(pwd);
				free_space = get_free_space(pwd);

				if (verbosity_level > 1)
					printe("[Info] Free space for -o dir: %.0f\n", (float)free_space);
				make_output_path(pwd, name, dvd_name, titleid, -1);
			} else {
				for (i = 1; i < alternate_dir_count; i++) {
					if (paths_taken == i) {
						add_end_slash(alternate_output_dir[i - 1]);
						free_space = get_free_space (alternate_output_dir[i - 1]);

						if (verbosity_level > 1)
							printe("[Info] Free space for -%i dir: %.0f\n", i, (float)free_space);

						make_output_path(alternate_output_dir[i - 1], name, dvd_name, titleid, -1);
						/*alternate_dir_count--;*/
					}
				}
			}

			streamout = open_partial(name);
			if (streamout < 0) {
				printe("[Error] Either don't have access or couldn't open %s\n", name);
				goto end;
			}
		}

		if (stdout_flag)			/*this writes to stdout */
			streamout = STDOUT_FILENO;	/*in other words: 1, see "man stdout" */

		/* this here is the main copy part */

		printe("\n");
		memset(bufferin, 0, (DVD_SECTOR_SIZE * sizeof(unsigned char)));

		/*dvd context, starting sector, ending sector, retries, output*/
		rip_vob_file(dvd_file, start_sector, end_sector, 10, streamout);

		if (!stdout_flag) {
			if (fdatasync(streamout) < 0)
				die("\n[Error] error writing to %s \n"
				       "[Error] error: %s\n",
				       name,
				       strerror(errno)
				);

			close(streamout);

			/*rename file from .partial to what it should be called.*/
			rename_partial(name);

			if (verbosity_level >= 1) {
				printe("[Info] offset at the end %8.0f \n",      (float)offset);
				printe("[Info] file_size_in_blocks %8.0f \n",    (float)file_size_in_blocks);
			}

			/* now lets see whats the size of this file in bytes */
			disk_vob_size += filesizeof(name);

			if (verbosity_level >= 1) {
				printe("[Info] Single file size (of copied file %s ) %.0f\n", name, filesizeof(name));
				printe("[Info] Cumulated size %.0f\n", (float)disk_vob_size);
			}
		}
		printe("\n[Info] Successfully copied file %s\n", name);

		num_of_files++; /* # of seperate files we have written */
	}
	/*end of main copy loop */

	if (verbosity_level >= 1)
		printe("[Info] # of separate files: %i\n", num_of_files);



	/******************
	 * Print the status
	 */
	printe("\n[Info] Copying finished! Let's see if the sizes match (roughly)\n\n");
	printe("\n[Info] Difference in bytes: %lld\n", (vob_size - disk_vob_size)); /*REMOVE, TESTING ONLY*/
	printe(  "[Info] Combined size of title-vobs: %.0f (%.0f MB)\n", (float)vob_size, (float)vob_size / (float)MEGA);
	printe(  "[Info] Copied size (size on disk):  %.0f (%.0f MB)\n", (float)disk_vob_size, (float)disk_vob_size / (float)MEGA);

	if ((vob_size - disk_vob_size) > MAX_DIFFER) {
		printe("[Error] Hmm, the sizes differ by more than %d\n", MAX_DIFFER);
		printe("[Hint] Take a look with MPlayer if the output is ok\n");
		goto end;
	}

	printe("[Info] Everything seems to be fine, the sizes match pretty good\n");
	printe("[Hint] Have a lot of fun!\n");

	/*********************************
	 * clean up and close everything *
	 *********************************/

	end:
		free(pwd);
		free(name);
		free(onefile);
		free(bufferin);
		free(dvd_path);
		free(vobcopy_call);
		free(logfile_path);
		free(provided_input_dir);
		free(provided_output_dir);

		for (i = 0; i < 4; i++)
			free(alternate_output_dir[i]);
		free(alternate_output_dir);

		if (dvd_file)
			DVDCloseFile(dvd_file);
		if (vmg_file)
			ifoClose(vmg_file);
		if (dvd)
			DVDClose(dvd);

	return 0;
}


/*
 * if you symlinked a dir to some other place the path name might not get
 * ended by a slash after the first tab press, therefore here is a / added
 * if necessary
 */

int add_end_slash(char *path)
{				/* add a trailing '/' to path */
	char *pointer;

	if (path[strlen(path) - 1] != '/') {
		 pointer = path + strlen(path);
		*pointer = '/';
		 pointer++;
		*pointer = '\0';
	}

	return 0;
}

/*
 * this function concatenates the given information into a path name
 */

int make_output_path(char *pwd, char *name, char *dvd_name, int titleid, int partcount)
{
	char temp[5];

	safestrncpy(name, pwd, PATH_MAX);
	strcat(name, dvd_name);

	memset(&temp, 0, sizeof(temp));
	snprintf(temp, sizeof(temp), "%d", titleid);
	strcat(name, temp);

	if (partcount >= 0) {
		strcat(name, "-");
		sprintf(temp, "%d", partcount);
		strcat(name, temp);
	}

	strcat(name, ".vob");

	if (!have_access(name, true))
		die("[Error] No access to %s\n", name);
	printe("\n[Info] Outputting to %s\n", name);
	return 0;
}

/*
 *The usage function
 */

void usage(char *program_name)
{
	printe( "Vobcopy " VERSION " - GPL Copyright (c) 2001 - 2009 robos@muon.de\n"
		"\nUsage: %s \n"
		"if you want the main feature (title with most chapters) you don't need _any_ options!\n"
		"Options:\n"
		"[-m (mirror the whole dvd)] \n"
		"[-M (Main title - i.e. the longest (playing time) title on the dvd)] \n"
		"[-i /path/to/the/mounted/dvd/]\n"
		"[-n title-number] \n"
		"[-t <your name for the dvd>] \n"
		"[-o /path/to/output-dir/ (can be \"stdout\" or \"-\")] \n"
		"[-f (force output)]\n"
		"[-V (version)]\n"
		"[-v (verbose)]\n"
		"[-v -v (create log-file)]\n"
		"[-h (this here ;-)] \n"
		"[-I (infos about title, chapters and angles on the dvd)]\n"
		"[-b <skip-size-at-beginning[bkmg]>] \n"
		"[-e <skip-size-at-end[bkmg]>]\n"
		"[-O <single_file_name1,single_file_name2, ...>] \n"
		"[-q (quiet)]\n"
		"[-w <watchdog-minutes>]\n"
		"[-x (overwrite all)]\n",
		program_name
	);
}

/**
 * Returns true if the pack is a NAV pack.  This check is clearly insufficient,
 * and sometimes we incorrectly think that valid other packs are NAV packs.  I
 * need to make this stronger.
 */
int is_nav_pack(unsigned char *buffer)
{
	return (buffer[41] == 0xbf && buffer[1027] == 0xbf);
}

/*
* Check the time determine whether a new progress line should be output (once per second)
*/

void progressUpdate(int starttime, int cur, int tot, int force)
{
	static int progress_time = 0;

	if (progress_time && (progress_time == time(NULL)) && (!force))
		return;

	int barLen, barChar, numChars, timeSoFar, minsLeft, secsLeft, ctr, cols;
	float percentComplete, percentLeft, timePerPercent;
	int curtime, timeLeft;
	struct winsize ws;

	ioctl(0, TIOCGWINSZ, &ws);
	cols = ws.ws_col - 1;

	progress_time = time(NULL);
	curtime = time(NULL);

	printf("\r");
	/*calc it this way so it's easy to change later */
	/*2 for brackets, 1 for space, 5 for percent complete, 1 for space,
	 *6 for time left, 1 for space, 1 for 100.0%, 4 for status bar bug*/
	barLen = cols - 2 - 1 - 5 - 1 - 6 - 1 - 1 - 4;
	barChar = (int)roundf((float)tot / (float)barLen);
	percentComplete = (((float)cur / (float)tot) * 100.f);
	percentLeft = 100.f - percentComplete;
	numChars = ((float)cur / (float)barChar);

	/*guess remaining time */
	timeSoFar = curtime - starttime;
	if (percentComplete == 0)
		timePerPercent = 0;
	else
		timePerPercent = (float)(timeSoFar / percentComplete);

	timeLeft = timePerPercent * percentLeft;
	minsLeft = (int)(timeLeft / 60);
	secsLeft = (int)(timeLeft % 60);

	printf("[");
	for (ctr = 0; ctr < numChars - 1; ctr++)
		printf("=");

	printf("|");
	for (ctr = numChars; ctr < barLen; ctr++)
		printf(" ");

	printf("] ");
	printf("%6.1f%% %02d:%02d ", percentComplete, minsLeft, secsLeft);
	fflush(stdout);
}

void set_signal_handlers(void)
{
	struct sigaction action;

	/*SIGALRM or watchdog handler*/
	action.sa_flags = 0;
	action.sa_handler = watchdog_handler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGALRM, &action, NULL);

	/*SIGTERM or ctrl+c handler*/
	action.sa_flags = 0;
	action.sa_handler = shutdown_handler;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTERM, &action, NULL);
}

void watchdog_handler(int signal)
{
	printe("\n[Info] Timer expired - terminating program.\n");
	kill(getpid(), SIGTERM);
}

void shutdown_handler(int signal)
{
	printe("\n[Info] Terminate signal received, exiting.\n");
	_exit(2);
}

#if defined(__APPLE__) && defined(__GNUC__) || defined(OpenBSD)
int fdatasync(int value)
{
	return 0;
}
#endif
