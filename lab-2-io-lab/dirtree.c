//--------------------------------------------------------------------------------------------------
// System Programming                         I/O Lab                                    Fall 2020
//
/// @file
/// @brief resursively traverse directory tree and list all entries
/// @author <yourname>
/// @studid <studentid>
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>

#define MAX_DIR 64            ///< maximum number of directories supported

/// @brief output control flags
#define F_TREE      0x1       ///< enable tree view
#define F_SUMMARY   0x2       ///< enable summary
#define F_VERBOSE   0x4       ///< turn on verbose mode

/// @brief struct holding the summary
struct summary {
  unsigned int dirs;          ///< number of directories encountered
  unsigned int files;         ///< number of files
  unsigned int links;         ///< number of links
  unsigned int fifos;         ///< number of pipes
  unsigned int socks;         ///< number of sockets

  unsigned long long size;    ///< total size (in bytes)
  unsigned long long blocks;  ///< total number of blocks (512 byte blocks)
};


/// @brief abort the program with EXIT_FAILURE and an optional error message
///
/// @param msg optional error message or NULL
void panic(const char *msg)
{
  if (msg) fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}


/// @brief read next directory entry from open directory 'dir'. Ignores '.' and '..' entries
///
/// @param dir open DIR* stream
/// @retval entry on success
/// @retval NULL on error or if there are no more entries
struct dirent *getNext(DIR *dir)
{
  struct dirent *next;
  int ignore;

  do {
    errno = 0;
    next = readdir(dir);
    if (errno != 0) perror(NULL);
    ignore = next && ((strcmp(next->d_name, ".") == 0) || (strcmp(next->d_name, "..") == 0));
  } while (next && ignore);

  return next;
}


/// @brief qsort comparator to sort directory entries. Sorted by name, directories first.
///
/// @param a pointer to first entry
/// @param b pointer to second entry
/// @retval -1 if a<b
/// @retval 0  if a==b
/// @retval 1  if a>b
static int dirent_compare(const void *a, const void *b)
{
  struct dirent *e1 = (struct dirent*)a;
  struct dirent *e2 = (struct dirent*)b;

  // if one of the entries is a directory, it comes first
  if (e1->d_type != e2->d_type) {
    if (e1->d_type == DT_DIR) return -1;
    if (e2->d_type == DT_DIR) return 1;
  }

  // otherwise sorty by name
  return strcmp(e1->d_name, e2->d_name);
}


