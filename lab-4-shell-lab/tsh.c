//--------------------------------------------------------------------------------------------------
// System Programming                         Shell Lab                                    Fall 2020
//
/// @author <Kim Gideok>
/// @studid <2018-13627>
//--------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXPIPES      8   /* max MAXPIPES */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */
#define ERROR_ -1

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
  pid_t pid;              /* job PID */
  int jid;                /* job ID [1, 2, ...] */
  int state;              /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/*----------------------------------------------------------------------------
 * Functions that you will implement
 */

void eval(char *cmdline);
int builtin_cmd(char *(*argv)[MAXARGS]  );
void do_bgfg(char *(*argv)[MAXARGS]  );
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

/*----------------------------------------------------------------------------*/

/* These functions are already implemented for your convenience */
int parseline(const char *cmdline, char *(*argv)[MAXARGS],  int* pipec);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

int redirect_in_koo(char **cmd); 
int redirect_out_koo(char **cmd); 


/*
 * main - The shell's main routine
 */
int main(int argc, char **argv)
{
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  // Redirect stderr outputs to stdout
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
      case 'h':             /* print help message */
        usage();
        break;
      case 'v':             /* emit additional diagnostic info */
        verbose = 1;
        break;
      case 'p':             /* don't print a prompt */
        emit_prompt = 0;  /* handy for automatic testing */
        break;
      default:
        usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT,  sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1) {

    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }
    	
    eval(cmdline);
    
    fflush(stdout);
    fflush(stdout);
  }

  exit(0); /* control never reaches here */
}


/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 * When there is redirection(>), it return file name to char** file
 */


void eval(char *cmdline)
{ 
  char *command[MAXPIPES][MAXARGS];
  int cmds;
  sigset_t mask;
  pid_t pid;
  int file=-1;
  int fd[MAXPIPES][2];
  
  int is_bg = parseline(cmdline, command, &cmds); // parse command lines
  if(command[0][0]==NULL) return;
  if(!builtin_cmd(command)){//treat built in command
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);// block sigchld before fork
    if((pid = fork())==0){
      setpgid(0, 0);
      int k = 0;

    for(int i=0;i<cmds-1;i++) {//make pipes and redirect to stdin, stdout to  pipes
      if(pipe(fd[i])==-1){
        printf("pipe error\n");
        return;
      }
      if((pid=fork())==0){
        setpgid(0,0);
        dup2(fd[i][0], 0);
        close(fd[i][0]);
        close(fd[i][1]);
        k=i+1;
        continue;
      }
      sigprocmask(SIG_UNBLOCK, &mask, NULL);
      dup2(fd[i][1], 1);
      close(fd[i][0]);
      close(fd[i][1]);
      break;
    }
      int j=0;
      if(k==cmds-1){// if last command wanna redirect to file ->  redirect to file
      while(command[cmds-1][j]!=NULL){
        if(!strcmp(command[cmds-1][j], ">") && command[cmds-1][j+1]){
          command[cmds-1][j]=NULL;
          if((file = open(command[cmds-1][j+1], O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR))==-1){
            printf("%s: Open file error\n", command[cmds-1][j+1]);
          }
          dup2(file, 1);
          if(execvp(command[cmds-1][0], command[cmds-1])<0){
            printf("%s: No such file ofr directory.\n", command[cmds-1][0]);
            exit(0);
          }

          break;
        }
        j++;
      }
      }
        
      if(execvp(command[k][0], command[k])<0){//execute
        printf("%s: No such file or directory.\n", command[k][0]);
        exit(0);
      }
    }
    if(!is_bg){//handle when fg
      addjob(jobs, pid, FG, cmdline);
      sigprocmask(SIG_UNBLOCK, &mask, NULL);
      waitfg(pid);
    }
    else{//handle when bg
      addjob(jobs, pid, BG, cmdline);
      sigprocmask(SIG_UNBLOCK, &mask, NULL);
      printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
    }
}
  return;

}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 * argv[MAXPIPES][MAXARGS]
*/


