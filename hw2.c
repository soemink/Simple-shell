#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>


#define	MAXLINE	 80  /* Max text line length */
#define MAXBUF   80  /* Max I/O buffer size */
#define MAXARGS 80
#define MAXJOB 5

int newjid = 1;
int fd_in, fd_out;

void eval(char *cmdline);
int parseline(char *buf, char **argv, int *numArgs);
int builtin_command(char **argv, int argc);

void IOredirection(char **argv) 
{
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    int i;
    for (i = 0; i < sizeof(argv); i++) {
        if (argv[i] == NULL){
            break;
        }
        else if (strcmp(argv[i], "<") == 0) {
            int inFileID = open(argv[i + 1], O_RDONLY, mode);
            if (inFileID < 0) {
                printf("Input redirection fail");
                exit(1);
            }
            
            dup2(inFileID, STDIN_FILENO);
            close(inFileID);
        }
        else if (strcmp(argv[i], ">") == 0) {
            int outFileID = open(argv[i + 1], O_CREAT | O_WRONLY | O_TRUNC, mode);
            if (outFileID < 0) {
                printf("Output redirection fail");
                exit(1);
            }
            
            argv[i] = NULL;
            dup2(outFileID, STDOUT_FILENO);
            close(outFileID);
        }
    }
    return;
}




void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

pid_t Fork(void)
{
	pid_t pid;
	if ((pid = fork()) < 0)
		unix_error("Fork error");
	return pid;
}

int jobcount = 0;
struct job
{
	int jid;
	int pid;
	char *status;
	char bg;
	char cmd[MAXBUF];
};

int numJobs;
struct job joblist[MAXJOB];
void killAll()
{
    int i;
    for(i = 1; i <= MAXJOB; i++)
    {
        if(joblist[i].jid > 0)
        {
            kill(joblist[i].pid, SIGKILL);
            joblist[i].jid = 0;
            wait(0);
        }
    }
}

void sigint_handler(int sig)    // SIGINT (ctrl-c) --> terminate foreground child
{
    // printf("Caught SIGINT!");
    // int i;
    // for (i = MAXJOB - 1; i >= 0; i--)
    // {
    //     if (joblist[i].jid != 0)
    //     {
    //         // printf("%d %s %d\n", joblist[i].jid, joblist[i].status, joblist[i].bg);
    //         if (!strcmp(joblist[i].status, "Running") && !joblist[i].bg)
    //         {
    //             kill(joblist[i].pid, SIGINT);
    //             break;
    //         }
    //     }
    // }
    return;
}

void sigtstp_handler(int sig)    // SIGTSTP (ctrl-z) --> Stop
{
	// int i;
    // for (i = MAXJOB - 1; i >= 0; i--)
    // {
    //     if (joblist[i].jid != 0)
    //     {
            
    //         if (!strcmp(joblist[i].status, "Running") && !joblist[i].bg)
    //         {
				
    //             kill(joblist[i].pid, SIGTSTP);
    //             break;
    //         }
    //     }
    // }
    return;
}

// void sigcont_handler(int sig)    // SIGCONT --> Resume
// {
//     return;
// }


