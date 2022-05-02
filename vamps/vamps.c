// Vamps - eVAporate DVD compliant Mpeg2 Program Stream
//
// This file is part of Vamps.
//
// Vamps is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//
// Vamps is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Vamps; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
//
// Revision history (latest first):
//
// 2006/04/15: V0.99.2: Fixed some signed/unsigned issues which caused compiler
//                      warnings on some platforms. No funtional changes.
//
// 2006/02/26: V0.99.1: Introduced detection of special packets inserted by
//                      play_cell when it encounters a cell gap. This enables
//                      proper evaporation of some strangely authored DVDs.
//
// 2006/02/14: V0.99: Usage of inttypes.h. No functional changes (however,
//		      play_cell *is* modified).
//
// 2005/11/06: V0.98: Updated requant module to
//		      latest version from Metakine web site
//		      (http://www.metakine.com/files/M2VRequantiser.tgz).
//		      Since the new version requires the total size of the
//		      unshrinked video elementary stream at start time, the
//		      user *has to* supply the size of the unshrinked PS if
//		      standard input is not redirected from a regular file.
//		      For this purpose the PS-size (-S) option has been added.
//		      Fixed large file support (>2GB).
//		      Introduced preserve option (-p) on user request. If
//		      enabled, the audio and subpicture tracks will keep their
//		      original identifiers. This is required for full DVD
//		      copies, i.e. including control structures, menus, etc.
//
// 2005/11/02:        Fixed support for MPEG audio. Did never work up to now.
//                    Added support for mere padding packets. These actually
//                    should not occur in DVD program streams but they do. At
//                    least mplex *does* spit out such.
//
// 2004/11/30: V0.97-1: Implemented injection support. This gives us *much*
//                    more reliable results with the targeted size. The
//                    vaporized data size now usually hits within a range of
//                    a few thousand sectors (i.e. <~5MB) only. Don't forget
//                    to remove the injections file with each new disk!
//
// 2004/11/29: V0.97: Video buffers aren't static any more. This
//                    is needed for streams with multiple adjacent
//                    GOPs not starting on sector boundaries.
//
// 2004/05/25: V0.96: Now GOPs do not necessarily need to start
//                    on a sector boundary. This gets us rid of
//                    "Bad sequence header code at ...".
//
// 2003/12/13: V0.95: No more known bugs nor any essential to-dos!

// for large file support (>2GB)
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS	64
#define VERSION			"V0.99.2"

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/stat.h>

// DVD sector size
#define SECT_SIZE 2048

// read buffer size (4MB)
#define RBUF_SIZE (0x1000*1024)

// write buffer size (4MB)
#define WBUF_SIZE (0x1000*1024)

// initial video buffer size (1MB)
#define VBUF_SIZE (1024*1024)


// a lot of global data - we really should use C++...
uint8_t     rbuf  [RBUF_SIZE];		// the PS read buffer
uint8_t     wbuf  [WBUF_SIZE];		// the PS write buffer
uint8_t    *vibuf;			// the video ES requant input buffer
uint8_t    *vobuf;			// the video ES requant output buffer
uint8_t    *rptr = rbuf;		// pointer to current char in read buf
uint8_t    *rhwp = rbuf;		// read buffer high water pointer
uint8_t    *wptr = wbuf;		// pointer to first unused char in wbuf
uint64_t    bytes_read;			// total PS bytes read
uint64_t    bytes_written;		// total PS bytes written
uint64_t    padding_bytes;		// total padding bytes written
uint64_t    vin_bytes;			// total unshrinked video ES bytes
uint64_t    vout_bytes;			// total shrinked video ES bytes
uint64_t    ps_size;			// total PS size in bytes
int         vbuf_size = VBUF_SIZE;	// the video ES requant buffers' size
int         vilen;			// current GOP's unshrinked vidES bytes
int         volen;			// current GOP's shrinked vidES bytes
int         total_packs;		// total no. PS packs
int         video_packs;		// no. video packs in PS
int         skipped_video_packs;	// skipped thereof
int         aux_packs;			// no. audio and subpicture packs in PS
int         skipped_aux_packs;		// skipped thereof
int         cell_gap_packs;		// sectors skipped reported by play_cell
int         sequence_headers;		// no. sequence headers (== #GOPs)
int         nav_packs;			// no. nav packs
static int         v_eof;			// end of file flag
uint8_t    *rqt_rptr;			// ptr to cur char in requant read buf
uint8_t    *rqt_wptr;			// ptr to first unused char in rqt wbuf
int         rqt_rcnt;			// no. bytes in requant read buffer
int         rqt_wcnt;			// no. bytes in requant write buffer
uint64_t    rqt_inbytes;		// total bytes fed into requantization
uint64_t    rqt_outbytes;		// total bytes melt from requantization
uint64_t    rqt_visize;			// calc. total unshrinked vidES bytes
float       rqt_fact;			// video ES vaporization factor (>1)
int         spu_track_map [32];		// subpicture track# translation map
int         audio_track_map [8];	// audio track# translation map
int         verbose;			// level of verbosity
int         calc_ps_vap;		// calc vaporization based on PS size
int         preserve;			// preserve audio/spu track numbers
float       vap_fact    = 1.0f;		// vaporization factor from cmd line
const char *injections_file;		// where to inject internal status from
const char  progname [] = "vamps";	// we're sucking bytes!