/// @brief recursively process directory @a dn and print its tree
///
/// @param dn absolute or relative path string
/// @param pstr prefix string printed in front of each entry
/// @param stats pointer to statistics
/// @param flags output control flags (F_*)
void processDir(const char *dn, const char *pstr, struct summary *stats, unsigned int flags)
{
  
  errno = 0;
  DIR *sub_dir = opendir(dn);
  struct dirent *dep;
  struct dirent *files;
  int len = 0;
  int size = 50;
  unsigned int tree = flags & F_TREE;
  unsigned int verbose = flags & F_VERBOSE;
  char *nstr, *shown_name;

  if(errno){//find error about opendir
    if(tree) printf("%s`-ERROR: %s\n", pstr, strerror(errno));
    else printf("%s  ERROR: %s\n", pstr, strerror(errno));
    return;
  }
  files = (struct dirent *)malloc(size*sizeof(struct dirent));
  if(errno){//memory error found
    panic("Out of memory");
  }

  //store files in directory
  while((dep = getNext(sub_dir))!=NULL){
    if(len==size){//if allocated memory got full -> realloc
      size*=2;
      files = realloc(files, size*sizeof(struct dirent));
      if(errno){
        panic("Out of memory");
      }
    }
    files[len++] = *dep;
  }
  free(dep);

  //sort by filetype, filename
  qsort(files, len, sizeof(struct dirent), dirent_compare);
  
  for(int i=0; i<len; i++){
    errno = 0;
    nstr = strdup(pstr);//add pstr with '| ' or '  ' to make inner files' pstr
    nstr = realloc(nstr, strlen(nstr)+3);
    shown_name = strdup(pstr);//current file name includint pstr + '`-' or '|-' or '  '(shown name when print)
    shown_name = realloc(shown_name, strlen(shown_name)+strlen(files[i].d_name)+4);

    char *sub_file_name = strdup(dn);//subfile's name
    sub_file_name = realloc(sub_file_name, strlen(sub_file_name)+strlen(files[i].d_name)+3);

    if(!nstr || !shown_name || !sub_file_name){// memory error detect
      printf("err: %d ", errno);
      panic("Out of memory");
    }

    strcat(sub_file_name, "/");
    strcat(sub_file_name, files[i].d_name);//complete subfile's name
    //make shown_name, nstr according to conditions
    if(i==len-1 && tree) {
      strcat(shown_name, "`-");
      strcat(nstr, "  ");
    }
    else if(tree) {
      strcat(shown_name, "|-");
      strcat(nstr, "| ");
    }
    else {
      strcat(shown_name, "  ");
      strcat(nstr, "  ");
    }

    strcat(shown_name, files[i].d_name);

    //if shown_name's length > 54 : omit last some words and make ...
    if(strlen(shown_name)>54 && verbose){
      shown_name[51]='.';
      shown_name[52]='.';
      shown_name[53]='.';
    }
    if(verbose){
      printf("%-54.54s", shown_name);
    }
    else{
      printf("%s", shown_name);
    }
    free(shown_name);
    shown_name = NULL;
  
    struct stat info;
    lstat(sub_file_name, &info);//get information of subfile
    char type='\0';
    //statistic sum according to types, accumulate size, blocks
    if(S_ISDIR(info.st_mode)) {
      type = 'd';
      stats->dirs++;
    }
    else if(S_ISCHR(info.st_mode)) {
      type = 'c';
    }
    else if(S_ISBLK(info.st_mode)) {
      type = 'b';
    }
    else if(S_ISFIFO(info.st_mode)) {
      type = 'f';
      stats->fifos++;
    }
    else if(S_ISLNK(info.st_mode)) {
      type = 'l';
      stats->links++;
    }
    else if(S_ISSOCK(info.st_mode)) {
      type = 's';
      stats->socks++;
    }
    else{
      stats->files++;
    }
    stats->size+=info.st_size;
    stats->blocks+=info.st_blocks;

    //verbose additional print
    if(verbose){
      errno = 0;
      struct passwd *user = getpwuid(info.st_uid);
      struct group *group = getgrgid(info.st_gid);
      if(!user || !group){// memory error detect
        printf("\n%d ", errno);
        panic("Out of memory");
      }

      printf("  %8s:%-8s  %10ld  %8lu  %1c", getpwuid(info.st_uid)->pw_name, getgrgid(info.st_gid)->gr_name, info.st_size, info.st_blocks, type);
    }
    printf("\n");
    //if sub file is directory : recursive call
    if(files[i].d_type == DT_DIR){
       processDir(sub_file_name, nstr, stats, flags);
    }
     
    free(sub_file_name);
    free(nstr);
    sub_file_name = NULL;
    nstr = NULL;
  }
  free(files);
  files=NULL;
  closedir(sub_dir);
}


/// @brief print program syntax and an optional error message. Aborts the program with EXIT_FAILURE
///
/// @param argv0 command line argument 0 (executable)
/// @param error optional error (format) string (printf format) or NULL
/// @param ... parameter to the error format string
void syntax(const char *argv0, const char *error, ...)
{
  if (error) {
    va_list ap;

    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);

    printf("\n\n");
  }

  assert(argv0 != NULL);

  fprintf(stderr, "Usage %s [-t] [-s] [-v] [-h] [path...]\n"
                  "Gather information about directory trees. If no path is given, the current directory\n"
                  "is analyzed.\n"
                  "\n"
                  "Options:\n"
                  " -t        print the directory tree (default if no other option specified)\n"
                  " -s        print summary of directories (total number of files, total file size, etc)\n"
                  " -v        print detailed information for each file. Turns on tree view.\n"
                  " -h        print this help\n"
                  " path...   list of space-separated paths (max %d). Default is the current directory.\n",
                  basename(argv0), MAX_DIR);

  exit(EXIT_FAILURE);
}


