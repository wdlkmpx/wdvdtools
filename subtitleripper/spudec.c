/* SPUdec.c
   Skeleton of function spudec_process_controll() is from xine sources.
   Further works:
   LGB,... (yeah, try to improve it and insert your name here! ;-)

   Kim Minh Kaplan
   implement fragments reassembly, RLE decoding.
   read brightness from the IFO.


   Arne Driescher driescher@mpi-magdeburg.mpg.de
   Strip down to ppm export. 

   For information on SPU format see:
   <URL:http://sam.zoy.org/doc/dvd/subtitles/>
   <URL:http://members.aol.com/mpucoder/DVD/spu.html>


Revision:
   2002-10-25:
      Merging with CVS diff version 1.35 to 1.36 of supdec.c (mplayer CVS)
      because the lost subtitles problem should be fixed.

   SPUH = Sub-Picture Unit Header 
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "spudec.h"
#include <assert.h>
#include <string.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_LIB_PPM
#include <ppm.h>
#endif

#ifdef HAVE_PNG
#include "png.h"
#endif

static void spudec_handle_rest(spudec_handle_t *this);

// read a big endian 16 bit value
static inline unsigned int get_be16(const unsigned char *p)
{
	  return (p[0] << 8) + p[1];
}

// read a big endian 24 bit value
static inline unsigned int get_be24(const unsigned char *p)
{
    return (get_be16(p) << 8) + p[2];
}

static void next_line(spudec_handle_t *this)
{
    if (this->current_nibble[this->deinterlace_oddness] % 2)
        this->current_nibble[this->deinterlace_oddness]++;
    this->deinterlace_oddness = (this->deinterlace_oddness + 1) % 2;
}

static inline unsigned char get_nibble(spudec_handle_t *this)
{
    unsigned char nib;
    int *nibblep = this->current_nibble + this->deinterlace_oddness;
    if (*nibblep / 2 >= this->control_start) {
        fprintf(stderr,"SPUdec: ERROR: get_nibble past end of packet\n");
        return 0;
    }
    nib = this->packet[*nibblep / 2];
    if (*nibblep % 2)
        nib &= 0xf;
    else
        nib >>= 4;
    ++*nibblep;
    return nib;
}

static void spudec_process_data(spudec_handle_t *this)
{
    int i, x, y;

    if (this->image_size < this->stride * this->height) {
        if (this->image != NULL) {
            free(this->image);
            this->image_size = 0;
        }

        // allocate memory for image and alpha channel
        this->image = malloc(2 * this->stride * this->height);
        if (this->image) {
            this->image_size = this->stride * this->height;
            this->aimage = this->image + this->image_size;
        }
    }

    if (this->image == NULL) {
        fprintf(stderr,"this->image == NULL\n");
        return;
    }

    this->deinterlace_oddness = 0;
    i = this->current_nibble[1];
    x = 0;
    y = 0;
    while (this->current_nibble[0] < i
            && this->current_nibble[1] / 2 < this->control_start
            && y < this->height)
    {
        int len, color;
        unsigned int rle = 0;
        rle = get_nibble(this);
        if (rle < 0x04) {
            rle = (rle << 4) | get_nibble(this);
            if (rle < 0x10) {
                rle = (rle << 4) | get_nibble(this);
                if (rle < 0x040) {
                    rle = (rle << 4) | get_nibble(this);
                    if (rle < 0x0004)
                        rle |= ((this->width - x) << 2);
                }
            }
        }
        color = 3 - (rle & 0x3);
        len = rle >> 2;
        if (len > this->width - x || len == 0)
            len = this->width - x;

        assert( (color >=0) && (color <4));
        memset(this->image + y * this->stride + x, this->cmap[color], len);
        // assign alpha values. The original pallet contains alpha values
        // in the range 0x0..0xf that are here transformed to 0x0..0xff.
        memset(this->aimage + y * this->stride + x, this->alpha[color] << 4, len);

#ifdef DEBUG
        printf("setting color:%d (%d) for len:%d at (%d,%d)\n",
                color,this->palette[color],len,y,x);
#endif
        x += len;
        if (x >= this->width) {
            next_line(this);
            x = 0;
            ++y;
        }
    }
}


static void spudec_process_control(spudec_handle_t *this, int pts100)
{
    int a,b; /* Temporary vars */
    int date=0, type;
    int off;
    int start_off = 0;
    int next_off;

    this->control_start = get_be16(this->packet + 2);
    next_off = this->control_start;
    while (start_off != next_off) {
        start_off = next_off;
        // get delay to wait till the command should be executed.
        // The units are 90KHz clock divided by 1024.
        date = get_be16(this->packet + start_off);
        next_off = get_be16(this->packet + start_off + 2);
        // fprintf(stderr,"date=%d\n", date);
        off = start_off + 4;
        for (type = this->packet[off++]; type != 0xff; type = this->packet[off++]) {
            //fprintf(stderr,"cmd=%d  ",type);
            switch(type)
            {
            case 0x00:
            case 0x01:
                if ( this->end_pts==-1 ) {
                    this->end_pts = pts100 + date*(100*1024/90000.0);
                    spudec_handle_rest(this);
                }
                /* Start display, no arguments */
                //fprintf(stderr,"Start display:%d--%d!\n",pts100,date);
                this->is_forced = (type==0);
                this->start_pts = pts100 + date*(100*1024/90000.0);
                this->end_pts = -1;
                break;
            case 0x02:
                /* Stop display, no arguments */
                //fprintf(stderr,"Stop display:%d--%d!\n",pts100,date);
                this->end_pts = pts100 + date*(100*1024/90000.0);
                spudec_handle_rest(this);
                break;
            case 0x03:
                /* Set Color. Provides four indices into the color lookup table.
                One nibble addresses one color so that 2 bytes set all 4 colors.
                Format in stream: emph2 emph1 pattern background
                */
                this->palette[0] = this->packet[off] >> 4;      // emph2
                this->palette[1] = this->packet[off] & 0xf;     // emph1
                this->palette[2] = this->packet[off + 1] >> 4;  // pattern
                this->palette[3] = this->packet[off + 1] & 0xf; // background
                /*fprintf(stderr,"Palette %d, %d, %d, %d\n",
                this->palette[0], this->palette[1], this->palette[2], this->palette[3]);
                */
                off+=2;
                break;
            case 0x04:
                /* Alpha blendig:
                Directly provides the four contrast (alpha blend) values to associate with 
                the four pixel values. One nibble per pixel value for a total of 2 bytes. 
                0x0 = transparent, 0xF = opaque.
                Format in stream : emph2 emph1 pattern background
                */
                /* Alpha */
                this->alpha[0] = this->packet[off] >> 4;        // emph2    
                this->alpha[1] = this->packet[off] & 0xf;	// emph1    
                this->alpha[2] = this->packet[off + 1] >> 4;	// pattern  
                this->alpha[3] = this->packet[off + 1] & 0xf;	// background
                off+=2;
                break;
            case 0x05:
                /* DISPLAY AREA 
                Defines the display area, each pair (X and Y) of values is 3 bytes wide, 
                for a total of 6 bytes, and has the form 
                sx sx   sx ex   ex ex   sy sy   sy ey   ey ey 
                sx = starting X coordinate 
                ex = ending X coordinate 
                sy = starting Y coordinate 
                ey = ending Y coordinate 
                */
                a = get_be24(this->packet + off);
                b = get_be24(this->packet + off + 3);
                this->start_col = a >> 12;
                this->end_col = a & 0xfff;
                this->width = this->end_col - this->start_col + 1;
                this->stride = (this->width + 7) & ~7; /* Kludge: draw_alpha needs width multiple of 8 */
                this->start_row = b >> 12;
                this->end_row = b & 0xfff;
                this->height = this->end_row - this->start_row /* + 1 */;
                /*fprintf(stderr,"Coords  col: %d - %d  row: %d - %d  (%dx%d)\n",
                this->start_col, this->end_col, this->start_row, this->end_row,
                this->width, this->height);
                */
                off+=6;
                break;
            case 0x06:
                /*Graphic lines.
                Two 16 bit values that indicate the first and second 
                line of the subtitle image. (The subtitle image is
                interlaced so that.)
                Format in stream: line1 line2
                */
                this->current_nibble[0] = 2 * get_be16(this->packet + off);
                this->current_nibble[1] = 2 * get_be16(this->packet + off + 2);
                /*fprintf(stderr,"Graphic offset 1: %d  offset 2: %d\n",
                this->current_nibble[0] / 2, this->current_nibble[1] / 2);
                */
                off+=4;
                break;
            case 0xff:
                /* End command. */
                //fprintf(stderr,"Done!\n");
                return;
                break;
            default:
                fprintf(stderr,"spudec: Error determining control type 0x%02x.  Skipping %d bytes.\n",
                type, next_off - off);
                goto next_control;
            }
        }
  next_control:
        ;
    }
}