// injections file stuff
#define inject_int(v)		{ sizeof (v), &v, "%d"   }
#define inject_uint64(v)	{ sizeof (v), &v, "%llu" }
#define N_INJECTIONS		(sizeof (injections) / sizeof (*injections))

struct
{
  int         s;	// variable size
  const void *p;	// variable pointer
  const char *f;	// printf/scanf format
} injections [] =
{
  inject_uint64 (bytes_read),
  inject_uint64 (bytes_written),
  inject_uint64 (padding_bytes),
  inject_uint64 (vin_bytes),
  inject_uint64 (vout_bytes),
  inject_int    (total_packs),
  inject_int    (video_packs),
  inject_int    (skipped_video_packs),
  inject_int    (aux_packs),
  inject_int    (skipped_aux_packs),
  inject_int    (cell_gap_packs),
  inject_int    (sequence_headers),
  inject_int    (nav_packs)
};

// stuff for inter-thread com
pthread_cond_t  condr = PTHREAD_COND_INITIALIZER;
pthread_cond_t  condw = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutr  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutw  = PTHREAD_MUTEX_INITIALIZER;


// prototypes
void scan_track_map (int *, unsigned);
void vaporize (void);
void read_injections (const char *);
void write_injections (const char *);
void usage (void);
void fatal (char *, ...);

// this one lives in requant.c
extern void *requant_thread (void *);


int
main (int argc, char *const argv [])
{
  int                  c;
  char                *s;
  struct stat          st;
  pthread_t            thread;
  static struct option long_options [] =
  {
    { "audio",          1, NULL, 'a' },
    { "subpictures",    1, NULL, 's' },
    { "evaporate",      1, NULL, 'e' },
    { "ps-evaporate",   1, NULL, 'E' },
    { "verbose",        0, NULL, 'v' },
    { "inject",         1, NULL, 'i' },
    { "preserve",       0, NULL, 'p' },
    { "ps-size",        1, NULL, 'S' },
    { NULL,             0, NULL, 0   }
  };

  opterr = 1;

  while ((c = getopt_long_only (argc, argv, "a:s:e:E:vi:pS:",
				long_options, NULL)) >= 0)
  {
    switch (c)
    {
      case 'a':
	scan_track_map (audio_track_map, 8);
	break;

      case 's':
	scan_track_map (spu_track_map, 32);
	break;

      case 'e':
	calc_ps_vap = 0;
	vap_fact    = strtof (optarg, &s);

	if (*s)
	{
	  fprintf (stderr,
 "%s: Bad factor. You have to supply a floating point number, e.g. `1.5'.\n\n",
		   optarg);
	  usage ();
	}

	break;

      case 'E':
	calc_ps_vap = 1;
	vap_fact    = strtof (optarg, &s);

	if (*s)
	{
	  fprintf (stderr,
 "%s: Bad factor. You have to supply a floating point number, e.g. `1.5'.\n\n",
		   optarg);
	  usage ();
	}

	break;

      case 'v':
	verbose++;
	break;

      case 'i':
	injections_file = optarg;
        break;

      case 'p':
	preserve = 1;
	break;

      case 'S':
	ps_size = strtoull (optarg, &s, 10);

	if (*s || !ps_size)
	{
	  fprintf (stderr,
		  "%s: Bad program stream size. Please give size in bytes.\n\n",
		   optarg);
	  usage ();
	}

	break;

      default:
	putc ('\n', stderr);
	usage ();
    }
  }

  // read injections file - leaves variables untouched if file not present
  if (injections_file)
    read_injections (injections_file);

  // determine total size of input program stream
  if (!ps_size)
  {
    // size not given on command line
    if (fstat (0, &st) < 0 || !S_ISREG (st.st_mode))
    {
      // no way out
     fputs ("Standard input is not a regular file and -S option not given.\n\n",
	    stderr);
      usage ();
    }

    ps_size = st.st_size;
  }

  // allocate video buffers
  vibuf = malloc (vbuf_size);
  vobuf = malloc (vbuf_size);

  if (vibuf == NULL || vobuf == NULL)
    fatal ("Allocation of video buffers failed: %s", strerror (errno));

  // create requantization thread
  if (pthread_create (&thread, NULL, requant_thread, NULL))
      fatal ("pthread_create: %s", strerror (errno));

  // actually do vaporization
  vaporize ();

  // save internal status for adjacent run
  if (injections_file)
    write_injections (injections_file);

  // print this run's statistics if wanted
  if (verbose >= 1)
  {
    fprintf (stderr, "Info: Total bytes read: %llu\n", bytes_read);
    fprintf (stderr, "Info: Total bytes written: %llu\n", bytes_written);
    fprintf (stderr, "Info: Total sequence headers in video stream: %d\n",
	     sequence_headers);
    fprintf (stderr,
	     "Info: Total navigation packs (system headers/PCI/DSI): %d\n",
	     nav_packs);
    fprintf (stderr,
"Info: Total bytes vaporized in audio and subpicture streams: %llu (%.2f%%)\n",
	     (uint64_t) skipped_aux_packs * SECT_SIZE,
	     (double) skipped_aux_packs /
	     (double) (bytes_read / SECT_SIZE) * 100.0);
    fprintf (stderr,
	     "Info: Total bytes vaporized in cell gaps: %llu (%.2f%%)\n",
	     (uint64_t) cell_gap_packs * SECT_SIZE,
	     (double) cell_gap_packs /
	     (double) (bytes_read / SECT_SIZE) * 100.0);
    fprintf (stderr,
	     "Info: Total bytes vaporized in video stream: %llu (%.2f%%)\n",
	     (uint64_t) skipped_video_packs * SECT_SIZE,
	     (double) skipped_video_packs /
	     (double) (bytes_read / SECT_SIZE) * 100.0);
    fprintf (stderr,
	     "Info: Total padding bytes in video stream: %llu (%.2f%%)\n",
	     padding_bytes,
	     (double) padding_bytes / (double) bytes_written * 100.0);
    fprintf (stderr, "Info: Total vaporization factor: %.3f\n",
	     (double) bytes_read / (double) bytes_written);
    fprintf (stderr,
	     "Info: Average actual video ES vaporization factor: %.3f\n",
	     (double) vin_bytes / (double) vout_bytes);
    //fprintf (stderr,
    //	  "total_packs=%d, video_packs=%d, aux_packs=%d, nav_packs=%d\n",
    //	     total_packs, video_packs, aux_packs, nav_packs);
  }

  return 0;
}


