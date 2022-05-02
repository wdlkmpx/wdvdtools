/*
  
  Convert a subtitle stream to pgm/ppm images
  ( Meanwhile it also supports png )

  Author: Arne Driescher

  Copyright:

  Most of the code is stolen from
  mplayer (file spudec.c) http://mplayer.dev.hu/homepage/news.html 
  and transcode http://www.theorie.physik.uni-goettingen.de/~ostreich/transcode/
  so that the Copyright of the respective owner should be applied.
  (That means GPL.)
  
  Version: 0.02
*/

#include "spudec.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>     
#include "subtitle2pgm.h"
#include <string.h>
#include <unistd.h>  

#ifdef HAVE_LIB_PPM
#include <ppm.h>
#endif

#define READ_BUF_SIZE (64*1024)

// get the major version number from the version code
unsigned int major_version(unsigned int version)
{
    // bit 16-31 contain the major version number
    return version >> 16;
}

unsigned int minor_version(unsigned int version)
{
    // bit 0-15 contain the minor version number
    return version & 0xffff;
}

void usage(void)
{
    fprintf(stderr,"\n\t Convert a subtitle stream to pgm images \n\n");
    fprintf(stderr,"\t subtitle2pgm [options]\n");
    fprintf(stderr,"\t subtitle2pgm expects a subtitle stream as input.\n");
    fprintf(stderr,"\t Use\n\t\t tcextract -x ps1 -t vob -a 0x20 -i file.vob\n");
    fprintf(stderr,"\t to make a subtitle stream from a VOB and pipe its output\n"); 
    fprintf(stderr,"\t directly into subtitle2pgm.\n");
    fprintf(stderr,"\t Options:\n");
    fprintf(stderr,"\t  -i <file_name>   Use file_name for input instead of stdin.\n");
    fprintf(stderr,"\t  -o <base_name>   Use base_name for output files\n");
    fprintf(stderr,"\t                   default: movie_subtitle\n");
    fprintf(stderr,"\t  -c <c0,c1,c2,c3> Override the default grey levels.\n");
    fprintf(stderr,"\t                   default: -c 255,255,0,255\n");
    fprintf(stderr,"\t                   Valid values: 0<=c<=255\n");
    fprintf(stderr,"\t  -g <format>      Set output format to 0=PGM (default),\n");
    fprintf(stderr,"\t                   1=PPM, 2=PGM.GZ, 3=PNG_GRAY, 4=PNG_GRAY_ALPH, 5=PNG_RGBA \n");
    fprintf(stderr,"\t  -t <format>      Set tag file format 0=srtx (default) 1=dvdxml\n");
    fprintf(stderr,"\t  -l <seconds>     Add <seconds> to PTS for every DVD-9 layer skip\n"); 
    fprintf(stderr,"\t                   (default 0.0)\n");
    fprintf(stderr,"\t  -C <boarder>     Crop image but keep <boarder> pixels if possible\n");
    fprintf(stderr,"\t                   (default is < 0 = don't crop at all)\n");
    fprintf(stderr,"\t  -e <hh:mm:ss,n>  extract only n subtitles starting from hh:mm:ss\n");
    fprintf(stderr,"\t  -v               verbose output\n");
    fprintf(stderr,"\t  -P               progress output\n");
    fprintf(stderr,"\t Version 0.3 (alpha) for >transcode-0.6.0\n");

    exit(0);
}


// magic string (definition must match the one in transcode/import/extract_ac3.c)
static char *subtitle_header_str="SUBTITLE";
static unsigned int verbose=0;


