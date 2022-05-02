/*
  
  Various manipulations of srtfiles

  Author: Arne Driescher

  Copyright: GPL
 
  Version: 0.03
*/

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>     
#include <string.h>
#include <unistd.h> 
#include <math.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_TEXT_LEN 4096
typedef struct {
    unsigned int hour;
    unsigned int min;
    unsigned int sec;
    unsigned int msec;
} srt_time_t;

typedef struct{
    unsigned int number;
    srt_time_t start_time;
    srt_time_t end_time;
    char text[MAX_TEXT_LEN];
} srt_tag_t;

// global vars
static int verbose = 0;


static void usage(void)
{
    fprintf(stderr,"\t srttool [-v] [options]\n");
    fprintf(stderr,"\t Modify srt subtitle files. \n");
    fprintf(stderr,"\t -v                Verbose\n");
    fprintf(stderr,"\t -r                Renumber all entries.\n");
    fprintf(stderr,"\t -d <time>         Shift all timestamps by <time> seconds\n");
    fprintf(stderr,"\t -s                Substitute filename after time stamps\n"); 
    fprintf(stderr,"\t                   by its content\n");
    fprintf(stderr,"\t -i <filename>     Use filename for input (default) stdin\n");
    fprintf(stderr,"\t -o <filename>     Use filename for output (default stdout)\n");
    fprintf(stderr,"\t -c <first[,last]> Write only entries from first to last\n ");
    fprintf(stderr,"\t	               (default=last entry) to output\n");
    fprintf(stderr,"\t -a <hh:mm:ss,ms>  Adjust all time stamps so that the first tag\n");
    fprintf(stderr,"\t                   beginns at hh:mm:ss,ms\n");
    fprintf(stderr,"\t -x <basename>     Reverse -s operation: produce basename.srtx\n");
    fprintf(stderr,"\t	               and basenameXXXX.txt files.\n");
    fprintf(stderr,"\t -w                Remove leading white space in text lines\n");
    fprintf(stderr,"\t -e <seconds>      'Expand' the subtitle hour by <seconds>\n"
                    "\t                   (valid values are -60.0<=x<=+60.0 seconds)\n");
    fprintf(stderr,"\t Example: \n");
    fprintf(stderr,"\t Adjust the subtitle timing by -2.3 seconds. \n");
    fprintf(stderr,"\t srttool -d -2.3 -i subtitle.srt > new_subtitle.srt \n");
    exit(0);
}


// if line ends in "\r\n" convert it to "\n"
void convert_crlf2lf(char *line)
{
    unsigned int len = strlen(line);
    // does the line end in \r\n ?
    if (len >= 2 && line[len-2] == '\r' && line[len-1] == '\n') {
        line[len-2]='\n'; // replace \r with \n
        line[len-1] = 0;    // repalce \n with end of string
    }
}


void write_srt_time(const srt_time_t start, const srt_time_t stop)
{
  fprintf(stdout,"%02u:%02u:%02u,%.3u --> %02u:%02u:%02u,%.3u\n",
            start.hour, start.min, start.sec, start.msec,
            stop.hour, stop.min, stop.sec, stop.msec);
}


// read the time stamps form file
int read_srt_time(srt_time_t* start_time, srt_time_t* stop_time)
{
    unsigned int start_hour, start_min, start_sec, start_msec;
    unsigned int end_hour, end_min, end_sec, end_msec;
    unsigned int items;
    items = fscanf(stdin,"%u:%u:%u,%u --> %u:%u:%u,%u",
                    &start_hour, &start_min, &start_sec, &start_msec,
                    &end_hour, &end_min, &end_sec, &end_msec);
    // read till end of line
    while( (fgetc(stdin)!='\n') && (!feof(stdin)) );

    start_time->hour=start_hour;
    start_time->min=start_min;
    start_time->sec=start_sec;
    start_time->msec=start_msec;

    stop_time->hour=end_hour;
    stop_time->min=end_min;
    stop_time->sec=end_sec;
    stop_time->msec=end_msec;

    if (items!=8) {
        if (verbose)
            fprintf(stderr,"Cannot properly convert time stamp\n");
        return -1;
    }

    return 0;
}