// analyze track numbers given in cmd line ("x,y,z")
void
scan_track_map (int *map, unsigned ntracks)
{
  unsigned i = 1;
  unsigned track;
  char    *op = optarg;

  do
  {
    track = strtol (op, &op, 10) - 1;

    if (track >= ntracks)
      usage ();

    map [track] = i;
  }
  while (i++ <= ntracks && *op++);
}


// lock `size' bytes in read buffer
// i.e. ensure the next `size' input bytes are available in buffer
// returns nonzero on EOF
static inline int
lock (int size)
{
  int avail, n;

  avail = rhwp - rptr;

  if (avail >= size)
    return 0;

  if (avail)
  {
    memcpy (rbuf, rptr, avail);
    rptr = rbuf;
    rhwp = rptr + avail;
  }

  n = read (0, rhwp, RBUF_SIZE - avail);

  if (n % SECT_SIZE)
    fatal ("Premature EOF");

  rhwp       += n;
  bytes_read += n;

  return !n;
}


// copy `size' bytes from rbuf to wbuf
static inline void
copy (int size)
{
  if (!size)
    return;

  if ((wptr - wbuf) + size > WBUF_SIZE)
    fatal ("Write buffer overflow");

  memcpy (wptr, rptr, size);
  rptr += size;
  wptr += size;
}


// skip `size' bytes in rbuf
static inline void
skip (int size)
{
  rptr += size;
}


// flush wbuf
static void
flush (void)
{
  int size;

  size = wptr - wbuf;

  if (!size)
    return;

  if (write (1, wbuf, size) != size)
    fatal ("write to stdout: %s", strerror (errno));

  wptr           = wbuf;
  bytes_written += size;
}


// returns no. bytes read up to where `ptr' points
static inline uint64_t
rtell (uint8_t *ptr)
{
  return bytes_read - (rhwp - ptr);
}


// returns no. bytes written up to where `ptr' points
// (including those in buffer which are not actually written yet)
static inline uint64_t
wtell (uint8_t *ptr)
{
  return bytes_written + (ptr - wbuf);
}


// some pack header consistency checking
static inline void
check_pack (uint8_t *ptr)
{
  uint32_t pack_start_code;
  int      pack_stuffing_length;

  pack_start_code  = (uint32_t) (ptr [0]) << 24;
  pack_start_code |= (uint32_t) (ptr [1]) << 16;
  pack_start_code |= (uint32_t) (ptr [2]) <<  8;
  pack_start_code |= (uint32_t) (ptr [3]);

  if (pack_start_code != 0x000001ba)
    fatal ("Bad pack start code at %llu: %08lx", rtell (ptr), pack_start_code);

  if ((ptr [4] & 0xc0) != 0x40)
    fatal ("Not an MPEG2 program stream pack at %llu", rtell (ptr));

  // we rely on a fixed pack header size of 14
  // so better to ensure this is true
  pack_stuffing_length = ptr [13] & 7;

  if (pack_stuffing_length)
    fatal ("Non-zero pack stuffing length at %llu: %d\n",
	   rtell (ptr), pack_stuffing_length);
}


