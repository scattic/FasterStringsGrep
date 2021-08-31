/*
TODO:
- check arguments for validity (files exist, data types are correct etc)
- implement regex functionality with pcre
- implement regex functionality with pcre from a template file
- output metadata to sqlite database (to augment the output strings):
-- start,end,size,type: line or group, group name
- develop an UI to visualize the density of strings / chunk of original input (to detect where are most strings in the input, and also explore the strings)
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef unsigned char bool;

#define is_graphical(c) ( ((c>=32)&&(c<=126))||(c=='\t') )

#define UPDATE_INTERVAL 5 

#define INCLUDE_WORD 1
#define EXCLUDE_WORD 2

typedef struct _simple_filter_item {
  char *word;
  char type;   // INCLUDE_WORD or EXCLUDE_WORD
  struct _simple_filter_item *next;
} simple_filter_item;

simple_filter_item *first_sfi = NULL,*last_sfi = NULL;  // linked-list of simple words to filter by

bool flag_quiet = 0;
bool flag_simple_filter = 0;

/* ---------------------------------------------------------------------------*/
/* display a fatal error message and exits */

void error(char *msg) {
  fprintf(stderr,"ERROR: %s\n",msg);
  exit(1);
}

/* ---------------------------------------------------------------------------*/

void *xmalloc(size_t size) {
	void *t = malloc(size);
	if(!t) error("cannot allocate memory");
	return t;
}

/* ---------------------------------------------------------------------------*/
/* add a new simple filter */
void add_sfi(char *word, char type) {
	if(!first_sfi) {
		first_sfi = xmalloc(sizeof(simple_filter_item));
		first_sfi->word = word; // note we do not copy that string
		first_sfi->type = type;
		first_sfi->next = NULL;
		last_sfi = first_sfi;
	}
	else {
		last_sfi->next = xmalloc(sizeof(simple_filter_item));
		last_sfi = last_sfi->next;
		last_sfi->word = word; // note we do not copy that string
		last_sfi->type = type;
		last_sfi->next = NULL;
	}
	if(!flag_quiet) {
		printf("> strings with '%s' will be %s\n",word,(type==1?"included":"excluded"));
	}
}

/* ---------------------------------------------------------------------------*/
/* 
check the validity of the syntax and prepare the data structure which holds 
this simple list of words to filter by 
*/

void prepare_simple_filter(char *arg) {
	
	const char delim[2]=" "; // space is the delimiter

  char *tkn = strtok(arg, delim);
  if(!tkn) error("invalid filter specified");
   
   /* walk through other tokens */
   while( tkn != NULL ) {
     switch(tkn[0]){
     	 case '+':
     	 	 add_sfi(tkn+1,INCLUDE_WORD);
     	   break;
     	 case '-':
	     	 add_sfi(tkn+1,EXCLUDE_WORD);
     	   break;
     	 default:
     	 	 error("invalid filter specified, missing + or - suffix for word");
     }
     tkn = strtok(NULL, delim);
   }

}

/* ---------------------------------------------------------------------------*/
/* get file size with fstat */

off_t get_filesize(int fd) {
   struct stat s;
   if (fstat(fd, &s) == -1) {
      error("error getting file size");
   }
   return(s.st_size);
}

/* ---------------------------------------------------------------------------*/
/* 
   looks for null-terminated needed in the haystack. Haystack is not null 
   terminated, but we know the size.
   
   returns: 1 if needle was found, 0 if not.
*/

int findstr(const char *haystack, size_t haystack_len, const char *needle) {
  
  int needle_len;
  if ( ((needle_len=strlen(needle)) == 0) || (needle_len > haystack_len) ) return 0;
  
	for(int i=0;i<haystack_len-needle_len+1;i++) {
	  if(!strncmp(needle,haystack,needle_len)) return 1;
	  haystack++;
	}
	
	return 0;

}

/* ---------------------------------------------------------------------------*/
/* outputs the characters from start until but not including the char pointed to by end, followed by a newline char */

void output_string(FILE* f, char *start, char *end) {
  
  if( !start || !end ) error("invalid call to output_string");
  
  bool output = 1; // do we send to output or not? assume yes to begin
  bool include_result = 0;
  
  if( flag_simple_filter ) {
  	simple_filter_item *sfi = first_sfi;
  	while(sfi) {
  	  int r = findstr(start,end-start,sfi->word);
  	  if(sfi->type==EXCLUDE_WORD && r==1) { // any hit on exlusions means no output, game over
  	  	output = 0;  
  	  	break; 
  	  }
  	  if(sfi->type==INCLUDE_WORD && r==1) { // at least one of the words matched
  	  	include_result = 1; 
  	  }
  		sfi = sfi->next;
  	}
  }
  
  output = output & include_result;
  
  if( (flag_simple_filter==0) || (flag_simple_filter==1 && output==1) ) {
		while(start<end) {
		  fputc(start[0],f);
		  start++;
		}
		fputc('\n',f);
  }
}

/* ---------------------------------------------------------------------------*/