// write image in png format
static void spudec_writeout_png(spudec_handle_t *this)
{
#ifdef HAVE_PNG
    char file_name[FILENAME_MAX];
    unsigned int width  = this->bb_end_col-this->bb_start_col+1;
    unsigned int height = this->bb_end_row-this->bb_start_row+1;
    unsigned int stride = this->stride;
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep row_pointers[height];
    unsigned int k,j;
    volatile png_bytep output_image=NULL;
    unsigned int channels = 1;

    //fprintf(stderr,"Image size %d x %d\n",this->stride, this->height);
    assert(this->image);
    assert(this->aimage);

    // construct the file name for png image 
    sprintf(file_name,"%s%04d.png",this->ppm_base_name, this->title_num);

    // fprintf(stderr, "Writing to file %s\n",file_name);

    /* open the file */
    fp = fopen(file_name, "wb");
    if (fp == NULL) {
        fprintf(stderr,"Could not open file %s for writing PNG\n",file_name);
        return;
    }

    /* Create and initialize the png_struct with the desired error handler
    * functions.  If you want to use the default stderr and longjump method,
    * you can supply NULL for the last three parameters.  We also check that
    * the library version is compatible with the one used at compile time,
    * in case we are using dynamically linked libraries.  REQUIRED.
    */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL)
    {
        fclose(fp);
        fprintf(stderr,"png_create_write_struct() failed\n");
        return;
    }

    /* Allocate/initialize the image information data.  REQUIRED */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        fclose(fp);
        png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
        fprintf(stderr,"png_create_info_struct failed\n");
        return ;
    }

    /* Set error handling.  REQUIRED if you aren't supplying your own
     * error handling functions in the png_create_write_struct() call.
     */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        /* If we get here, we had a problem writing the file */
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fprintf(stderr,"PNG error handler called\n");
        if (output_image) {
            free(output_image);
        }
        return ;
    }

    /* set up the output control for using standard C streams */
    png_init_io(png_ptr, fp);

    /* Set the image information here.  Width and height are up to 2^31,
    * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
    * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
    * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
    * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
    * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
    * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
    */
    if (this->image_format == PNG_GRAY)
    {
        channels = 1; // only gray channel
        /* Write image in gray scale format without alpha channel */
        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_GRAY,
                    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        /* Construct an array with pointers to all rows of the image */
        for(k=this->bb_start_row,j=0; k <= this->bb_end_row; ++k,++j) {
            row_pointers[j] = this->image + k*stride + this->bb_start_col;
        }
    } else if (this->image_format == PNG_GRAY_ALPHA) {
        channels = 2; // gray and alpha channel
        /* Write image in gray scale format with alpha channel */
        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_GRAY_ALPHA,
                    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        //Convert the original gray scale image to grayscale + alpha.
        output_image = malloc(width*height*channels*sizeof(png_byte)); 
        /* Copy the gray and alpha values into the new image */
        for(k = 0; k < height; ++k) {
            for(j = 0; j < width; ++j) {
                const png_byte pixel = this->image[(this->bb_start_row+k)*stride+j+this->bb_start_col];
                const png_byte alpha = this->aimage[(this->bb_start_row+k)*stride+j+this->bb_start_col];
                output_image[k*width*channels+channels*j]   = pixel;
                output_image[k*width*channels+channels*j+1] = alpha;
            }
        }
        /* Construct an array with pointers to all rows of the image */
        for (k = 0; k < height; k++) {
            row_pointers[k] = output_image + k*width*channels;
        }
    } else if (this->image_format == PNG_RGBA) {
        channels = 4; // RGB+A channels
        /* Write image in gray scale format with alpha channel */
        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
                    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        // Convert the original gray scale image to RGB-A format.
        output_image = malloc(width*height*channels*sizeof(png_byte)); // 4 channels
        // Copy the gray and alpha values into the new image
        for(k = 0; k < height; ++k) {
            for(j = 0; j < width; ++j) {
                const png_byte pixel = this->image[(this->bb_start_row+k)*stride+j+this->bb_start_col];
                const png_byte alpha = this->aimage[(this->bb_start_row+k)*stride+j+this->bb_start_col];
                /* TODO: color lookup table needed */
                output_image[k*width*channels+channels*j]     = pixel; // red
                output_image[k*width*channels+channels*j+1]   = pixel; // green
                output_image[k*width*channels+channels*j+2]   = pixel; // blue
                output_image[k*width*channels+channels*j+3]   = alpha;
            }
        }

        /* Construct an array with pointers to all rows of the image */
        for (k = 0; k < height; k++) {
            row_pointers[k] = output_image + k*width*channels;
        }
    } else {
        fprintf(stderr,"Unsupported PNG output format\n");
        return ;
    }

    /* Write the file header information. */
    png_write_info(png_ptr, info_ptr);

    /* Write the full image */
    png_write_image(png_ptr, row_pointers);

    /* It is REQUIRED to call this to finish writing the rest of the file */
    png_write_end(png_ptr, info_ptr);
    /* clean up after the write, and free any memory allocated */
    png_destroy_write_struct(&png_ptr, &info_ptr);

    /* close the file */
    fclose(fp);

    /* Free allocated image if used */
    if (output_image) {
        free(output_image);
    }