// video packet consistency checking
static inline int
check_video_packet (uint8_t *ptr)
{
  int      vid_packet_length, pad_packet_length, rc = 0;
  uint32_t vid_packet_start_code, pad_packet_start_code, sequence_header_code;

  vid_packet_start_code  = (uint32_t) (ptr [0]) << 24;
  vid_packet_start_code |= (uint32_t) (ptr [1]) << 16;
  vid_packet_start_code |= (uint32_t) (ptr [2]) <<  8;
  vid_packet_start_code |= (uint32_t) (ptr [3]);

  if (vid_packet_start_code != 0x000001e0)
    fatal ("Bad video packet start code at %llu: %08lx",
	   rtell (ptr), vid_packet_start_code);

  vid_packet_length  = ptr [4] << 8;
  vid_packet_length |= ptr [5];
  vid_packet_length += 6;

  if ((ptr [6] & 0xc0) != 0x80)
    fatal ("Not an MPEG2 video packet at %llu", rtell (ptr));

  if (ptr [7])
  {
    if ((ptr [7] & 0xc0) != 0xc0)
      fatal ("First video packet in sequence starting at %llu "
	     "misses PTS or DTS, flags=%02x", rtell (ptr), ptr [7]);

    sequence_header_code  = (uint32_t) (ptr [6 + 3 + ptr [8] + 0]) << 24;
    sequence_header_code |= (uint32_t) (ptr [6 + 3 + ptr [8] + 1]) << 16;
    sequence_header_code |= (uint32_t) (ptr [6 + 3 + ptr [8] + 2]) <<  8;
    sequence_header_code |= (uint32_t) (ptr [6 + 3 + ptr [8] + 3]);

    if (sequence_header_code == 0x000001b3)
    {
      rc = 1;
    }
    else
    {
      //fprintf (stderr, "Start of GOP at %llu not on sector boundary\n",
      //         rtell (ptr + 6 + 3 + ptr [8]));
      sequence_headers++;
    }
  }

  pad_packet_length = 0;

  if (14 + vid_packet_length < SECT_SIZE - 6)
  {
    // video packet does not fill whole sector
    // check for padding packet
    ptr += vid_packet_length;

    pad_packet_start_code  = (uint32_t) (ptr [0]) << 24;
    pad_packet_start_code |= (uint32_t) (ptr [1]) << 16;
    pad_packet_start_code |= (uint32_t) (ptr [2]) <<  8;
    pad_packet_start_code |= (uint32_t) (ptr [3]);

    if (pad_packet_start_code != 0x000001be)
      fatal ("Bad padding packet start code at %llu: %08lx",
	     rtell (ptr + vid_packet_length), pad_packet_start_code);

    pad_packet_length  = ptr [4] << 8;
    pad_packet_length |= ptr [5];
    pad_packet_length += 6;
  }

  // length of video packet plus padding packet must always match sector size
  if (14 + vid_packet_length + pad_packet_length != SECT_SIZE)
    fatal ("Bad video packet length at %llu: %d",
	   rtell (ptr), vid_packet_length);

  return rc;
}


// here we go
// this is where we switch to the requantization thread
// note that this and the requant thread never run concurrently (apart
// from a very short time) so a dual CPU box does not give an advantage
// returns size of evaporated GOP
static inline int
requant (uint8_t *dst, uint8_t *src, int n, float fact)
{
  int rv;

  // this ensures for the requant thread to stop at this GOP's end
  memcpy (src + n, "\0\0\1", 3);

  pthread_mutex_lock (&mutr);

  rqt_rptr     = src;
  rqt_wptr     = dst;
  rqt_rcnt     = n;
  rqt_wcnt     = 0;
  rqt_fact     = fact;
  rqt_inbytes  = vin_bytes;
  rqt_outbytes = vout_bytes;
  rqt_visize   = (uint64_t) ((float) ps_size * (float) vin_bytes /
			     ((float) total_packs * (float) SECT_SIZE));

  pthread_cond_signal (&condr);
  pthread_mutex_unlock (&mutr);

  // now the requant thread should be running

  pthread_mutex_lock (&mutw);

  // wait for requant thread to finish
  while (!rqt_wcnt)
    pthread_cond_wait (&condw, &mutw);

  rv = rqt_wcnt;

  pthread_mutex_unlock (&mutw);

  if (verbose >= 2)
    fprintf (stderr, "Info: Actual video ES vaporization factor: %.3f\n",
	     (double) n / (double) rv);

  return rv;
}


