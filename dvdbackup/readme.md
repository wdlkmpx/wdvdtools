
## dvdbackup

Origin:
- https://sourceforge.net/projects/dvdbackup/files/dvdbackup/dvdbackup-0.4.2/dvdbackup-0.4.2.tar.xz

Removed autotools/gettext code

Applied misc patches:

- https://www.fabiankeil.de/sourcecode/dvdbackup-0.4.2-combined-patches.diff
- http://deb.debian.org/debian/pool/main/d/dvdbackup/dvdbackup_0.4.2-4.1.debian.tar.xz
- https://github.com/archlinux/svntogit-community/blob/packages/dvdbackup/trunk/dvdbackup-dvdread-6.1.patch
- https://github.com/dustin/dvdbackup

## README

To gather info about the DVD:

	dvdbackup -I

	/dev/dvd is the default device tried - you need to use -i if your device
	name is different.


General backup information:

	If your backup directory is /my/dvd/backup/dir/ specified with the -o
	flag, then dvdbackup will create a DVD-Video structure under
	/my/dvd/backup/dir/TITLE_NAME/VIDEO_TS.	If the -o flag is omitted, the
	current directory is used.

	Since the title is "unique" you can use the same directory for all your
	DVD backups. If it happens to have a generic title dvdbackup will exit
	with a return value of 2. And you will need to specify a title name with
	the -n switch.

	dvdbackup will always mimic the original DVD-Video structure. Hence if
	you e.g. use the -M (mirror) you will get an exact duplicate of the
	original. This means that every file will be have the same size as the
	original one. Like wise goes also for the -F and the -T switch.

	However the -t and (-t -s/-e) switch is a bit different the titles
	sectors will be written to the original file but not at the same offset
	as the original one since they may be gaps in the cell structure that we
	do not fill.


To backup the whole DVD

	dvdbackup -M

	This action creates a valid DVD-Video structure that can be burned to a
	DVD-/+R(W) with help of genisoimage.


To backup the main feature of the DVD:

	dvdbackup -F

	This action creates a valid DVD-Video structure of the feature title
	set.  Note that this will not result in an image immediately watchable -
	you will need another program like dvdauthor to help construct the IFO
	files.

	dvdbackup defaults to get the 16:9 version of the main feature if a 4:3
	is also present on the DVD. To get the 4:3 version use -a 0.

	dvdbackup makes it best to make a intelligent guess what is the main
	feature of the DVD - in case it fails please send a bug report.


To backup a title set

	dvdbackup -T 2

	where "-T 2" specifies that you want to backup title set 2 i.e. all
	VTS_02_X.XXX files.

	This action creates a valid DVD-Video structure	of the specified title
	set.  Note that this will not result in an image immediately watchable -
	you will need another program like dvdauthor to help construct the IFO
	files.


To backup a title:

	dvdbackup -t 1

	This action backups all cells that forms the specified title. Note that
	there can be sector gaps in between one cell and an other. dvdbackup
	will backup all sectors that belongs to the title but will skip sectors
	that are not a part of the title.


To backup a specific chapter or chapters from a title:

	dvdbackup -t 1 -s 20 -e 25

	This action will backup chapter 20 to 25 in title 1, as with the backup
	of a title there can be sector gaps between one chapter (cell) and an
	other. dvdbackup will backup all sectors that belongs to the title 1
	chapter 20 to 25 but will skip sectors that are not a part of the
	title 1 chapter 20 to 25.

	To backup a single chapter e.g. chapter 20 do -s 20 -e 20.
	To backup from chapter 20 to the end chapter use only -s 20.
	To backup to chapter 20 from the first chapter use only -e 20.

	You can skip the -t switch and let the program guess the title although
	it is not recommended.

	If you specify a chapter that his higher than the last chapter of the
	title dvdbackup will truncate to the highest chapter of the title.

Return values:
	0 on success
	1 on usage error
	2 on title name error
	-1 on failure


Todo - i.e. what's on the agenda.
	Make the main feature guessing algorithm better. Not that it doesn't do
	it's job, but it's implementation isn't that great. I would also like
	to preserve more information about the main feature since that would
	let me perform better implementations in other functions that depends
	on the titles_info_t and title_set_info_t structures.

	Make it possible to extract cells in a title not just chapters (very
	easy so it will definitely be in the next version).

	Make a split mirror (-S) option that divides a DVD-9 to two valid DVD-5
	video structures. This is not a trivial hack and it's my main goal
	the next month or so. It involves writing ifoedit and vobedit
	libraries in order to be able to manipulate both the IFO structures
	and the VOB files. Out of this will most probably also come tscreate
	and vtscreate which will enable you to make a very simple DVD-Video
	from MPEG-1/2 source.
