/* 
 * tsh - A tiny shell program with job control
 * 
 * @author Wei-Lin Tsai weilints@andrew.cmu.edu
 *
 * @note
 *   Some functions are copied (maybe with little modification)
 *   from csapp.c:
 *      Fork()
 *      Setpgid()
 *      Kill()   
 *      Sigprocmask()
 *      Sigemptyset()
 *      Sigaddset()
 *      Open()
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */
#define FILEPERMISSION 640   /* Linux default permission for new file */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */


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
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG} builtins;
};

/* End global variables */


/* Function prototypes */
void eval(char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list); 
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid); 
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);
void unix_error(char *msg); 
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* below are user (wei-lin) defined functions */
void do_builtin_cmd(struct cmdline_tokens *tok);    /// do builtin cmd
/// do non-builtin foregroud job 
void do_fg_job(struct cmdline_tokens *tok, char *cmdline);                  
/// do non-builtin background job
void do_bg_job(struct cmdline_tokens *tok, char *cmdline);                   
void do_builtin_quit_cmd();                         /// do builtin cmd: quit
/// do builtin cmd: jobs
void do_builtin_jobs_cmd(struct cmdline_tokens *tok);         
void do_builtin_bg_cmd(struct cmdline_tokens *tok); /// do builtin cmd: bg
void do_builtin_fg_cmd(struct cmdline_tokens *tok); /// do builtin cmd: fg
/// block target signals
void block_sigs(sigset_t *new_mask, sigset_t *old_mask);
/// handle io redirection for child process
void handle_io_redirection(struct cmdline_tokens *tok);

/* functions get from csapp.c */
pid_t Fork();                       /// wrapper of fork with error handling
/// wrapper of setpgid with error handling
void Setpgid(pid_t pid, pid_t pgid);
void Kill(pid_t pid, int signum);   /// wrapper of kill with error handling
/// wrapper of sigprocmask with error handling
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
/// wrapper of sigemptyset with error handling
void Sigemptyset(sigset_t *set);   
/// wrapper of sigaddset with error handling 
void Sigaddset(sigset_t *set, int signum);
/// wrapper of open with error handling
int Open(const char *pathname, int flags, mode_t mode);


/*
 * main - The shell's main routine 
 */
int 
main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
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
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(job_list);


    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { 
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
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
 */
void 
eval(char *cmdline) 
{
    int bg;              /* should the job run in bg or fg? */
    struct cmdline_tokens tok;

    /* Parse command line */
    bg = parseline(cmdline, &tok); 

    if (bg == -1) /* parsing error */
        return;
    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;

    if (tok.builtins == BUILTIN_NONE) { // non-builtin cmd
        if (bg) {
            do_bg_job(&tok, cmdline); 
        } else {
            do_fg_job(&tok, cmdline);
        }
    } else {                            // builtin cmd
        do_builtin_cmd(&tok);
    }

    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters
 *             enclosed in single or double quotes are treated as a single
 *             argument. 
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job  
 *  -1:        if cmdline is incorrectly formatted
 * 
 * Note:       The string elements of tok (e.g., argv[], infile, outfile) 
 *             are statically allocated inside parseline() and will be 
 *             overwritten the next time this function is invoked.
 */
int 
parseline(const char *cmdline, struct cmdline_tokens *tok) 
{

    static char array[MAXLINE];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */

    int parsing_state;                   /* indicates if the next token is the
                                            input or output file */

    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }

    (void) strncpy(buf, cmdline, MAXLINE);
    endbuf = buf + strlen(buf);

    tok->infile = NULL;
    tok->outfile = NULL;

    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;

    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;

        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }

        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
               quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }

        /* Terminate the token */
        *next = '\0';

        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
        case ST_NORMAL:
            tok->argv[tok->argc++] = buf;
            break;
        case ST_INFILE:
            tok->infile = buf;
            break;
        case ST_OUTFILE:
            tok->outfile = buf;
            break;
        default:
            (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
            return -1;
        }
        parsing_state = ST_NORMAL;

        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;

        buf = next + 1;
    }

    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }

    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;

    if (tok->argc == 0)  /* ignore blank line */
        return 1;

    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else {
        tok->builtins = BUILTIN_NONE;
    }

    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;

    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The 
 *     handler reaps all available zombie children, but doesn't wait 
 *     for any other currently running children to terminate.  
 */