// translate type of private stream 1 packet
// according to the track translation maps
// returns new track type (e.g. 0x80 for first AC3 audio
// track in cmd line) or zero if track is not to be copied
static inline int
new_private_1_type (uint8_t *ptr)
{
  int type, track, abase;

  type = ptr [6 + 3 + ptr [8]];
  //fprintf (stderr, "type=%02x\n", type);

  if (type >= 0x20 && type <= 0x3f)
  {
    // subpicture
    track = spu_track_map [type - 0x20];

    return track ? track - 1 + 0x20 : 0;
  }

  if (type >= 0x80 && type <= 0x87)
  {
    // AC3 audio
    abase = 0x80;
  }
  else if (type >= 0x88 && type <= 0x8f)
  {
    // DTS audio
    abase = 0x88;
  }
  else if (type >= 0xa0 && type <= 0xa7)
  {
    // LPCM audio
    abase = 0xa0;
  }
  else
  {
    fatal ("Unknown private stream 1 type at %llu: %02x", rtell (ptr), type);
    abase = 0;
  }

  track = audio_track_map [type - abase];

  return track ? track - 1 + abase : 0;
}


// selectivly copy private stream 1 packs
// patches track type to reflect new track
// mapping unless user opted to preserve them
static inline void
copy_private_1 (uint8_t *ptr)
{
  int type;

  type = new_private_1_type (ptr);

  if (type)
  {
    if (!preserve)
      ptr [6 + 3 + ptr [8]] = type;

    copy (SECT_SIZE);

    return;
  }

  skip (SECT_SIZE);
}


// translate ID of MPEG audio packet
// according to the audio track translation map
// returns new ID (e.g. 0xc0 for first MPEG audio
// track in cmd line) or zero if track is not to be copied
static inline int
new_mpeg_audio_id (int id)
{
  int track;

  track = audio_track_map [id - 0xc0];

  return track ? track - 1 + 0xc0 : 0;
}


// selectivly copy MPEG audio packs
// patches ID to reflect new track mapping unless user opted to preserve them
static inline void
copy_mpeg_audio (uint8_t *ptr)
{
  int id;

  id = new_mpeg_audio_id (ptr [3]);

  if (id)
  {
    if (!preserve)
      ptr [3] = id;

    copy (SECT_SIZE);

    return;
  }

  skip (SECT_SIZE);
}


// process beginning of program stream up to
// - but not including - first sequence header
// this PS leader is NOT shrunk since the PS may not
// necessarily begin at a GOP boundary (although it should?)
// nevertheless the unwanted private stream 1 and MPEG audio
// packs are skipped since some players could get confused otherwise
static void
vap_leader (void)
{
  uint8_t *ptr;
  int      id, data_length;

  while (!lock (SECT_SIZE))
  {
    ptr = rptr;
    check_pack (ptr);

    ptr += 14;
    id   = ptr [3];

    switch (id)
    {
      case 0xe0:
	// video
	if (check_video_packet (ptr))
	  // sequence header
	  return;

	copy (SECT_SIZE);
	break;

      case 0xbd:
	// private 1: audio/subpicture
	copy_private_1 (ptr);
	break;

      case 0xc0:
      case 0xc1:
      case 0xc2:
      case 0xc3:
      case 0xc4:
      case 0xc5:
      case 0xc6:
      case 0xc7:
	// MPEG audio
	copy_mpeg_audio (ptr);
	break;

      case 0xbb:
	// system header/private 2: PCI/DSI
	copy (SECT_SIZE);
	break;

      case 0xbe:
	// padding
	data_length  = ptr [4] << 8;
	data_length |= ptr [5];

	if (14 + data_length != SECT_SIZE - 6)
	  fatal ("Bad padding packet length at %llu: %d",
		 rtell (ptr), data_length);

	break;

      case 0xbf:
	// private 2
	if (strcmp ((char *) (ptr + 6), "Vamps-data") != 0 || ptr [17] != 0x01)
	  fatal ("Misplaced private stream 2 pack at %llu", rtell (ptr));

	skip (SECT_SIZE);

	break;

      default:
	fatal ("Encountered stream ID %02x at %llu, "
	       "probably bad MPEG2 program stream", id, rtell (ptr));
    }

    if (wptr == wbuf + WBUF_SIZE)
      flush ();
  }

  v_eof = 1;
  flush ();

  return;
}