double srt_time2sec(const srt_time_t time_stamp)
{
    // convert time stamp to seconds
    return time_stamp.hour*3600 + time_stamp.min*60
            + time_stamp.sec + time_stamp.msec/1000.0;
}


srt_time_t sec2srt_time(double t)
{
    srt_time_t time_stamp;
    time_stamp.hour   = t/(3600);
    time_stamp.min    = (t-3600*time_stamp.hour)/60;
    time_stamp.sec    = (t-3600*time_stamp.hour-60*time_stamp.min);
    time_stamp.msec = (unsigned int) rint((t-3600*time_stamp.hour-60*time_stamp.min
                        -time_stamp.sec)*1000.0);
    return time_stamp;
}


void adjust_srt_time(srt_time_t* time_stamp, double time_offset, double time_scale)
{
    double t;
    t=srt_time2sec(*time_stamp);
    // adjust time according to offset
    t+=time_offset;
    // don't allow negative time
    if (t < 0) t=0;
    // scale the time 
    t *=time_scale;
    // writeout start and end time of this title
    *time_stamp=sec2srt_time(t);
}


void substitute_txt(srt_tag_t* tag)
{
    FILE* sub_file;
    char file_name[FILENAME_MAX];
    char read_buf[MAX_TEXT_LEN];
    int status;
    sscanf(tag->text,"%[^\n]",file_name);
    if (verbose) {
        fprintf(stderr,"reading substitution from file '%s'\n",file_name);
    }
    if (!(sub_file=fopen(file_name,"r"))) {
        perror("Cannot open file for substitution");
        fprintf(stderr,"Tried filename: %s\n",tag->text);
        exit(1);
    }
    // read and copy the text line by line in a way that
    // empty lines that might be produced by OCR are thrown away.
    tag->text[0]=0;
    do {
        read_buf[0]=0;
        status = fscanf(sub_file,"%[^\n]",read_buf);
        if (EOF == status ) {
            break;
        }
        // if nothing was read skip the next char (that would be an newline)
        if (strlen(read_buf)==0) {
            fgetc(sub_file);
            continue;
        }
        strcat(tag->text,read_buf);
        strcat(tag->text,"\n");
    } while(1);

    // append a newline to produce an empty line between tags
    strcat(tag->text,"\n");

    fclose(sub_file);
}


void remove_leading_whitespace(srt_tag_t *tag)
{
    char orig_text[MAX_TEXT_LEN];
    int line_start=~0;
    unsigned int len, i;
    unsigned int j=0;
    char c;
    // copy the original text
    strcpy(orig_text, tag->text);

    len = strlen(orig_text);
    tag->text[0]=0;

    for(i=0;i < len; ++i)
    {
        c = orig_text[i];
        // skip the leading whitespace
        if (isspace(c) && (c != '\n') && line_start) {
            continue;
        } 
        line_start=0;
        tag->text[j++]=c;
        // find end of line
        if (c == '\n') {
            line_start=~0;
        }
    }
    // make sure the string is terminated
    tag->text[j]=0;
}


void extract_txt(srt_tag_t* tag, const char* basename)
{
    FILE* ext_file;
    char file_name[FILENAME_MAX];

    // construct the file name from basename and tag number
    snprintf(file_name,FILENAME_MAX-9,"%s%04d.txt", 
                basename, tag->number);
    if (verbose) {
        fprintf(stderr,"extracting tag number %d to file '%s'\n",
                tag->number, file_name);
    }
 
    if (!(ext_file=fopen(file_name,"w"))) {
        perror("Cannot open file for extraction");
        fprintf(stderr,"Tried filename: %s\n",file_name);
        exit(1);
    }
    // write the text to the file
    fprintf(ext_file,"%s",tag->text);
    // replace the tag.text by its filename
    tag->text[0]=0;
    strcat(tag->text,file_name);
    strcat(tag->text,"\n");
    // append a newline to produce an empty line between tags
    strcat(tag->text,"\n");
    fclose(ext_file);
}