int main(int argc, char** argv)
{
    void *spudec_handle;
    int len;
    char read_buf[READ_BUF_SIZE];
    char output_base_name[FILENAME_MAX];
    char input_file_name[FILENAME_MAX];
    char filename_extension[FILENAME_MAX];

    subtitle_header_v3_t subtitle_header;
    int ch,n;
    int colors[4];
    output_formats image_format;
    tag_formats tag_format;
    int format_arg;
    int skip_len;
    double pts_seconds=0.0;
    unsigned int show_version_mismatch=~0;
    double layer_skip_adjust=0.0;
    double layer_skip_offset=0.0;
    unsigned int discont_ctr=0;
    int crop = -1;  // default cropping = don't crop

    unsigned int extract_start_hour=0;
    unsigned int extract_start_min=0;
    unsigned int extract_start_sec=0;
    unsigned int extract_number=~0;

    unsigned int progress_output=0;
    unsigned int last_subtitle_number=0;

#ifdef HAVE_LIB_PPM
    // initialize libppm
    ppm_init(&argc, argv);
#endif

    /* initialize default values here that can be overriden by commandline arguments */

    // base filename used for output
    strcpy(output_base_name,"movie_subtitle");

    // color table for subtitle colors 0-3 
    colors[0]=255;
    colors[1]=255;
    colors[2]=0;
    colors[3]=255;

    // default output format
    image_format=PGM;

    // default tag format
    tag_format=SRTX;

    // default filename extension 
    strcpy(filename_extension,"pgm");

    /* scan command line arguments */
    opterr=0;
    while ((ch = getopt(argc, argv, "e:i:g:t:c:C:o:l:hvP")) != -1)
    {
        switch (ch)
        {
            // color palett
            case 'c':
                n = sscanf(optarg,"%d,%d,%d,%d", &colors[0], &colors[1], &colors[2], &colors[3]);
                if (n<1 || n>4) {
                    fprintf(stderr,"invalid argument to color\n");
                    exit(-1);
                }
                break;

            case 'C':
                n = sscanf(optarg,"%d", &crop);
                if (n != 1) {
                    fprintf(stderr,"invalid argument to crop\n");
                    exit(-1);
                }
                break;

            case 'o':
                n = sscanf(optarg,"%s", output_base_name);
                if (n!=1) {
                    fprintf(stderr,"no filename specified to option -o\n");
                    exit(1);
                }
                break;

            case 'i':
                n = sscanf(optarg,"%s", input_file_name);
                if (n!=1) {
                    fprintf(stderr,"no filename specified to option -i\n");
                    exit(1);
                }
                // open the specified input file for reading
                if ( !(freopen(input_file_name,"r",stdin)) ) {
                    perror("stdin redirection");
                    fprintf(stderr,"Tried to open %s for input\n",input_file_name);
                    exit(1);
                }
                break;

            case 'h':
                usage();
                break;

            case 'v':
                verbose=~0;
                break;

            case 'P':
                progress_output=~0;
                break;

            case 'g':
                n = sscanf(optarg,"%d", &format_arg);
                if (n!=1) {
                    fprintf(stderr,"image format omitted for -g option\n");
                    exit(1);
                }
                if ( (format_arg<0) || (format_arg >=(int)LAST_FORMAT) ) {
                    fprintf(stderr,"Unknown image format selected for -g.\n");
                    fprintf(stderr,"Valid are 0, %d\n", (int)LAST_FORMAT);
                    exit(1);
                }
                image_format = (output_formats) format_arg;
 #ifndef HAVE_ZLIB
                if (image_format == PGMGZ) {
                    fprintf(stderr,"Cannot use compressed format. Hint: Recompile with -DHAVE_ZLIB\n");
                    exit(1);
                }
#endif
                break;

            case 't':
                n = sscanf(optarg,"%d", &format_arg);
                if (n!=1) {
                    fprintf(stderr,"tag format omitted for -t option\n");
                    exit(1);
                }
                if ( (format_arg <0) || (format_arg >= (int) LAST_TAG_FORMAT) ) {
                    fprintf(stderr,"Unknown tag format selected for -t.\n");
                    fprintf(stderr,"Valid are 0, %d\n", (int)LAST_TAG_FORMAT);
                    exit(1);
                }
                tag_format = (tag_formats) format_arg;
                break;

            case 'l':
                n = sscanf(optarg,"%lf", &layer_skip_offset);
                if (n!=1) {
                    fprintf(stderr,"Invalid time argument for -l option\n");
                    exit(1);
                }
                break;

            case 'e':
                n = sscanf(optarg,"%d:%d:%d,%d", 
                    &extract_start_hour, &extract_start_min, &extract_start_sec, &extract_number);
                if (n!=4) {
                    fprintf(stderr,"Invalid number of parameters for option -e\n");
                    exit(1);
                }
                if (verbose) {
                    fprintf(stderr,"Extracting %d subtitles starting from %02d:%02d:%02d\n",
                    extract_number, extract_start_hour, extract_start_min, extract_start_sec);
                }
                break;

            default:
                fprintf(stderr,"Unknown option. Use -h for list of valid options.\n");
                exit(1);
        }
    }
  
    switch(image_format)
    {
        case PGM:
            strcpy(filename_extension,"pgm");
            break;
        case PPM:
            strcpy(filename_extension,"ppm");
            break;
        case PGMGZ:
            strcpy(filename_extension,"pgm.gz");
            break;
        case PNG_GRAY:
        case PNG_GRAY_ALPHA:
        case PNG_RGBA:
            strcpy(filename_extension,"png");
            break;
        default:
            fprintf(stderr,"Unknown output file format\n");
            exit(1);
    }

    // allocate the data struct used by the decoder
    spudec_handle = spudec_new(colors, output_base_name, output_base_name, image_format, tag_format, crop);

    assert(spudec_handle);

    // reset to defaults 
    spudec_reset(spudec_handle);

    // process all packages in the stream
    // the stream is an "augmented" raw subtitle stream
    // where two additional headers are used.
    while (1)
    {
        // read the magic "SUBTITLE" identified
        len=fread(read_buf, strlen(subtitle_header_str),1, stdin);
        if (feof(stdin)) {
            break;
        }
        if (len != 1) {
            fprintf(stderr,"Could not read magic header %s\n", subtitle_header_str);
            perror("Magic header");
            exit(1);
        }
        if (strncmp(read_buf,subtitle_header_str,strlen(subtitle_header_str))) {
            fprintf(stderr,"Header %s not found\n",subtitle_header_str);
            fprintf(stderr,"%s\n",read_buf);
            exit(1);
        }
        // read the real header
        len=fread(&subtitle_header, sizeof(subtitle_header_v3_t), 1, stdin);
        if (len != 1) {
            fprintf(stderr,"Could not read subtitle header\n");
            perror("Subtitle header");
            exit(1);
        }
        // check for version mismatch and warn the user
        if ( (subtitle_header.header_version < MIN_VERSION_CODE) && show_version_mismatch) {
            fprintf(stderr,"Warning: subtitle2pgm was compiled for header version %u.%u"
                    " and the stream was produced with version %u.%u.\n",
                    major_version(MIN_VERSION_CODE), 
                    minor_version(MIN_VERSION_CODE),
                    major_version(subtitle_header.header_version),
                    minor_version(subtitle_header.header_version));
                    // don't show this message again
                    show_version_mismatch=0;
        }
        // we only try to proceed if the major versions match
        if ( major_version(subtitle_header.header_version) != major_version(MIN_VERSION_CODE) ) {
            fprintf(stderr,"Versions are not compatible. Please extract subtitle stream\n"
                    " with a newer transcode version\n");
            exit(1);
        }
        // calculate exessive header bytes
        skip_len = subtitle_header.header_length - sizeof(subtitle_header_v3_t);
        // handle versions mismatch
        if (skip_len) {
            // header size can only grow (unless something nasty happend)
            assert(skip_len > 0);
            // put the rest of the header into read buffer
            len = fread(read_buf, sizeof(char), skip_len, stdin);
            if (len != skip_len) {
                perror("Header skip:");
                exit(1);
            }
        }
        /* depending on the minor version some additional information might
        be available. */
        // since version 3.1 discont_ctr is available but works only sine 4-Mar-2002. Allow extra
        // adjustment if requested.
        if (minor_version(subtitle_header.header_version) > 0) {
            discont_ctr=*((unsigned int*) read_buf);
            layer_skip_adjust = discont_ctr*layer_skip_offset;
        }

#ifdef DEBUG
        fprintf(stderr,"subtitle_header: length=%d version=%0x lpts=%u (%f), rpts=%f, payload=%d, discont=%d\n",
                subtitle_header.header_length,
                subtitle_header.header_version,
                subtitle_header.lpts,
                (double)(subtitle_header.lpts/300)/90000.0,
                subtitle_header.rpts,
                subtitle_header.payload_length,
                discont_ctr);
#endif

        // read one byte subtitle stream id (should match the number given to tcextract -a)
        len=fread(read_buf, sizeof(char), 1, stdin);
        if (len != 1) {
            perror("stream id");
            exit(1);
        }
        // debug output
        //fprintf(stderr,"stream id: %x \n",(int)*read_buf);

        // read numer of bytes given in header
        len = fread(read_buf, subtitle_header.payload_length-1, 1, stdin);
    
        if (len >0) {
            if (subtitle_header.rpts > 0) {
                // if rpts is something useful take it
                pts_seconds=subtitle_header.rpts;
            } else {
                // calculate the time from lpts
                if (verbose) {
                    fprintf(stderr, "fallback to lpts!\n");
                }
                pts_seconds = (double)(subtitle_header.lpts/300)/90000.0;
            }

            // add offset for layer skip
            pts_seconds += layer_skip_adjust;

            if (pts_seconds < extract_start_hour*3600+extract_start_min*60+extract_start_sec) {
                // ignore all subtitle till we reached the start time
                continue;
            }

            // output progress report if requested by -P option
            if (last_subtitle_number != spudec_get_title_num(spudec_handle) && progress_output) {
                last_subtitle_number = spudec_get_title_num(spudec_handle);
                fprintf(stderr,"Generating image: %s%04d.%s\n",output_base_name, 
                        last_subtitle_number, filename_extension);
            }
            // do the actual work of assembling and decoding the data package
            spudec_assemble(spudec_handle,read_buf, subtitle_header.payload_length-1, 
                            (int) (pts_seconds*100));
            // check if the number of requested subtitles
            // is already reached. (The internal subtitle number
            // counter starts with 1.)
            if (spudec_get_title_num(spudec_handle) == (extract_number+1)) {
                // exit while loop
                break;
            }
        } else {
            perror("Input file processing finished");
            exit(errno);
        }
    }
  
    if (verbose) {
        fprintf(stderr,"Wrote %d subtitles\n",
        spudec_get_title_num(spudec_handle)-1);
        fprintf(stderr,"Conversion finished\n");
    }
    spudec_free((void *) spudec_handle);

    return 0;
}