// process end of program stream
// the same counts here as for the PS' beginning
static void
vap_trailer (int length)
{
  uint8_t *ptr;
  int      i, id, data_length;

  for (i = 0; i < length; i += SECT_SIZE)
  {
    ptr = rptr + 14;
    id  = ptr [3];

    if (id == 0xbd)
    {
      // private 1: audio/subpicture
      copy_private_1 (ptr);
    }
    else if (id >= 0xc0 && id <= 0xc7)
    {
      // MPEG audio
      copy_mpeg_audio (ptr);
    }
    else if (id == 0xbe)
    {
      // padding
      data_length  = ptr [4] << 8;
      data_length |= ptr [5];

      if (14 + data_length != SECT_SIZE - 6)
	fatal ("Bad padding packet length at %llu: %d",
	       rtell (ptr), data_length);
    }
    else if (id == 0xbf)
    {
      // private 2
      if (strcmp ((char *) (ptr + 6), "Vamps-data") != 0 || ptr [17] != 0x01)
	fatal ("Misplaced private stream 2 pack at %llu", rtell (ptr));

      skip (SECT_SIZE);
    }
    else
    {
      copy (SECT_SIZE);
    }

    if (wptr == wbuf + WBUF_SIZE)
      flush ();
  }

  flush ();
}


// vaporization is split in two phases - this is phase 1
// PS packs are read into rbuf until a sequence header is found.
// All video packs are unpacketized and the contained video ES
// GOP copied to vibuf. In the same course the private stream 1
// and MPEG audio packs are inspected and the number of packs
// not to be copied are counted. This is to forecast the video
// vaporization factor in case the user specified a PS shrink factor.
// returns GOP length in bytes
static inline int
vap_phase1 (void)
{
  uint8_t *ptr, *viptr = vibuf;
  int      seq_length, id, data_length, opt_length, seqhdr, cell_gap;

  for (seq_length = 0;
       !lock (seq_length + SECT_SIZE); seq_length += SECT_SIZE)
  {
    ptr = rptr + seq_length;
    check_pack (ptr);

    // avoid duplicate counts for sequence headers
    if (seq_length)
      total_packs++;

    ptr += 14;
    id   = ptr [3];

    //fprintf (stderr, "id=%02x\n", id);

    switch (id)
    {
      case 0xe0:
	// video
	seqhdr = check_video_packet (ptr);

	if (seq_length)
	{
	  video_packs++;

	  if (seqhdr)
	  {
	    sequence_headers++;
	    vilen = viptr - vibuf;

	    return seq_length;
	  }
	}

	// copy contained video ES fragment to vibuf
	data_length  = ptr [4] << 8;
	data_length |= ptr [5];
	opt_length   = 3 + ptr [8];
	data_length -= opt_length;

	if ((viptr - vibuf) + data_length > vbuf_size - 3)
	{
	  // reallocate video buffers
	  int i = viptr - vibuf;

	  // grow by another VBUF_SIZE bytes
	  vbuf_size += VBUF_SIZE;
	  vibuf      = realloc (vibuf, vbuf_size);
	  vobuf      = realloc (vobuf, vbuf_size);

	  if (vibuf == NULL || vobuf == NULL)
	    fatal ("Reallocation of video buffers failed: %s",
		   strerror (errno));

	  viptr = vibuf + i;
	}

	//fprintf (stderr, "data_length=%d\n", data_length);
	memcpy (viptr, ptr + 6 + opt_length, data_length);
	viptr += data_length;
	break;

      case 0xbd:
	// private 1: audio/subpicture
	aux_packs++;

	if (!new_private_1_type (ptr))
	  skipped_aux_packs++;

	break;

      case 0xc0:
      case 0xc1:
      case 0xc2:
      case 0xc3:
      case 0xc4:
      case 0xc5:
      case 0xc6:
      case 0xc7:
	// MPEG audio
	aux_packs++;

	if (!new_mpeg_audio_id (id))
	  skipped_aux_packs++;

	break;

      case 0xbb:
	// system header/private 2: PCI/DSI
	nav_packs++;
	break;

      case 0xbe:
	// padding
	skipped_aux_packs++;
	data_length  = ptr [4] << 8;
	data_length |= ptr [5];

	if (14 + data_length != SECT_SIZE - 6)
	  fatal ("Bad padding packet length at %llu: %d",
		 rtell (ptr), data_length);

	break;

      case 0xbf:
	// private 2
	if (strcmp ((char *) (ptr + 6), "Vamps-data") != 0 || ptr [17] != 0x01)
	  fatal ("Misplaced private stream 2 pack at %llu", rtell (ptr));

	cell_gap        = ptr [18] << 8;
	cell_gap       |= ptr [19];
	cell_gap_packs += cell_gap--;
	total_packs    += cell_gap;
	bytes_read     += cell_gap * SECT_SIZE;

	break;

      default:
	fatal ("Encountered stream ID %02x at %llu, "
	       "probably bad MPEG2 program stream", id, rtell (ptr));
    }
  }

  v_eof = 1;

  return seq_length;
}