int parseline(const char *cmdline, char *(*argv)[MAXARGS] , int *rpipec  )
{
  static char array[MAXLINE]; /* holds local copy of command line */
  char* buf = array;          /* ptr that traverses command line */
  char* delim;                /* points to first space delimiter */
  char* pdelim;               /* points to pipe */
  int argc;                   /* number of args */
  int bg=0;                   /* background job? */
  int pipec;
  
  strcpy(buf, cmdline);
  buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */

  argc = 0;// How many argv
  pipec = 0;

  // ignore leading spaces
  while (*buf && (*buf == ' ')) buf++;

  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[pipec ][ argc++] = buf;
    *delim = '\0';
    buf = delim + 1;

    // ignore spaces
    while (*buf && (*buf == ' ')) buf++;

    if (*buf) pdelim = strchr(buf, '|');

    // if there is pipe right on buf pointer
    if (*buf && pdelim  && *buf == *pdelim) {
      pipec++;
      argc=0;
      buf = buf + 1;

      // ignore spaces
      while (*buf && ( *buf == ' ')) buf++;
    }

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[pipec][ argc] = NULL;
  pipec++;
  *rpipec = pipec;// change pipec value for eval()

  // ignore blank line
  if (argc == 0) return 1;

  // should the job run in the background?
 if ((bg = (strcmp(argv[pipec-1][argc-1] , "&") == 0)) != 0) {
    argv[pipec-1][--argc] = NULL; 
  }
  return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */

int builtin_cmd(char *(*argv)[MAXARGS] )
{
  if(!strcmp(argv[0][0], "quit"))//case quit
    exit(0);
  if(!strcmp(argv[0][0], "jobs")){//case jobs
    listjobs(jobs);
    return 1;
  }
  if(!strcmp(argv[0][0], "bg")){//case bg
    do_bgfg(argv);
    return 1;
  }
  if(!strcmp(argv[0][0], "fg")){// casefg
    do_bgfg(argv);
    return 1;
  }

  return 0;
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */

void do_bgfg(char *(*argv)[MAXARGS] )
{
  struct job_t *target;
  char *arg;
  int jid;
  pid_t pid;
  arg = argv[0][1];//jid or pid
  if(arg == NULL){
    printf("%s command requires PIDPID or %%jobid argument\n", argv[0][0]);
    return;
  }
  if(arg[0]=='%'){//jid
    jid = atoi(&arg[1]);
    target = getjobjid(jobs, jid);
    if(target == NULL){
      printf("%s: No such job\n", arg);
      return;
    }
    else {
      pid = target->pid;
    }
  }
  else if(isdigit(arg[0])){//pid
    pid = atoi(arg);
    target = getjobpid(jobs, pid);
    if(target == NULL){
      printf("(%s): No such process\n", arg);
      return;
    }
  }
  else {//invalid arg
    printf("%s: argument must be a PID or %%jobid\n", (argv[0][0]));
    return;
  }
  kill(-pid, SIGCONT);//re running
  if(!strcmp(argv[0][0], "bg")){
    target->state=BG;
    printf("[%d] (%d) %s", target->jid, target->pid, target->cmdline);
  }
  else{
    target->state=FG;
    waitfg(target->pid);
  }
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{ 
  struct job_t* job;
  job = getjobpid(jobs, pid);

  if(pid==0){
    return;
  }
  if(job!=NULL){//if fgpid exist -> wait
    while(pid==fgpid(jobs));
  }
  return;
}


/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */

void sigchld_handler(int sig)
{
  int status, savedErrno;
  pid_t childPid;
  
  while((childPid=waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0){//wait for any child
    if(WIFSTOPPED(status)){//ctrl+z
      getjobpid(jobs,childPid)->state=ST;
      int jid = pid2jid(childPid);
      printf("Job [%d] (%d) Stopped by signal %d\n", jid, childPid, WSTOPSIG(status));
    }
    else if(WIFSIGNALED(status)){//ctrl+c
      int jid = pid2jid(childPid);
      printf("Job [%d] (%d) terminated by signal %d\n", jid, childPid, WTERMSIG(status));
      deletejob(jobs, childPid);
    }
    else if(WIFEXITED(status)){//normal exit
      deletejob(jobs, childPid);
    }
    return;
  }
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */

void sigint_handler(int sig)
{
  pid_t fgPid = fgpid(jobs);//find foreground job and force exit
  if(fgPid!=0){
    kill(-fgPid, sig);
  }
  return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
//
void sigtstp_handler(int sig)
{
  pid_t fgPid = fgpid(jobs);//find foreground fob and froce stop
  if(fgPid!=0){
   kill(-fgPid, sig);
  }
  return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
  int i, max=0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == 0) {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
        nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if(verbose){
        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs)+1;
      return 1;
    }
  }
  return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return jobs[i].pid;
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
  int i;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
        case BG:
          printf("Running ");
          break;
        case FG:
          printf("Foreground ");
          break;
        case ST:
          printf("Stopped ");
          break;
        default:
          printf("listjobs: Internal error: job[%d].state=%d ",
              i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}

/* $end tshref-ans */
