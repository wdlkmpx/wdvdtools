#ifndef _SUBTITLE2PGM_H_
#define _SUBTITLE2PGM_H_


// minimum version code for subtitle stream that 
// can be handled by the program
#define MIN_VERSION_CODE 0x00030000
// common part of the subtitle header struct for major version 3
typedef struct {
 
  unsigned int header_length;   
  unsigned int header_version;
  unsigned int payload_length;
  
  unsigned int lpts;
  double rpts;
 
} subtitle_header_v3_t;

#endif