#else
    fprintf(stderr,"PNG not supported. Recompile with HAVE_PNG to activate this feature\n");
#endif
}

// write image in PGM format (grayscale)
static void spudec_writeout_pgm(spudec_handle_t *this)
{
    int i,j;
    unsigned char c;
    char file_name[FILENAME_MAX];
    FILE* out_file;
    unsigned int width;
    unsigned int height;

    // calculate width and hight of the image
    width = this->bb_end_col-this->bb_start_col+1,
    height= this->bb_end_row-this->bb_start_row+1;
  
    //fprintf(stderr,"Image size %d x %d\n",this->stride, this->height);
    assert(this->image);

    if (this->image_format == PGMGZ) {
#ifdef HAVE_ZLIB
        gzFile gz_out_file;
        // construct the file name for pgm image with gzip
        sprintf(file_name,"%s%04d.pgm.gz",this->ppm_base_name, this->title_num);
        gz_out_file = gzopen(file_name,"wb");
        if (!gz_out_file) {
            perror("Open for compressed file");
            exit(1);
        }
        gzprintf(gz_out_file, "P5\n# CREATORE: subtitle2pgm\n %d %d\n255\n",width, height);
        // copy the spu-image to pgm
        for(j=this->bb_start_row; j <= this->bb_end_row; j++) {
            for(i=this->bb_start_col ; i <= this->bb_end_col; i++) {
                c=*(this->image+j*(this->stride)+i);
                gzputc(gz_out_file,c);
            }
        }
        gzclose(gz_out_file);
#endif
    } else {

        // construct the file name for pgm image without gzip
        sprintf(file_name,"%s%04d.pgm",this->ppm_base_name, this->title_num);

        // fprintf(stderr, "Writing to file %s\n",file_name);
        out_file=fopen(file_name,"w");

        // write PGM header
        fprintf(out_file,"P5\n# CREATORE: subtitle2pgm\n %d %d\n255\n",width, height);
        // copy the spu-image to pgm
        for(j=this->bb_start_row; j <= this->bb_end_row; j++) {
            for(i=this->bb_start_col ; i <= this->bb_end_col; i++) {
                c=*(this->image+j*(this->stride)+i);
                fputc(c,out_file);
            }
        }
        fclose(out_file);
    } 
}