void 
sigchld_handler(int sig) 
{   
    int status;
    pid_t pid; 

    // reap zobie if any, use WNOHANG for non-blocking waiting 
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0 ){
        if (WIFEXITED(status)){ // normal exit 
            // delete from job list
            deletejob(job_list, pid);
        } else if (WIFSIGNALED(status)){ // terminated  
            // print msg for sdriver use
            fprintf(stdout,"Job [%d] (%d) terminated by signal %d\n",
                  pid2jid(pid),pid,WTERMSIG(status));
            // delete from job list 
            deletejob(job_list, pid);
        } else if (WIFSTOPPED(status)){  // stopped
            // update state in job_list
            getjobpid(job_list, pid)->state = ST;

            // print msg for sdriver use
            fprintf(stdout,"Job [%d] (%d) stopped by signal %d\n",
                  pid2jid(pid),pid,WSTOPSIG(status));
        } else {
            fprintf(stdout, "child %d terminated abnormally\n", pid);
        }
    }

    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void 
sigint_handler(int sig) 
{
    pid_t pid;
    // get the foreground job pid
    if ((pid = fgpid(job_list))){  // use global variable job_list
        // kill the foreground job and its children (judge by same group pid)
        Kill(-pid, SIGINT);
    } else { 
        // if return 0, no ongoing foreground, do nothing
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void 
sigtstp_handler(int sig) 
{
    pid_t pid;
    // get the foreground job pid
    if ((pid = fgpid(job_list))){  // use global variable job_list
        // stop the foreground job and its children (judge by same group pid)
        Kill(-pid, SIGTSTP);
    } else { 
        // if return 0, no ongoing foreground, do nothing
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
void 
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void 
initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int 
maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int 
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n",
                       job_list[i].jid,
                       job_list[i].pid,
                       job_list[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int 
deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t 
fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t 
*getjobpid(struct job_t *job_list, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int 
pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void 
listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            switch (job_list[i].state) {
            case BG:
                sprintf(buf, "Running    ");
                break;
            case FG:
                sprintf(buf, "Foreground ");
                break;
            case ST:
                sprintf(buf, "Stopped    ");
                break;
            default:
                sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                        i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
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
void 
usage(void) 
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
void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void 
app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t 
*Signal(int signum, handler_t *handler) 
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
void 
sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* Here are user(wei-lin) defined function */

/**
 * @brief 
 *   Do the builtin cmd accrding to tok's builtin cmd type
 *
 * @attention  
 *   Will terminate probram when the cmd is quit   
 *
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 */
void do_builtin_cmd(struct cmdline_tokens *tok) {
    switch ( tok-> builtins){
    case BUILTIN_QUIT:
        do_builtin_quit_cmd(); 
        break;
    case BUILTIN_JOBS:
        do_builtin_jobs_cmd(tok);
        break;
    case BUILTIN_BG:
        do_builtin_bg_cmd(tok);
        break;
    case BUILTIN_FG:
        do_builtin_fg_cmd(tok);
        break;
    default:
        app_error("Error: invalid builtin cmd");
    }
}

/**
 * @brief 
 *   do non-builtin foreground job 
 *
 * Do the following
 *   1. block signals before fork to ensure order
 *   2. prepare empty mask for parent process use 
 *   3. fork !!
 *      3.1 child: recover signal block, change group id,
 *                 do io-redirection and then exec job
 *      3.2 parent: add child pid to job list,
 *                  and use empty mask for sigsuspend to wait child complete
 *                  finally it recover the mask
 * @note
 *   use global variable job_list
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 *   cmdline: Pointer to char string, contain the input cmd line. 
 */
void do_fg_job(struct cmdline_tokens *tok, char *cmdline) {
    pid_t child_pid;
    struct job_t *job_ptr;  /// point to current job to see if complete or not
    sigset_t empty_mask;    /// empty mask so can receive all signal
    sigset_t new_mask, old_mask; /// old_mask stores original signal mask
   
    // prepare mask 
    Sigemptyset(&empty_mask);

    // block signal
    block_sigs(&new_mask, &old_mask);

    if ( (child_pid = Fork()) == 0 ) { // child
        // recover signal block mask 
        Sigprocmask(SIG_SETMASK, &old_mask, NULL);

        // change group pid first
        Setpgid (0, 0);
        // do io redirection
        handle_io_redirection(tok);

        if (execve ( tok->argv[0], tok->argv, environ) < 0) {
            fprintf(stdout, "%s: Command not found.\n", tok->argv[0]);
            exit(0);
        }
    } else {    // parent
        if (!addjob(job_list, child_pid, FG, cmdline)){
            app_error("Error: addjob failed");
        }

        job_ptr = getjobpid(job_list, child_pid);
        // wait until foreground job complete
        // note that fgpid is must because the child may complete too fast
        while ( fgpid(job_list) && job_ptr->state == FG) {
            sigsuspend(&empty_mask);
        }

        // recover signal block mask 
        Sigprocmask(SIG_SETMASK, &old_mask, NULL);
    }
}

/**
 * @brief 
 *  do non-builtin backgroud job
 *
 * Do the following
 *   1. block signals before fork to ensure order
 *   2. fork !!
 *      2.1 child: recover signal block, change group id,
 *                 do io-redirection and then exec job
 *      2.2 parent: add child pid to job list, recover signal block 
 *                 no need to wait child process this time
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 *   cmdline: Pointer to char string, contain the input cmd line. 
 */
void do_bg_job(struct cmdline_tokens *tok, char *cmdline) {
    pid_t child_pid;
    sigset_t new_mask, old_mask; /// old_mask stores original signal mask

    // block signal
    block_sigs(&new_mask, &old_mask);

    if ( (child_pid = Fork()) == 0 ) { // child
        // recover signal block mask 
        Sigprocmask(SIG_SETMASK, &old_mask, NULL);

        // change group pid first
        Setpgid (0, 0);
        // do io redirection
        handle_io_redirection(tok);

        if (execve ( tok->argv[0], tok->argv, environ) < 0) {
            fprintf(stdout, "%s: Command not found.\n", tok->argv[0]);
            exit(0);
        }
    } else {    // parent
        if (!addjob(job_list, child_pid, BG, cmdline)){
            app_error("Error: addjob failed");
        }
        // recover signal block mask 
        Sigprocmask(SIG_SETMASK, &old_mask, NULL);

        // print message for sdriver use
        fprintf(stdout, "[%d] (%d) %s\n",pid2jid(child_pid),\
        				 child_pid,cmdline);
    } 
}

/**
 * @brief 
 *   Do builtin cmd: quit
 * @note
 *   Do nothing but exit the shell
 */
void do_builtin_quit_cmd(){
    // terminate 
    exit(0);
}

/**
 * @brief 
 *   Do builtin cmd: jobs
 * 
 * List all background jobs by calling listjobs()
 *
 * @note
 *   Use global variable job_list
 *   Also do output redirection as well
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 */
void do_builtin_jobs_cmd(struct cmdline_tokens *tok){
    int fd_out = 1;  /// file descriptor for output
   
    if (NULL != tok->outfile){ // output redirection
        fd_out = Open(tok->outfile, O_WRONLY | O_CREAT, FILEPERMISSION);
        listjobs(job_list, fd_out);
        close(fd_out);
    } else {                  // no need for redirection
        listjobs(job_list, STDOUT_FILENO);
    }
}

/**
 * @brief 
 *   Do builtin cmd: bg
 *
 * Find a target background job and let it run 
 *
 * @note
 *   if job not found, print out msg
 *   use global variable job_list
 * @attetion
 *   can support both "bg %JID" and "bg PID"
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 */
void do_builtin_bg_cmd(struct cmdline_tokens *tok){
    char *tmp_cptr = tok->argv[1];
    int id;  // can be PID or JID
    struct job_t *job;

    // get job 
    if (*tmp_cptr == '%'){               // If %JID
        id = atoi(tmp_cptr+1);
        job = getjobjid(job_list, id);
        if (NULL == job){
            fprintf(stdout,"%%%d: No such job\n", id);
            return;
        } 
    } else if (isdigit(*tmp_cptr)){      // If PID
        id = atoi(tmp_cptr);
        job = getjobpid(job_list, id);
        if (NULL == job){
            fprintf(stdout,"(%d): No such process\n", id);
            return;
        }
    } else {
        unix_error("bg: invalid argument");
        return;
    }
   
    // deal job
    if (job->state == ST){
        job->state = BG;
        Kill(job->pid, SIGCONT);
        fprintf(stdout,"[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
    }
}

/**
 * @brief 
 *   Do builtin cmd: fg
 *
 * Do the following
 *   1. prepare empty mask for later use 
 *   2. find target job, if any 
 *   3. if the target job is stopped or runs as background, let it run
 *   4. use empty mask and sigsuspend to wait for job complete
 *
 * @note
 *   if job not found, print out msg
 *   use global variable job_list
 * @attetion
 *   can support both "bg %JID" and "bg PID"
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 */
void do_builtin_fg_cmd(struct cmdline_tokens *tok){
    char *tmp_cptr = tok->argv[1];
    int id;  // can be PID or JID
    struct job_t *job;
    sigset_t empty_mask;

    // prepare mask 
    Sigemptyset(&empty_mask);

    // get job 
    if (*tmp_cptr == '%'){               // If %JID
        id = atoi(tmp_cptr+1);
        job = getjobjid(job_list, id);
        if (NULL == job){
            fprintf(stdout,"%%%d: No such job\n", id);
            return;
        } 
    } else if (isdigit(*tmp_cptr)){     // If PID
        id = atoi(tmp_cptr);
        job = getjobpid(job_list, id);
        if (NULL == job){
            fprintf(stdout,"(%d): No such process\n", id);
            return;
        }
    } else {
        unix_error("fg: invalid argument");
        return;
    }
   
    // deal job
    if ((job->state == ST) || (job->state == BG)){
        job->state = FG;
        Kill(job->pid, SIGCONT);
        // wait until foreground job complete
        // note that fgpid is must because the child may complete too fast
        while ( fgpid(job_list) && job->state == FG) {
            sigsuspend(&empty_mask);
        }
    }
}

/**
 * @brief 
 *   block some signals before fork() called 
 * Block following signals:
 *   SIGCHLD
 *   SIGINT
 *   SIGTSTP
 *
 * @param 
 *   new_mask pointer to sigset_t, used for mask 
 *   old_mask pointer to sigset_t, old mask, will be recover later
 */
void block_sigs(sigset_t *new_mask, sigset_t *old_mask){
    
    // clear new_mask 
    Sigemptyset(new_mask);

    // add mask 
    Sigaddset(new_mask, SIGCHLD); 
    Sigaddset(new_mask, SIGINT); 
    Sigaddset(new_mask, SIGTSTP); 

    // do blocking
    Sigprocmask(SIG_BLOCK, new_mask, old_mask);
}

/**
 * @brief 
 *   Handle IO redirection for child process 
 *
 * @attetion
 *   Print out error msg when error occured 
 * @param
 *   tok:     Pointer to a cmdline_tokens structure.
 */
void handle_io_redirection(struct cmdline_tokens *tok){
    int fd_in  = 0;  /// file descriptor for input
    int fd_out = 1;  /// file descriptor for output

    // for input redirection 
    if (NULL != tok->infile){
        fd_in = Open(tok->infile, O_RDONLY, 0);
        if(dup2(fd_in, STDIN_FILENO) != -1){
            // when redirct success, no need fd_in anymore
            close(fd_in);
        } else {
            unix_error("dup2 for input redirection failed");
        }
   }
    
    // for output redirection 
    if (NULL != tok->outfile){
        fd_out = Open(tok->outfile, O_WRONLY | O_CREAT, FILEPERMISSION);
        if(dup2(fd_out, STDOUT_FILENO) != -1){
            // when redirct success, no need fd_out anymore
            close(fd_out);
        } else {
            unix_error("dup2 for output redirection failed");
        }
    }
}

/** Followings are functions from csapp.c */

/**
 * @brief 
 *   A wrapper of fork with additional error handling
 * @note
 *   Copy from csapp.c 
 * @return 
 *   pid pid_t: a valid forked pid
 */
pid_t Fork() 
{
    pid_t pid;

    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}

/**
 * @brief 
 *   A wrapper of setpgid with additional error handling
 * @note
 *   Copy from csapp.c 
 * @param
 *   pid : target pid 
 *   pgid : targe pgid
 */
void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
	unix_error("Setpgid error");
    return;
}

/**
 * @brief 
 *   A wrapper of kill with additional error handling
 * @note
 *   Copy from csapp.c 
 * @param
 *   pid : target pid 
 *   signum : signal number 
 */
void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
	unix_error("Kill error");
}

/**
 * @brief 
 *   A wrapper of sigprocmask with additional error handling
 * @note
 *   Copy from csapp.c 
 * @param
 *   how : please man sigprocmask
 *   set : please man sigprocmask 
 *   oldset: please man sigprocmask
 */
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
	unix_error("Sigprocmask error");
    return;
}

/**
 * @brief 
 *   A wrapper of sigemptyset with additional error handling
 * @note
 *   Copy from csapp.c 
 * @param
 *   set : please man sigemptyset
 */
void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
	unix_error("Sigemptyset error");
    return;
}

/**
 * @brief 
 *   A wrapper of aigaddset with additional error handling
 * @note
 *   Copy from csapp.c 
 * @param
 *   set : please man sigaddset
 *   signum : please man sigaddset
 */
void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
	unix_error("Sigaddset error");
    return;
}

/**
 * @brief 
 *   A wrapper of read with additional error handling
 * @note
 *   Copy from csapp.c 
 * @param
 *   pathname : the whole path file name to be oepned
 *   signum : please man sigaddset
 * @ret
 *   valid file descriptor
 */
int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}