// re-packetize the video ES
// `ptr' points to original PES packet where to put the video data
// `voptr' points to first unpacketized byte in vobuf
// `avail' specifies number of bytes remaining in vobuf
// returns number of ES bytes in generated PES packet
static inline int
gen_video_packet (uint8_t *ptr, uint8_t *voptr, int avail)
{
  int i, header_data_length, data_length, padding_length;

  // if original PES holds optional data (e.g. DTS/PTS) we must keep it
  header_data_length = (ptr [7] & 0xc0) == 0xc0 ? ptr [8] : 0;
  data_length        = SECT_SIZE - (14 + 6 + 3 + header_data_length);

  if (avail >= data_length)
  {
    // write out a full video packet (usually 2025 byte)
    memcpy (ptr + 6 + 3 + header_data_length, voptr, data_length);
    ptr [4] = (SECT_SIZE - (14 + 6)) >> 8;
    ptr [5] = (SECT_SIZE - (14 + 6)) & 0xff;
    ptr [8] = header_data_length;

    return data_length;
  }

  if (avail < data_length - 6)
  {
    // write a short video packet and a padding packet
    memcpy (ptr + 6 + 3 + header_data_length, voptr, avail);
    ptr [4] = (3 + header_data_length + avail) >> 8;
    ptr [5] =  3 + header_data_length + avail;
    ptr [8] = header_data_length;

    // generate padding packet
    ptr           += 6 + 3 + header_data_length + avail;
    padding_length = data_length - (avail + 6);
    padding_bytes += padding_length + 6;
    ptr [0]        = 0;
    ptr [1]        = 0;
    ptr [2]        = 1;
    ptr [3]        = 0xbe;
    ptr [4]        = padding_length >> 8;
    ptr [5]        = padding_length;

    for (i = 0; i < padding_length; i++)
      ptr [6+i] = 0xff;

    return avail;
  }

  // write a padded video packet (1 to 6 padding bytes)
  padding_length = data_length - avail;
  padding_bytes += padding_length;
  memset (ptr + 6 + 3 + header_data_length, 0xff, padding_length);
  header_data_length += padding_length;
  memcpy (ptr + 6 + 3 + header_data_length, voptr, avail);
  ptr [4] = (SECT_SIZE - (14 + 6)) >> 8;
  ptr [5] = (SECT_SIZE - (14 + 6)) & 0xff;
  ptr [8] = header_data_length;

  return avail;
}


// this is phase 2 of vaporization
// the shrunk video ES is re-packetized by using the source PES packets
// unused PS packs are skipped
// only wanted private stream 1 and MPEG audio packs are copied
// all nav packs are copied
static inline void
vap_phase2 (int seq_length)
{
  int      i, id, avail, data_length;
  uint8_t *ptr, *voptr = vobuf, *vohwp = vobuf + volen;

  for (i = 0; i < seq_length; i += SECT_SIZE)
  {
    ptr = rptr + 14;
    id  = ptr [3];

    switch (id)
    {
      case 0xe0:
	// video
	avail = vohwp - voptr;

	if (avail)
	{
	  // still some video output data left
	  voptr += gen_video_packet (ptr, voptr, avail);
	  copy (SECT_SIZE);
	}
	else
	{
	  // no video output data left - skip input sector
	  skip (SECT_SIZE);
	  skipped_video_packs++;
	}

	break;

      case 0xbd:
	// private 1: audio/subpicture
	copy_private_1 (ptr);
	break;

      case 0xc0:
      case 0xc1:
      case 0xc2:
      case 0xc3:
      case 0xc4:
      case 0xc5:
      case 0xc6:
      case 0xc7:
	// MPEG audio
	copy_mpeg_audio (ptr);
	break;

      case 0xbb:
	// system header/private 2: PCI/DSI
	copy (SECT_SIZE);
	break;

      case 0xbe:
	// padding
	data_length  = ptr [4] << 8;
	data_length |= ptr [5];

	if (14 + data_length != SECT_SIZE - 6)
	  fatal ("Bad padding packet length at %llu: %d",
		 rtell (ptr), data_length);

	break;

      case 0xbf:
	// private 2
	if (strcmp ((char *) (ptr + 6), "Vamps-data") != 0 || ptr [17] != 0x01)
	  fatal ("Misplaced private stream 2 pack at %llu", rtell (ptr));

	skip (SECT_SIZE);

	break;

      default:
	fatal ("Encountered stream ID %02x at %llu, "
	       "probably bad MPEG2 program stream", id, rtell (ptr));
    }

    if (wptr == wbuf + WBUF_SIZE)
      // end of write buffer reached --> flush it to disk
      flush ();
  }
}