// write image in PPM format
static void spudec_writeout_ppm(spudec_handle_t* this)
{
#ifdef HAVE_LIB_PPM
    int i,j,k,l;
    pixel** ppm_image;
    pixel p;
    unsigned char c;
    char file_name[FILENAME_MAX];
    FILE* out_file;
    unsigned int width;
    unsigned int height;

    // calculate width and hight of the image
    width = this->bb_end_col-this->bb_start_col+1,
    height= this->bb_end_row-this->bb_start_row+1;

    /*  
    fprintf(stderr,"### Image number %d ###\n",this->title_num);
    fprintf(stderr,"Image width=%d\n",this->width);
    fprintf(stderr,"Image height=%d\n",this->height);
    fprintf(stderr,"Image stride=%d\n",this->stride);
    fprintf(stderr,"start_pts=%d\n",this->start_pts);
    fprintf(stderr,"end_pts=%d\n",this->end_pts);
    fprintf(stderr,"start_col=%d\n",this->start_col);
    fprintf(stderr,"start_row=%d\n",this->start_row);
    fprintf(stderr,"end_col=%d\n",this->end_col);
    fprintf(stderr,"end_row=%d\n",this->end_row);
    */

    // alloc an new ppm array
    ppm_image =  ppm_allocarray(width, height);  

    //fprintf(stderr,"Image size %d x %d\n",this->stride, this->height);
    assert(this->image);

    // copy the spu-image to ppm
    for(j=this->bb_start_row,k=0; j <= this->bb_end_row; j++,k++) {
        for(i=this->bb_start_col,l=0 ; i <= this->bb_end_col; i++,l++) {
            c=*(this->image+j*(this->stride)+i);
            PPM_ASSIGN(p,c,c,c); 
            ppm_image[k][l]=p;
        }
    }
  
    // construct the file name for ppm image
    sprintf(file_name,"%s%04d.ppm",this->ppm_base_name, this->title_num);
    // fprintf(stderr, "Writing to file %s\n",file_name);
    out_file=fopen(file_name,"w");

    assert(out_file);

    ppm_writeppm(out_file, ppm_image, width, height, 255, 0 /*forceplane*/);

    fclose(out_file);
    ppm_freearray(ppm_image, this->height);  
#else
    fprintf(stderr,"PPM not supported. Please recompile with -DHAVE_LIB_PPM\n");
    exit(1);
#endif  
}


