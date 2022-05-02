#ifndef _SUBTITLE2PPM_SPUDEC_H
#define _SUBTITLE2PPM_SPUDEC_H
#include <stdio.h>

// list of supported output formats
typedef enum{PGM, PPM, PGMGZ, PNG_GRAY,PNG_GRAY_ALPHA,PNG_RGBA, LAST_FORMAT} output_formats;
typedef enum{SRTX, DVDAUTHOR_XML,LAST_TAG_FORMAT} tag_formats;
typedef struct {
  unsigned char* packet;
  size_t packet_reserve;	    /* size of the memory pointed to by packet */
  int packet_offset;		    /* end of the currently assembled fragment */
  int packet_size;		    /* size of the packet once all
				       fragments are assembled */
  unsigned int packet_pts;	    /* PTS for this packet */
  int control_start;		    /* index of start of control data */
  int palette[4];
  unsigned int alpha[4];
  int cmap[4];
  int now_pts;
  int is_forced; 
  int start_pts, end_pts;
  int start_col;
  int end_col;
  int start_row;
  int end_row;
  int width, height, stride;
  int current_nibble[2];	    /* next data nibble (4 bits) to be
                                       processed (for RLE decoding) for
                                       even and odd lines */
  int deinterlace_oddness;	    /* 0 or 1, index into current_nibble */
  size_t image_size;		    /* Size of the image buffer */
  unsigned char *image;		    /* Grayscale values */
  unsigned char *aimage;            /* alpha channel */
  char ppm_base_name[FILENAME_MAX]; /* base filename of ppm images */
  char tag_base_name[FILENAME_MAX]; /* base file name fo tag file */
  unsigned int title_num;           /* counter for current subtitle
				       nummer */
  FILE* tag_file;
  tag_formats tag_format;           /* desired output format for tag
				       file */
  output_formats image_format;      /* write the image in this format
				     */
  /* start and end of the detected bounding box of the text */
  unsigned int bb_start_row;
  unsigned int bb_end_row;
  unsigned int bb_start_col;
  unsigned int bb_end_col;
  int crop_border_size;              

} spudec_handle_t;


void spudec_assemble(spudec_handle_t *this, unsigned char *packet, int len, int pts100);
spudec_handle_t * spudec_new(int color[4], char* ppm_base_name, 
			     char* tag_base_name, output_formats image_format,
			     tag_formats tag_format, int crop);
 void spudec_free(spudec_handle_t *this);
void spudec_reset(spudec_handle_t *this);	// called after seek
unsigned int spudec_get_title_num(spudec_handle_t* this);
#endif