/// @brief program entry point
int main(int argc, char *argv[])
{
  //
  // default directory is the current directory (".")
  //
  const char CURDIR[] = ".";
  const char *directories[MAX_DIR];
  int ndir = 0;

  struct summary tstat, dstat;
  
  unsigned int flags = 0;

  char section[101]; // ----------------------
  for(int i=0;i<100;i++){
    section[i]='-';
  }
  section[100]='\0';
 
  //
  // parse arguments
  //

  
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      // format: "-<flag>"
      if      (!strcmp(argv[i], "-t")) flags |= F_TREE;
      else if (!strcmp(argv[i], "-s")) flags |= F_SUMMARY;
      else if (!strcmp(argv[i], "-v")) flags |= F_VERBOSE;
      else if (!strcmp(argv[i], "-h")) syntax(argv[0], NULL);
      else syntax(argv[0], "Unrecognized option '%s'.", argv[i]);
    } else {
      // anything else is recognized as a directory
      if (ndir < MAX_DIR) {
        directories[ndir++] = argv[i];
      } else {
        printf("Warning: maximum number of directories exceeded, ignoring '%s'.\n", argv[i]);
      }
    }
  } 
  // if no directory was specified, use the current directory
  if (ndir == 0) directories[ndir++] = CURDIR;


  //
  // process each directory
  //
  // TODO
  // initialize tstat
  tstat.blocks=0;
  tstat.dirs=0;
  tstat.fifos=0;
  tstat.files=0;
  tstat.links=0;
  tstat.size=0;
  tstat.socks=0;

  for (int i = 0; i < ndir ; i++){
    //initalize dstat
    dstat.blocks=0;
    dstat.dirs=0;
    dstat.fifos=0;
    dstat.files=0;
    dstat.links=0;
    dstat.size=0;
    dstat.socks=0;

    //-s : title
    if(flags & F_SUMMARY){
      printf("%-60s", "Name");
      //-v : additional title
      if(flags & F_VERBOSE){
        printf("%-21s%-8s%-7s%s", "User:Group", "Size", "Blocks", "Type");
      }
      printf("\n%s\n", section);
    }
    printf("%s\n", directories[i]);
    processDir(directories[i], "", &dstat, flags); // statistic data of this directory is in dstat

    //summary
    if(flags & F_SUMMARY){
      printf("%s\n", section);
      char *str;
      //summary data by directory and grammarly correct
      int combine_err = asprintf(&str, "%d %s, %d %s, %d %s, %d %s, and %d %s", 
          dstat.files, dstat.files==1?"file":"files", 
          dstat.dirs,dstat.dirs==1?"directory":"directories", 
          dstat.links, dstat.links==1?"link":"links",
          dstat.fifos,dstat.fifos==1?"pipe":"pipes", 
          dstat.socks, dstat.socks==1?"socket":"sockets");
      if(combine_err==-1){
        panic("Out of memory");
      }
      printf("%-67s", str);
      free(str);
      str = NULL;

      //additional verbose summary
      if(flags & F_VERBOSE){
        printf("   %15lld %9llu", dstat.size, dstat.blocks);//current directory's size, blocks
      }
      printf("\n\n");

      //accumulate dstat's data to tstat
      tstat.blocks += dstat.blocks;
      tstat.dirs += dstat.dirs;
      tstat.fifos += dstat.fifos;
      tstat.files += dstat.files;
      tstat.links += dstat.links;
      tstat.size += dstat.size;
      tstat.socks += dstat.socks;
    }
  }

  //
  // print grand total
  //
  if ((flags & F_SUMMARY) && (ndir > 1)) {
    printf("Analyzed %d directories:\n"
           "  total # of files:        %16d\n"
           "  total # of directories:  %16d\n"
           "  total # of links:        %16d\n"
           "  total # of pipes:        %16d\n"
           "  total # of socksets:     %16d\n",
           ndir, tstat.files, tstat.dirs, tstat.links, tstat.fifos, tstat.socks);

    if (flags & F_VERBOSE) {
      printf("  total file size:         %16llu\n"
             "  total # of blocks:       %16llu\n",
             tstat.size, tstat.blocks);
    }

  }

  //
  // that's all, folks
  //
  return EXIT_SUCCESS;
}