static void spudec_writeout_srtx_tag(spudec_handle_t *this)
{
    int    start_sec, end_sec;
    int    start_min, end_min;
    int    start_hour, end_hour;
    double start_pts=this->start_pts/100.0;
    double  end_pts=this->end_pts/100.0;
    int start_subsec, end_subsec;

    // writeout current number
    fprintf(this->tag_file,"%d\n",this->title_num);

    // writeout start and end time of this title
    start_hour = start_pts/(3600);
    start_min  = (start_pts-3600*start_hour)/60;
    start_sec  = (start_pts-3600*start_hour-60*start_min);
    start_subsec = (start_pts-3600*start_hour-60*start_min-start_sec)*1000;

    end_hour = end_pts/(3600);
    end_min  = (end_pts-3600*end_hour)/60;
    end_sec  = (end_pts-3600*end_hour-60*end_min);
    end_subsec = (end_pts-3600*end_hour-60*end_min-end_sec)*1000;

    fprintf(this->tag_file,"%02d:%02d:%02d,%.3d --> %02d:%02d:%02d,%.3d\n",
            start_hour,start_min,start_sec,start_subsec,
            end_hour,end_min,end_sec,end_subsec);
    switch(this->image_format)
    {
        case PGM:
            fprintf(this->tag_file,"%s%04d.pgm.txt\n\n",
                    this->ppm_base_name, 
                    this->title_num);
            break;
        case PGMGZ:
            fprintf(this->tag_file,"%s%04d.pgm.gz.txt\n\n",
            this->ppm_base_name, 
            this->title_num);
            break;
        case PPM:
            fprintf(this->tag_file,"%s%04d.ppm.txt\n\n",
            this->ppm_base_name, 
            this->title_num);
            break;
        case PNG_GRAY:
        case PNG_GRAY_ALPHA:
        case PNG_RGBA:
            fprintf(this->tag_file,"%s%04d.png.txt\n\n",
                    this->ppm_base_name, 
                    this->title_num);
            break;
        default:
            fprintf(stderr,"Invalid output format\n");
            exit(1);
    }
}