void read_srt_tag(srt_tag_t* tag)
{
    char line[MAX_TEXT_LEN];
    unsigned int num_line=0;

    // read the current number
    fscanf(stdin,"%d\n",&tag->number);
 
    // read the time stamps 
    if (read_srt_time(&tag->start_time, &tag->end_time)) {
        fprintf(stderr,"Wrong time stamp format in tag number %d detected\n",tag->number);
        fprintf(stderr,"Please correct the file and run again\n");
        exit(1);
    }
    // copy text until an empty line occurs
    tag->text[0]=0;

    do{
        if (fgets(line,MAX_TEXT_LEN-1,stdin)) {
            convert_crlf2lf(line);
            strcat(tag->text,line);
        } else {
            break;
        }
        // count the lines
        num_line++;
        // support DOS and Unix line endings
    } while( strcmp(line,"\n") && strcmp(line,"\r\n")  );


    if (num_line == 1)
    {
        if (verbose) {
            fprintf(stderr, "Empty text or missing newline in tag %d found. "
                    "Please correct file and run again.\n", tag->number);
            exit(-1);
        }
        strcat(tag->text,"\n");
    }
}


void write_srt_tag(const srt_tag_t tag)
{
    // write the tag number
    fprintf(stdout,"%d\n",tag.number);
    // write out the new time stamp
    write_srt_time(tag.start_time, tag.end_time);

    if (strlen(tag.text)>0) {
        //write the text
        fprintf(stdout,"%s",tag.text);
    } else {
        // empty text fields are not allowed.
        // Lets write some stupid text instead.
        fprintf(stdout,"(empty line detected)\n");
        if (verbose) {
            fprintf(stderr,"Empty text line in tag %d detected\n",tag.number);
        }
    }
}