void sigchld_handler(int sig)    // SIGCHLD --> 
{
	pid_t pid;
    int status;
    while((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
	{
        if(WIFSTOPPED(status))
        {
            // printf("Testing ctrlz");
			int j;
			for (j = MAXJOB - 1; j >= 0; j--)
			{
				if (joblist[j].jid != 0 && joblist[j].bg != 1)
				{
					// printf("%s stopped", joblist[j].cmd);
					joblist[j].bg = 1;
					joblist[j].status = "Stopped";
                    
					
				}
			}
        }
		else if(WIFEXITED(status) | WIFSIGNALED(status)) 
		{
			int j;
			for (j = MAXJOB - 1; j >= 0; j--)
			{
                
				if (joblist[j].pid == pid && joblist[j].bg == 0)
				{
                    
					//printf("%s terminated", joblist[j].cmd);
					joblist[j].jid = 0;
					joblist[j].pid = 0;
					joblist[j].bg = 0;
					joblist[j].status = NULL;
					char blankCMD[MAXBUF];
					strncpy(joblist[j].cmd, blankCMD, MAXBUF);
                    // printf("%d before --", jobcount);
                    // jobcount--;
					
				}
			}
		}
        


	    }
	// int status;
	// int res = waitpid (-1,&status,WNOHANG);
	// int j;
	// for (j = MAXJOB - 1; j >= 0; j--)
	// {
	// 	if (joblist[j].pid == res)
	// 	{
	// 		printf("%s terminated", joblist[j].cmd);
	// 		joblist[j].jid = 0;
	// 		joblist[j].pid = 0;
	// 		joblist[j].bg = 0;
	// 		joblist[j].status = NULL;
	// 		char blankCMD[MAXBUF];
	// 		strncpy(joblist[j].cmd, blankCMD, MAXBUF);
    //         break;
			
	// 	}
	// }

	
}


void setJID(int *jid)
{
	*jid = newjid;
	newjid++;
}

void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
	int argc;
    
    

    strcpy(buf, cmdline);
    bg = parseline(buf, argv, &argc);
	// printf("%d", argc); 
    if (argv[0] == NULL)  
	    return;   /* Ignore empty lines */

    // if (argv[1] != NULL && (strcmp(argv[1], "<") || strcmp(argv[1], ">"))) 
    // {
    //     //redirect IO
    //     fd_in = dup(STDIN_FILENO);
    //     fd_out = dup(STDOUT_FILENO);
    //     IOredirection(argv); // moved from original location so it changes for each child process
    // }
    
    // int i;
    // mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    // for(i = 0; i < sizeof(argv); i++)
    // {
    //     if (argv[i] == NULL)
    //     {
    //         break;
    //     }
    //     else if (!strcmp(argv[i], "<"))
    //     {
    //         char *redirect_in_file = argv[i+1];
    //         int infileFD = open(redirect_in_file, O_RDONLY, mode);
    //         dup2(infileFD,STDIN_FILENO);
    //     }
    //     if (!strcmp(argv[i], ">"))
    //     {
    //         char *redirect_out_file;
    //         int outfileFD = open(redirect_out_file, O_CREAT | O_WRONLY | O_TRUNC, mode);
    //         dup2(outfileFD, STDOUT_FILENO);
    //     }
    // }


    if (!builtin_command(argv, argc)) 
    { 
        // int i;
        // int count = 0;
        // for (i = 0; i < MAXJOB; i++)
        // {
        //     if( joblist[i].jid != 0)
        //     {
        //         count++;
        //     }
        // }
        // if(count >= MAXJOB)
        // {
        //     printf("Max jobs reached\n");
        //     return;
        // }

        


	    if ((pid = Fork()) == 0) 
        {   /* Child runs user job */
            
            if (argv[1] != NULL && (strcmp(argv[1], "<") || strcmp(argv[1], ">"))) 
    {
        //redirect IO
        fd_in = dup(STDIN_FILENO);
        fd_out = dup(STDOUT_FILENO);
        IOredirection(argv); // moved from original location so it changes for each child process
    }
	        if ((execv(argv[0], argv) < 0) && (execvp(argv[0], argv) < 0)) 
            {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
	        }

	    }
        
	/* Parent waits for foreground job to terminate */
		if (!bg) 
        {
            int status;
            int i;
            for (i = 0; i < MAXJOB; i++)
            {
                if(joblist[i].jid == 0)
                {
                    setJID(&joblist[i].jid);
                    joblist[i].pid = pid;
                    joblist[i].bg = bg;
                    joblist[i].status = "Running";
                    strcpy(joblist[i].cmd, cmdline);
                    break;
                }
            }
            
            waitpid(pid, &status, WUNTRACED);
            if(WIFSTOPPED(status))
	        {
		        joblist[i].status = "Stopped";
                joblist[i].bg = 1;
	        }
            // if (waitpid(pid, &status, WNOHANG) == 0)
            //     unix_error("waitfg: waitpid error");
            // //printf("waitpid finished");

            int j;
            for (j = MAXJOB - 1; j >= 0; j--)
            {
                if (joblist[j].jid != 0 && joblist[i].bg == 0)
                {
                    //printf("%s resetted", joblist[j].cmd);
                    joblist[j].jid = 0;
                    joblist[j].pid = 0;
                    joblist[j].bg = bg;
                    joblist[j].status = NULL;
                    char blankCMD[MAXBUF];
                    strncpy(joblist[j].cmd, blankCMD, MAXBUF);
                    break;
                }
            }
        }
		else
		{
			// printf("%d %s", pid, cmdline);
			int i;
			for (i = 0; i < MAXJOB; i++)
			{
				if(joblist[i].jid == 0)
				{
					setJID(&joblist[i].jid);
					joblist[i].pid = pid;
					joblist[i].bg = 1;
					joblist[i].status = "Running";
					strcpy(joblist[i].cmd, cmdline);
					break;
				}
			}
		}
    }
    if (argv[1] != NULL && (strcmp(argv[1], "<") || strcmp(argv[1], ">")))
    { 
        dup2(fd_in,STDIN_FILENO);
        dup2(fd_out,STDOUT_FILENO);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, int argc) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
	    exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    if (!strcmp(argv[0], "cd"))
    {
        char cwd[64];
		if(argc > 1){
			char* newdir = argv[1];
			if(chdir(newdir) < 0){
				fprintf(stderr, "%s", "Directory error\n");
			}else{
				printf("%s\n", getcwd(cwd, 64));
			}
		}else if(argc == 1)
		{
			chdir("/home");
			printf("%s\n", getcwd(cwd, 64));
		}
		return 1;
    }
	if (!strcmp(argv[0], "jobs"))
	{
		int i;
		for (i = 0; i < MAXJOB; i++)
		{
			if(joblist[i].jid != 0 && joblist[i].bg == 1)
				printf("[%d] (%d) %s %s", joblist[i].jid, joblist[i].pid, joblist[i].status, joblist[i].cmd);
		}
		return 1;
	}
    if (!strcmp(argv[0], "fg"))
    {
        
        int i;
        if (argv[1][0] == '%')
        {
            i = atoi(argv[1]+sizeof(char));
            int j;
            for (j = 0; j < MAXJOB; j++)
            {
                if(joblist[j].jid == i)
                {
                    joblist[j].bg = 0;
                    joblist[j].status = "Running";
                    kill(joblist[j].pid, SIGCONT);
                    int status;
                    waitpid(joblist[j].pid, &status, WUNTRACED);
                    if(WIFSTOPPED(status))
                    {
                        joblist[j].status = "Stopped";
                        joblist[j].bg = 1;
                    }

                    return 1;
                }
            }
        }
        else
        {
            
            for (i = 0; i < MAXJOB; i++)
            {
                if(joblist[i].pid == atoi(argv[1]))
                {
                    break;
                }
            }
        }
        joblist[i].bg = 0;
        joblist[i].status = "Running";
        kill(joblist[i].pid, SIGCONT);
        int status;
        waitpid(joblist[i].pid, &status, WUNTRACED);
        if(WIFSTOPPED(status))
        {
            joblist[i].status = "Stopped";
            joblist[i].bg = 1;
        }

        return 1;

        
    }

    if (!strcmp(argv[0], "bg"))
    {
        int i;
        if (argv[1][0] == '%')
        {
            i = atoi(argv[1]+sizeof(char));
            int j;
            for (j = 0; j < MAXJOB; j++)
            {
                if(joblist[j].jid == i)
                {
                    joblist[j].status = "Running";
                    joblist[j].bg = 1;
                    kill(joblist[j].pid, SIGCONT);

                    return 1;
                }
            }
        }
        else
        {
            
            for (i = 0; i < MAXJOB; i++)
            {
                if(joblist[i].pid == atoi(argv[1]))
                {
                    break;
                }
            }
        }
        joblist[i].status = "Running";
        joblist[i].bg = 1;
        kill(joblist[i].pid, SIGCONT);
        return 1;

    }

    if( !strcmp(argv[0], "kill"))
    {
        int i;
        if (argv[1][0] == '%')
        {
            i = atoi(argv[1]+sizeof(char));
            int j;
            for (j = 0; j < MAXJOB; j++)
            {
                if(joblist[j].jid == i)
                {
                    joblist[j].jid = 0;
                    kill(joblist[j].pid, SIGCONT);
                    kill(joblist[j].pid, SIGINT);
                    int status;
                    waitpid(joblist[j].pid, &status, WUNTRACED);
                    if(WIFEXITED(status) | WIFSIGNALED(status))
                    {
                        joblist[j].jid = 0;
                        joblist[j].pid = 0;
                        joblist[j].bg = 0;
                        joblist[j].status = NULL;
                        char blankCMD[MAXBUF];
                        strncpy(joblist[j].cmd, blankCMD, MAXBUF);
                    }
                }
            }
        }
        else
        {
            
            for (i = 0; i < MAXJOB; i++)
            {
                if(joblist[i].pid == atoi(argv[1]))
                {
                    break;
                }
            }
        }
        joblist[i].jid = 0;
        kill(joblist[i].pid, SIGCONT);
		kill(joblist[i].pid, SIGINT);
        int status;
        waitpid(joblist[i].pid, &status, WUNTRACED);
        if(WIFEXITED(status) | WIFSIGNALED(status))
        {
            joblist[i].jid = 0;
            joblist[i].pid = 0;
            joblist[i].bg = 0;
            joblist[i].status = NULL;
            char blankCMD[MAXBUF];
            strncpy(joblist[i].cmd, blankCMD, MAXBUF);
        }
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv, int *numArgs) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
	       buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;
	// printf("%d", bg);
	*numArgs = argc ;
    return bg;
}
/* $end parseline */



int main() 
{

   
    // mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    // char *redirect_in_file;
    // char *redirect_out_file;
    // int inFileID = open(redirect_in_file, O_RDONLY, mode);
    // dup2(inFileID, STDIN_FILENO);
    // int outFileID = open(redirect_out_file, O_CREAT|O_WRONLY|O_TRUNC, mode);
    // dup2(outFileID,STDOUT_FILENO);


    char cmdline[MAXLINE]; /* Command line */

    int i;
    for (i = 0; i < MAXJOB; i++)
    {
        joblist[i].jid = 0;
    }

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        unix_error("SIGINT signal error");

    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR)
        unix_error("SIGTSTP signal error");

    // if (signal(SIGCONT, sigcont_handler) == SIG_ERR)
    //     unix_error("SIGCONT signal error");

    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
        unix_error("SIGCHLD signal error");

    while (1) 
    {
    /* Read */
    printf("prompt> ");
    fgets(cmdline, MAXLINE, stdin); 
    if (feof(stdin))
        exit(0);

    /* Evaluate */
    eval(cmdline);
    }

}