static void spudec_writeout_dvdxml_tag(spudec_handle_t *this)
{
    int    start_sec, end_sec;
    int    start_min, end_min;
    int    start_hour, end_hour;
    double start_pts=this->start_pts/100.0;
    double  end_pts=this->end_pts/100.0;
    int start_subsec, end_subsec;

    // writeout start and end time of this title
    start_hour = start_pts/(3600);
    start_min  = (start_pts-3600*start_hour)/60;
    start_sec  = (start_pts-3600*start_hour-60*start_min);
    start_subsec = (start_pts-3600*start_hour-60*start_min-start_sec)*1000;

    end_hour = end_pts/(3600);
    end_min  = (end_pts-3600*end_hour)/60;
    end_sec  = (end_pts-3600*end_hour-60*end_min);
    end_subsec = (end_pts-3600*end_hour-60*end_min-end_sec)*1000;

    if (end_pts <= start_pts) {
        fprintf(stderr,"end_pts <= start_pts! (You might have a problem with this file)\n");
        fprintf(stderr,"Line:");
        fprintf(stderr," start=\"%02d:%02d:%02d.%.3d\" end=\"%02d:%02d:%02d.%.3d\" \n",
                start_hour,start_min,start_sec,start_subsec,
                end_hour,end_min,end_sec,end_subsec);
    }

    fprintf(this->tag_file,"    <spu start=\"%02d:%02d:%02d.%.3d\" end=\"%02d:%02d:%02d.%.3d\" ",
            start_hour,start_min,start_sec,start_subsec,
            end_hour,end_min,end_sec,end_subsec);
    if ( this->is_forced )
        fprintf(this->tag_file,"force=\"1\" ");
    switch(this->image_format)
    {
        case PGM:
            fprintf(this->tag_file,"image=\"%s%04d.pgm\"",
                    this->ppm_base_name, this->title_num);
            break;
        case PGMGZ:
            fprintf(this->tag_file,"image=\"%s%04d.pgm.gz\"",
                    this->ppm_base_name, this->title_num);
            break;
        case PPM:
            fprintf(this->tag_file,"image=\"%s%04d.ppm\"",
                    this->ppm_base_name, this->title_num);
            break;
        case PNG_GRAY:
        case PNG_GRAY_ALPHA:
        case PNG_RGBA:
            fprintf(this->tag_file,"image=\"%s%04d.png\"",
                    this->ppm_base_name, this->title_num);
            break;
        default:
            fprintf(stderr,"Invalid output format\n");
            exit(1);
    }

    // close the current tag
    fprintf(this->tag_file,"> </spu>\n");
}