void display_usage(char *name) {
  printf("FasterStringsGrep - faster alternative to running strings | grep\n");
  printf("\n  %s [-o output] [-n length] [-j offset] [-q] [-h] [-f \"filter\"] [-r \"regex\"] [-t template] input\n",name);
  printf("\nwhere:\n");
  printf("input:\t\tname of input file\n");
  printf("-o output:\toptional, name of output file or `stdout` if none specified\n");
  printf("-n length:\toptional, minimum number of chars for a string, default is 4\n");
  printf("-j offset:\toptional, start searching from this offset onwards. Useful to resume a failed search in a large file.\n");
  printf("-f filter:\toptional, filter argument is a special string containing list of words which must exist or not in the output.\n");
  printf("\t\tPrefix words which must be included with a +, and excluded with a -.\n\t\tAll inclusions are OR'ed, then result is AND'ed with the OR'ed exclusions.\n\t\tWords are case sensitive, must be separated by spaces, and sub-strings with spaces are not supported.\n");
  printf("\t\tExample: \"+foo +goo -bar -tar\" will output strings which contain 'foo' OR 'goo' AND do NOT contain 'bar' OR 'tar'.\n");
  //printf("-r regex:\toptional, regexp argument is a special string containing a valid regex expression which must match against the output.\n");
  printf("-q:\t\trun quietly, without showing progress. Progress is shown only when writing results to a file.\n");
  printf("-h:\t\tthis message\n");
}

/* ---------------------------------------------------------------------------*/
/* main */

int main(int argc, char* argv[]) {

int  input_file = 0;           // file descriptor
char *input_file_name = NULL;  // name of file to open 
char *p_input = NULL;          // pointer to data in the memory mapped file

FILE *output_file = stdout;
char *output_file_name = NULL;

int  c;                    // the char we just read
int  min_string_size = 4;  // must have at least these many valid chars to be a string
bool in_string = 0;        // state

unsigned long long read_bytes = 0;    // how many bytes read from the input
unsigned long long old_read = 0;      // used for stats
unsigned long long file_size;         // same as above

unsigned long long offset = 0; // start from this offset, optionally 

time_t last_time,time_elapsed,time_started; // used for stats

char *string_start=NULL, *string_end=NULL; // this will remember the position of the last string in the memory mapping of the file

	opterr = 0;

  while ((c = getopt (argc, argv, "i:o:n:j:f:qh")) != -1)
    switch (c)
      {
      case 'o':
        output_file_name = optarg;
        output_file = fopen(output_file_name,"w");
        if(!output_file) error("Cannot open output file");
        break;
      case 'n':
        min_string_size = strtol(optarg,NULL,0);
        break;
      case 'j':
      	if(!optarg) error("Offset argument must be a number.");
        offset = strtol(optarg,NULL,0);
        break;
        
      case 'f':
        prepare_simple_filter(optarg);
        flag_simple_filter = 1;
        break;
        
      case 'q':
        flag_quiet = 1;
        break;
      case 'h':
				display_usage(argv[0]);
        return 0;
      case '?':
        if ( (optopt == 'o') || (optopt == 'n') || (optopt == 'j') || (optopt == 'f') )
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character `\\x%x'.\n",optopt);
        return 1;
      default:
        abort ();
      }

	input_file_name = argv[optind];	// remaining is the only argument without a prefix
	
	if(!input_file_name) error("Input file name not specified. Run with -h to see the syntax.");

	input_file = open(input_file_name,O_RDONLY | 40000 | 01000000,0); // the numbers are O_DIRECT and O_NOATIME

	if(input_file<0) error("cannot open input file");

	posix_fadvise(input_file,0,0,POSIX_FADV_SEQUENTIAL); // this might help a bit

	file_size = get_filesize(input_file);

	p_input = mmap(0, file_size, PROT_READ, MAP_PRIVATE, input_file, 0);
	if (p_input == MAP_FAILED) error("cannot map the input file into memory");
	
	posix_madvise(p_input,file_size,POSIX_MADV_SEQUENTIAL); // kernel can also optimize based on this

	if(offset>0) {
		p_input += offset;
		file_size -= offset;
	}

	time_started = last_time = time(NULL);
	
  if(!flag_quiet) {
  	if(!offset) printf("\nStarting...\n");
  	else printf("\nStarting from offset %llu...\n",offset);
  }
    
	while(read_bytes < file_size) {
	
		c = (int)p_input[0];

    if( !flag_quiet && !(read_bytes % 1024*1024)) { // check every 1 MB AND
		  time_elapsed = time(NULL);
		  if(time_elapsed - last_time > UPDATE_INTERVAL) { // update at least every 5s 
		  	int progress = (read_bytes * 100) / file_size;
		  	unsigned long long bytes_between = read_bytes - old_read;
		  	old_read = read_bytes;
		  	float speedmbps = bytes_between / UPDATE_INTERVAL / 1024 / 1024;
		  	float avgspeedmbps = read_bytes / (time_elapsed-time_started) / 1024 / 1024;
		  	float remaining_time = (file_size - read_bytes) / 1024 / 1024 / avgspeedmbps / 60;
		  	printf("Progress: %d%%\t[%.2f MBps]\t[%llu total bytes read]\t[AVG: %.2f MBps]\t[ETA: %.2f minutes]\n",progress,speedmbps,read_bytes,avgspeedmbps,remaining_time);
		  	last_time = time(NULL);
		  }
    }

    if(is_graphical(c)) {                     
      if(!in_string) {                                  // we just started reading a potential string
				in_string = 1;
				string_start = p_input;                         // remember the start
    	}
  	}
  	else { // read a non printable character, this means we also terminate a current string
    	if(in_string) {
    		if ((p_input - string_start) >= min_string_size) { // does this input qualify?
    			string_end = p_input;
    			output_string(output_file,string_start, string_end);
    		}
				in_string = 0;
				string_start = string_end = 0;
    	}
  	}
  	
  	p_input++; 
  	read_bytes++;

  }
  
  if(!flag_quiet) {
  	printf("Done.\n");
  }
  
  fclose(output_file);

}
