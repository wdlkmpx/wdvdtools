/*this files is part of vobcopy*/

bool get_dvd_name(const char *, char *);
int get_device(char *, char *);
int get_device_on_your_own(char *, char *);
off_t get_vob_size(int , char *); 
/* int dvdtime2msec(dvd_time_t *); */
int get_longest_title( dvd_reader_t * );