// find a bounding box for the text and
// add an border of "crop_border_size" pixels to the box 
static void  spudec_crop_image(spudec_handle_t * this)
{
    unsigned int i,j;
    unsigned char pixel,alpha;
    unsigned int start_row;
    unsigned int end_row;
    unsigned int start_col;
    unsigned int end_col;
    unsigned char background_color;
    unsigned char background_alpha;
    int border = this->crop_border_size;

    // only crop if border is >= 0 (see -C flag)
    if (border < 0 ) {
        // set begin and end to the full image size
        this->bb_start_row = 0;
        this->bb_end_row   = this->height-1;
        this->bb_start_col = 0;
        this->bb_end_col   = this->width-1;
        return;
    }

    // start values are at the opposite edge of the image
    start_row = this->height-1;
    end_row   = 0;
    start_col = this->width-1;
    end_col   = 0;

    // assume that the upper left pixel is always the background color
    background_color = this->image[0];
    background_alpha = this->aimage[0];

    // Loop over all pixel. Every time we find a
    // pixel not equal to the background color the bounding box
    // is enlarged (if necessary) to include this pixel too.
    for(j=0; j < this->height; j++) {
        for(i=0;i<this->width;i++) {
            pixel=*(this->image+j*(this->stride)+i);
            alpha=*(this->aimage+j*(this->stride)+i);
            if (pixel != background_color || alpha != background_alpha) {
                // adjust the line start/end if necessary
                if (i<start_col) start_col=i;
                if (i>end_col) end_col=i;
                // adjust the row start/end if necessary
                if (j<start_row) start_row=j;
                if (j>end_row) end_row=j;
            }
        }
    }

    // sanity check for start and end (some subtitles are empty and crop will fail)
    if ( (end_row >=start_row) && (end_col >= start_col) ) {
        // make sure the requested border around the text is kept
        // and assigne the appropriate values to the bounding box
        this->bb_start_row = start_row > border ? start_row - border : 0;
        this->bb_end_row = end_row + border < this->height ? end_row + border : this->height-1;
        this->bb_start_col = start_col > border ? start_col - border : 0;
        this->bb_end_col = end_col + border < this->width ? end_col + border : this->width-1;
    } else {
        // set begin and end to the full image size for empty images
        fprintf(stderr,"warning: Empty subtitle, cannot crop image\n");
        this->bb_start_row = 0;
        this->bb_end_row   = this->height-1;
        this->bb_start_col = 0;
        this->bb_end_col   = this->width-1;
    }
}


// processes a complete packet
static void spudec_decode(spudec_handle_t *this, int pts100)
{
    spudec_process_control(this, pts100);
}


static void spudec_handle_rest(spudec_handle_t *this)
{
    spudec_process_data(this);
    // grap only the text segment from the image
    spudec_crop_image(this);
    // write out the image file
    switch(this->image_format)
    {
        case PPM:
            spudec_writeout_ppm(this);
            break;
        case PGM:
        case PGMGZ:
            spudec_writeout_pgm(this);
            break;
        case PNG_GRAY:
        case PNG_GRAY_ALPHA:
        case PNG_RGBA:
            spudec_writeout_png(this);
            break;
        default:
            fprintf(stderr,"Unknown image format selected\n");
            exit(1);
    }

    switch(this->tag_format)
    {
        case SRTX:
            spudec_writeout_srtx_tag(this);
            break;
        case DVDAUTHOR_XML:
            spudec_writeout_dvdxml_tag(this);
            break;
        default:
            fprintf(stderr,"Unknown tag file format\n");
    }
    // next tile
    this->title_num++;
}


void spudec_assemble(spudec_handle_t *this, unsigned char *packet, int len, int pts100)
{
    if (len < 2) {
        fprintf(stderr,"SPUasm: packet too short\n");
        return;
    }
    if ((this->packet_pts + 10000) < pts100) {
        // [cb] too long since last fragment: force new packet
        this->packet_offset = 0;
    }
    this->packet_pts = pts100;  

    // start of a new packet ?
    if (this->packet_offset == 0) {
        // read packet length
        unsigned int len2 = get_be16(packet);
        // make sure we have enought memory allocated for this packet
        if (this->packet_reserve < len2) {
            // free old allocated memory
            free(this->packet);
            // allocate new memory
            this->packet = malloc(len2);
            assert(this->packet); // debug code
            // save available memory size
            this->packet_reserve = this->packet != NULL ? len2 : 0;
        }
        // copy the data to the allocated memory
        if (this->packet != NULL) {
            this->packet_size = len2;
            memcpy(this->packet, packet, len);
            this->packet_offset = len;
        }
    } else {
        /* continue current fragment */
        // the size of the new fragment should be <= what was given in the first fragment header
        if (this->packet_size < this->packet_offset + len) {
            fprintf(stderr,"SPUasm: invalid fragment in %d\n", this->title_num);
            // reset size and offset to recover from the error
            this->packet_size = 0;
            this->packet_offset = 0;
        } else {
            // a correct fragment arrived, just copy it to the buffer
            memcpy(this->packet + this->packet_offset, packet, len);
            this->packet_offset += len;
        }
    }

    assert(this->packet_offset <= this->packet_size);   // debug 

#if 1
    /* check if we have a complete packet 
        (unfortunatelly packet_size is bad for some disks)
        [cb] packet_size is padded to be even -> may be one byte too long
    */
    if ( (this->packet_offset == this->packet_size) ||
       ((this->packet_offset +1) == this->packet_size))
    {
        int x=0,y;
        while(x>=0 && x+4<=this->packet_offset) {
            // next control pointer
            y=get_be16(this->packet+x+2);
            /* fprintf(stderr,"SPUtest: x=%d y=%d off=%d size=%d\n",
                x,y,this->packet_offset,this->packet_size);
            */
            // if it points to self - we're done!
            if (x>=4 && x==y) {		
                // we got it!
                //fprintf(stderr,"SPUgot: off=%d  size=%d \n",this->packet_offset,this->packet_size);
                spudec_decode(this, pts100);
                this->packet_offset = 0;
                break;
            }
            if (y<=x || y>=this->packet_size) { // invalid?
                fprintf(stderr,"SPUtest: broken packet!!!!! y=%d < x=%d in %d\n",y,x,
                this->title_num);
                this->packet_size = this->packet_offset = 0;
                break;
            }
            x=y;
        }
        // [cb] packet is done; start new packet
        this->packet_offset = 0;
    }
#else
    if (this->packet_offset == this->packet_size) {
        spudec_decode(this, pts100);
        this->packet_offset = 0;
    }
#endif
}