int main(int argc, char** argv)
{
    char in_file_name[FILENAME_MAX];
    char out_file_name[FILENAME_MAX];
    char extract_basename[FILENAME_MAX];
    int n;
    double time_offset=0.0;
    double hour_expansion=0.0;
    double time_scale_factor=1.0;
    unsigned int input_counter=0;
    unsigned int output_counter=0;
    int ch;
    srt_tag_t input_tag;
    srt_tag_t output_tag;
    srt_time_t adjust_time={0,0,0,0};

    // first / last entry to process
    // 0 is file start/end 
    unsigned int first_tag_number=1;
    unsigned int last_tag_number=~0;

    // bool flags
    int renumber=0;
    int substitute=0;
    int adjust=0;
    int extract=0;
    int remove_whitespace=0;

    /* scan command line arguments */
    opterr=0;
    while ((ch = getopt(argc, argv, "wvi:o:a:d:c:x:e:rsh")) != -1)
    {
        switch (ch)
        {
        case 'v':
            verbose=~0;
            break;
        case 'd':  // time shift
            n = sscanf(optarg,"%lf", &time_offset);
            if (n!=1) {
                fprintf(stderr,"no time offset specified with option -d\n");
                exit(1);
            }
            if (verbose) {
                fprintf(stderr,"Using time offset of %f\n",time_offset);
            }
            break;
        case 'a':  // adjust relativ to given time stamp
            n = sscanf(optarg,"%d:%d:%d,%d", 
                &adjust_time.hour,&adjust_time.min,
                &adjust_time.sec, &adjust_time.msec);
            if (n!=4) {
                fprintf(stderr,"Invalid number of parameters for option -a\n");
                exit(1);
            }
            adjust = ~0;
            if (verbose) {
                fprintf(stderr,"Adjusting to %02d:%02d:%02d,%.3d as start time\n",
                        adjust_time.hour, adjust_time.min, adjust_time.sec, adjust_time.msec);
            }
            break;
        case 'c':  // cut output from first to last
            n = sscanf(optarg,"%d,%d", &first_tag_number, &last_tag_number);
            if (n==0) {
                fprintf(stderr,"Invalid parameter for -c option\n");
                exit(1);
            }
            if (first_tag_number==0) {
                first_tag_number=1;
                if (verbose)
                    fprintf(stderr, "setting first tag to 1\n");
            }
            if (verbose)
                fprintf(stderr,"Writing from %d. to %d. entrys. \n",
            first_tag_number, last_tag_number);
            break;
        case 'i':  // file name of input file
            n = sscanf(optarg,"%s", in_file_name);
            if (n!=1) {
                fprintf(stderr,"no filename for option -i\n");
                exit(1);
            }
            if (! (freopen(in_file_name,"r",stdin)) ) {
                perror("stdin redirection");
                exit(1);
            }
            if (verbose) {
                fprintf(stderr,"Using %s for input\n",in_file_name);
            }
            break;
    case 'o':  // file name of output file
            n = sscanf(optarg,"%s", out_file_name);
            if (n!=1) {
                fprintf(stderr,"no filename for option -o\n");
                exit(1);
            }
            if (! (freopen(out_file_name,"w",stdout)) ) {
                perror("stdout redirection");
                exit(1);
            }
            if (verbose)
                fprintf(stderr,"Using %s for output\n",out_file_name);
            break;
        case 'r': // renumber
            renumber=~0;
            break;
        case 's': // substitute
            substitute=~0;
            break;
        case 'w': // remove leading whitspace
            remove_whitespace=~0;
            break;
        case 'x':
            n = sscanf(optarg,"%s", extract_basename);
            if (n!=1) {
                fprintf(stderr,"no filename for option -x\n");
                exit(1);
            }
            extract=~0;
            break;
        case 'e':
            n = sscanf(optarg,"%lf", &hour_expansion);
            if (n!=1) {
                fprintf(stderr,"no time specified with option -e\n");
                exit(1);
            }
            // complain about an adjustment of more than 60 seconds
            if (fabs(hour_expansion > 60)) {
                fprintf(stderr,"Parameter to option -e to large.\n");
                exit(1);
            }
            // calculate the resulting scaling factor
            time_scale_factor = (3600.0 + hour_expansion)/3600.0;
            if (verbose)
                fprintf(stderr,"Using %f seconds for hour expansion\n",hour_expansion);
            break;
        case 'h':
            usage();
            break;
        default:
            fprintf(stderr,"Unknown option. Use -h for list of valid options.\n");
            exit(1);
        }
    }
  
    /* Check invalid option combinations here */
    if ( (substitute) && (extract) ) {
        fprintf(stderr,"-s and -x option cannot be used together");
        exit(1);
    }

    while(!feof(stdin))
    {
        // test if end of file reached
        ch=getc(stdin);
        if (ch != EOF) {
            ungetc(ch,stdin);
        } else {
            break;
        }
        // read the tag from file
        read_srt_tag(&input_tag);

        // count how many tags we have read
        ++input_counter;
        // only handle tags in given input range
        if ( (input_tag.number < first_tag_number) ||
            (input_tag.number > last_tag_number) ) {
            continue;
        }
        // copy the tag
        output_tag=input_tag;
        // adjust tag number if -r option is given
        if (renumber) {
            output_tag.number=output_counter+1;
        } 
        // adjust time so that tag starts at time given by -a option
        if (adjust && (output_counter == 0) ) {
            // simply add the correct adjustment to time_offset
            time_offset += srt_time2sec(adjust_time) - srt_time2sec(output_tag.start_time);
        }

        // add/sub the delay given in -d and -a option
        // and scale the time line according to -e option
        adjust_srt_time(&output_tag.start_time, time_offset, time_scale_factor);
        adjust_srt_time(&output_tag.end_time, time_offset, time_scale_factor);
    
        if (substitute) {
            // substitute the filename in next line by its content
            substitute_txt(&output_tag);
        }

        if (extract) {
            // write out the text input to extract_basenameXXXX.txt
            // and replace the tag.text by the filename
            extract_txt(&output_tag, extract_basename);
        }

        if (remove_whitespace) {
            remove_leading_whitespace(&output_tag);
        }
        // write out the tag
        write_srt_tag(output_tag);
        // count how many tag are written
        ++output_counter;
    }
  
    return 0;
}
