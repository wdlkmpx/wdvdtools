/*
  Convert a vobsub subtitle stream to pgm & srtx format

  Author: Jean-Yves Simon

  Copyright:

  Most of the code is stolen from
  mplayer (file vobsub.c) http://mplayer.dev.hu/homepage/news.html
  and transcode http://www.theorie.physik.uni-goettingen.de/~ostreich/transcode/
  so that the Copyright of the respective owner should be applied.
  (That means GPL.)
*/

/*
  TODO:
	add more options:
			vobsub_id using fr en, ... tags
			better verbose
	fix the segfault when vobsub_close() used

Revision:
2003-08-18 vobsub_close bug fixed in spudec.c / vobsub_close() now called before exit
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vobsub.h"
#include "spudec.h"

#define READ_BUF_SIZE (64*1024)

int vobsub_id;
int verbose;

static void usage(void)
{
    fprintf(stderr,"\n\t Convert vobsubs to pgm format\n\n");
    fprintf(stderr,"\t vobsub2pgm [options] baseinputfilename baseoutputfilename\n\n");
    fprintf(stderr,"\t -i <file_name.ifo> the ifo filename\n");
    fprintf(stderr,"\t                    default: inputfilename.ifo\n");
    fprintf(stderr,"\t -t <vobsub_id>     Set the used vobsub_id, default 0\n");
    fprintf(stderr,"\t -c <c0,c1,c2,c3>   override the default grey levels\n");
    fprintf(stderr,"\t                    default -c 0,255,255,255\n");
    fprintf(stderr,"\t -g <format>        Set output format to 0=PGM (default)\n");
    fprintf(stderr,"\t                    1=PPM, 2=PGM.GZ\n");
    fprintf(stderr,"\t -v                 verbose output\n");
    fprintf(stderr,"\t Version 0.2\n");
    exit(0);
}


int main(int argc, char* argv[])
{
    char ifofilename[FILENAME_MAX];
    output_formats image_format=0;
    int crop=-1;
    int colors[4];
    int ch,n; //getopt
    int format_arg;
    void *vobsub;
    int timestamp,len;
    void *data;
    spudec_handle_t *spudec_handle;

    //default options
    verbose=0;
    vobsub_id=0;

    ifofilename[0]=0;

    //default colors, should prolly be read from .ifo file
    colors[0]=0; //inside text
    colors[1]=255;
    colors[2]=255; // text outer limit
    colors[3]=255; // background

    if (argc < 3 )  usage();

    //scan input
    while((ch=getopt(argc,argv,"i:t:c:g:vh")) != -1)
    {
        switch(ch)
        {
            case 'i': //ifo filename
                n = sscanf(optarg,"%s", ifofilename);

                if (n!=1) {
                    fprintf(stderr,"no filename specified to option -i\n");
                    exit(1);
                }
                break;

            case 't': //vobsub_id number
                n = sscanf(optarg,"%d", &vobsub_id);
                if (n!=1) {
                    fprintf(stderr,"vobsub_id for -g option\n");
                    exit(1);
                }
                break;

            case 'c': //color palette
                n = sscanf(optarg,"%d,%d,%d,%d", &colors[0], &colors[1], &colors[2], &colors[3]);
                if (n<1 || n>4) {
                    fprintf(stderr,"invalid argument to color\n");
                    exit(-1);
                }
                break;

            case 'g': //gfx output format
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

            case 'v': //verbose
                verbose=1;
                break;

            case 'h': //help
                usage();
                break;

            default:
                fprintf(stderr,"Unknown option. Use -h to list all valid options.\n");
                exit(1);
        }
    }

    if ((argv[optind]==NULL) ||(argv[optind+1]==NULL)) usage();

    //default ifo filename
    if (ifofilename[0]==0) {
        strncpy(ifofilename,argv[optind],FILENAME_MAX-10);
        strcat(ifofilename,".ifo");
    }

    //let's open the vobsub inputstream
    vobsub=vobsub_open(argv[optind],ifofilename,1,NULL);
    if (vobsub==NULL) {
        fprintf(stderr,"Error opening vobsubs\n");
        exit(1);
    }

    //let's open the output stream
    spudec_handle = spudec_new(colors,argv[optind+1],argv[optind+1], image_format,SRTX, crop);
    //not needed
    //spudec_reset(spudec_handle);	

    //spudec_new_scaled to height & width?

    while(((len=vobsub_get_next_packet(vobsub,&data,&timestamp))!=-1))
    {
        spudec_assemble(spudec_handle,data,len,(int)timestamp/900.0);
        //should we check for next id?
    }

    // close stream
    vobsub_close(vobsub);

    spudec_free(spudec_handle);

    return 0;
}