// entry point from main()
// the requant thread already has been started
void
vaporize (void)
{
  int   seq_length;
  float fact = vap_fact;

  // process PS up to but not including first sequence header
  vap_leader ();

  // just in case - maybe should spit out a warning/error here
  if (v_eof)
    return;

  total_packs++;
  nav_packs++;
  total_packs++;
  video_packs++;

  // main loop
  while (1)
  {
    // do phase 1 of vaporization
    seq_length = vap_phase1 ();

    if (v_eof)
    {
      // EOF on source PS
      // process packs after and including last sequence header
      vap_trailer (seq_length);

      // only exit point from main loop
      return;
    }

    //fprintf (stderr, "seq_length=%d\n", seq_length);

    if (calc_ps_vap && vap_fact > 1.0f)
    {
      // forecast video ES vaporization factor
      // the basic formulars look like:
      // vap_fact  = total_packs/(restpacks+vop)
      // restpacks = total_packs-(video_packs+skipped_aux_packs+cell_gap_packs)
      // fact      = (video_packs*net-(gops*net/2+10))/(vop*net-(gops*net/2+10))
      // net       = SECT_SIZE-(14+9)
      // 14: pack header size
      // 9:  PES header size
      // 10: PTS+DTS size in PES header of sequence header
      // You are welcome to double check everything here!
      float vop, net;
      net  = (float) (SECT_SIZE - (14+9));
      vop  = video_packs + skipped_aux_packs + cell_gap_packs -
	     (float) total_packs * (1.0f-1.0f/vap_fact);
      fact = ((float) video_packs * net -
	      ((float) sequence_headers * net/2.0f + 10.0f)) /
	     (vop * net - ((float) sequence_headers * net/2.0f + 10.0f));

      // requant seems to get stuck on factors < 1
      if (fact < 1.0f)
	fact = 1.0f;

      if (verbose >= 2)
	fprintf (stderr, "Info: Target video ES vaporization factor: %.3f\n",
		 fact);
    }

    vin_bytes += vilen;

    if (fact > 1.0f)
    {
      // do requantization
      volen = requant (vobuf, vibuf, vilen, fact);
    }
    else
    {
      // don't do requantization
      memcpy (vobuf, vibuf, vilen);
      volen = vilen;
    }

    vout_bytes += volen;

    // do phase 2 of vaporization
    vap_phase2 (seq_length);

    //fprintf (stderr,
    //	       "tot=%d, vid=%d, ps1=%d, nav=%d, sv=%d, sp1=%d, fact=%.3f\n",
    //	       total_packs, video_packs, aux_packs, nav_packs,
    //	       skipped_video_packs, skipped_aux_packs, fact);
  }
}


// inject status from previous run into internal variables
void
read_injections (const char *filename)
{
  unsigned i;
  FILE    *fp;

  // lack of injections file is Ok
  // we will create one at end of this run
  if ((fp = fopen (filename, "r")) == NULL)
    return;

  for (i = 0; i < N_INJECTIONS; i++)
  {
    if (fscanf (fp, injections [i].f, injections [i].p) != 1)
      fatal ("Bad format of injections file: %s", filename);
  }

  fclose (fp);
}


// save status of internal variables to disk
void
write_injections (const char *filename)
{
  unsigned i;
  FILE    *fp;

  if ((fp = fopen (filename, "w")) == NULL)
    fatal ("Failed creating injections file: %s: %s",
	   filename, strerror (errno));

  for (i = 0; i < N_INJECTIONS; i++)
  {
    switch (injections [i].s)
    {
//      case 1:
//	fprintf (fp, injections [i].f, *((uint8_t *) injections [i].p));
//	break;
//
//      case 2:
//	fprintf (fp, injections [i].f, *((uint16_t *) injections [i].p));
//	break;
//
      case 4:
	fprintf (fp, injections [i].f, *((uint32_t *) injections [i].p));
	break;

      case 8:
	fprintf (fp, injections [i].f, *((uint64_t *) injections [i].p));
	break;

      default:
	abort ();
    }

    putc ('\n', fp);
  }

  if (fclose (fp))
    fatal ("Failed writing injections file: %s: %s");
}


// let's hope this helps
void
usage (void)
{
  fputs (
"Vamps " VERSION "\n"
"\n"
"Usage: vamps [--evaporate|-e factor] [--ps-evaporate|-E factor]\n"
"             [--audio|-a a-stream,a-stream,...]\n"
"             [--subpictures|-s s-stream,s-stream,...] [--verbose|-v]\n"
"             [--inject|-i injections-file]\n"
"             [--preserve|-p] [--ps-size|-S input-bytes]\n"
"\n"
"Vamps evaporates DVD compliant MPEG2 program streams by\n"
"selectively copying audio and subpicture streams and by re-quantizing\n"
"the embedded elementary video stream. The shrink factor may be either\n"
"specified for the video ES only (-e) or for the full PS (-E).\n",
         stderr);
  exit (1);
}


// this is a *very* sophisticated kind of error handling :-)
void
fatal (char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: Fatal: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  putc ('\n', stderr);
  exit (1);
}