// change the current color settings
void spudec_set_color(spudec_handle_t* this, int color[4])
{
    unsigned int i;
    // initialize colors  
    for(i=0;i<4;++i) {
        this->cmap[i]=color[i];
    }
}


void spudec_reset(spudec_handle_t *this)	// called after seek
{
    this->now_pts = -1;
    this->packet_size = this->packet_offset = 0;
}


// constructor
// color[4]          pixel values to use for subtitle color 0-3
// ppm_base_name     base filename for ppm files
// tag_base_name     base filename for tag file
spudec_handle_t * spudec_new(int color[4], char* ppm_base_name, 
                             char* tag_base_name, output_formats format,
                             tag_formats tag_format, int crop)
{
    char tag_filename[FILENAME_MAX];
    spudec_handle_t *this = calloc(1, sizeof(spudec_handle_t));

    if (!this) {
        perror("FATAL: spudec_init: calloc");
        exit(1);
    }

    // set how many pixel should be kept around the text
    this->crop_border_size = crop;

    // initialize the color settings
    spudec_set_color(this,color);
    // save base filenames
    strncpy(this->ppm_base_name,ppm_base_name,FILENAME_MAX-10);
    strncpy(this->tag_base_name,tag_base_name,FILENAME_MAX-10);
    // reset counter 
    this->title_num=1;
    // open the tag file for writing
    switch(tag_format) {
        case DVDAUTHOR_XML:
            sprintf(tag_filename,"%s.dvdxml",tag_base_name);
            break;
        case SRTX:
        default:
            sprintf(tag_filename,"%s.srtx",tag_base_name);
            break;
    }

    this->tag_file = fopen(tag_filename,"w");

    if (!(this->tag_file)) {
        perror("Cannot open tagfile");
        exit(1);
    }
    // set the output format 
    this->image_format = format;
    // save the tag format
    this->tag_format = tag_format;
    // write file header
    if (DVDAUTHOR_XML == tag_format) {
        fprintf(this->tag_file,"<subpictures>\n  <stream>\n");
    }
    return this;
}


// destructor
void spudec_free(spudec_handle_t* this)
{
    if (this) {
        if (this->packet)
            free(this->packet);
        if (this->image)
            free(this->image);
        if (this->tag_file) {
            // write file footer
            if (DVDAUTHOR_XML == this->tag_format) {
                fprintf(this->tag_file,"  </stream>\n</subpictures>\n");
            }
            fclose(this->tag_file);
        }
        free(this);
    }
}


// return the current title number
// (equal to the number of written subtitles)
unsigned int spudec_get_title_num(spudec_handle_t* this)
{
    return this->title_num;
}
