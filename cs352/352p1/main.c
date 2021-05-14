#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#define MAX_LINE 80
#define MAX_ARGS (MAX_LINE/2 + 1)
#define REDIRECT_OUT_OP '>'
#define REDIRECT_IN_OP '<'
#define PIPE_OP '|'
#define BG_OP '&'

#define MAXJOBS 24
#define UNKNOWN 0       // unknown state, not FOREGROUND, BACKGROUND, and STOPPED.
#define FOREGROUND 1
#define BACKGROUND 2
#define STOPPED 3

int nextjid = 1;
/* Holds a single command. */
typedef struct Cmd {
    /* The command as input by the user. */
    char line[MAX_LINE + 1];
    /* The command as null terminated tokens. */
    char tokenLine[MAX_LINE + 1];
    /* Pointers to each argument in tokenLine, non-arguments are NULL. */
    char* args[MAX_ARGS];
    /* Pointers to each symbol in tokenLine, non-symbols are NULL. */
    char* symbols[MAX_ARGS];
    /* The process id of the executing command. */
    pid_t pid;
} Cmd;

/*
 * job structure, which contains job id, command line, state and job pid.
 */
struct job_t {
    char commandline[MAX_LINE+1];   //command line
    int jobID;      // job id
    int state;      //[FOREGROUND, BACKGROUND, STOPPED]
    pid_t pid;      // job pid
};

extern pid_t foregroundPid;
extern struct job_t jobs[MAXJOBS];

/* Finds the index of the first occurance of symbol in cmd->symbols.
 * Returns -1 if not found.
 */
int findSymbol(Cmd* cmd, char symbol)
{
    for (int i = 0; i < MAX_ARGS; i++) {
        if (cmd->symbols[i] && *cmd->symbols[i] == symbol) {
            return i;
        }
    }
    return -1;
}

/*
 * to check if the command exist
 * return -1 if not found.
 */
int checker(char *comm,char *path)
{
    char * p;
    int i=0;
    p = getenv("PATH");
    char buffer[80];
    while(*p != '\0')
    {
        if(*p != ':'){
            buffer[i++] = *p;
        }
        else {
            buffer[i++] = '/';
            buffer[i] = '\0';
            strcat(buffer, comm);
            if(access(buffer, F_OK) == 0){
                strncpy(path,buffer,128);
                return 0;
            }
            else{
                i = 0;
            }
        }
        p++;
    }
    return -1;
}

/*
 * has similar function with findSymbol, return -1 if not found.
 */
int find_position(char *symbols[]){
    for (int i=0; i<MAX_ARGS; i++){
        if (symbols[i]){
            return i;
        }
    }
    return -1;
}
//--------------------------------------------
//          file redirection function
//--------------------------------------------
int redirect(Cmd* cmd){
    char path[128];
    char file_name[2];
    int status = 0;
    int in_check = -1;
    int out_check = -1;
    pid_t pid = -1;
    memset(path,0x0,sizeof(path));
    memset(file_name,0x0,sizeof(file_name));
    int temp = checker(cmd->tokenLine, path);
    if(temp == -1){
        printf("command not found: %s\n", cmd->tokenLine);
        return -1;
    }
    int position = find_position(cmd->symbols);
    strcpy(file_name, cmd->args[position+1]);
    if(cmd->symbols[position][0] == REDIRECT_IN_OP)
        in_check = 0;
    else if(cmd->symbols[position][0] == REDIRECT_OUT_OP)
        out_check = 0;

    int in = open(file_name, O_RDONLY, S_IRUSR|S_IWUSR);
    int out = open(file_name, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
    if((pid=fork()) == 0){
        if(in_check != -1){
            if(in == -1){
                printf("could not open in.\n");
                return -1;
            }
        }
        if(out_check != -1){
            if(out == -1) {
                printf("could not open out.\n");
                return -1;
            }
        }
        if(in_check != -1){
            if(dup2(in, STDIN_FILENO) == -1){
                printf("standard in error.\n");
                exit(1);
            }
        }
        if(out_check != -1){
            if(dup2(out, STDOUT_FILENO) == -1){
                printf("standard out error.\n");
                exit(1);
            }
        }
        execv(path, cmd->args);
    }else{
        waitpid(pid, &status, 0);
    }
    return 0;
}
//------------------------------------------------
//              Pipe for command
//------------------------------------------------
int pipe_func(Cmd* cmd){
    char path[128];
    char bin_file[128];
    char **ptr;
    int command = 0, fd[2];
    char end_part[1];
    pid_t child1, child2;
    memset(path,0x0,sizeof(path));
    memset(bin_file,0x0,sizeof(bin_file));
    int temp = checker(cmd->tokenLine,path);
    if(temp == -1) {
        printf("The first command is not founded!!!\n");
        return -1;
    }
    int position = find_position(cmd->symbols);
    if(pipe(fd) == -1) {
        printf("Open pipe error !\n");
        return -1;
    }

    /* create a process and executes the command preceding the pipe character and writes the output to the pipe*/
    if((child1 = fork()) == 0) {
        //close read
        close(fd[0]);
        if(fd[1] != STDOUT_FILENO) {
            /*Redirect the standard output to the writing end of the pipe*/
            if(dup2(fd[1], STDOUT_FILENO) == -1){
                printf("Redirect Standard Out error !\n");
                return -1;
            }
            //close write
            close(fd[1]);
        }
        execv(path, cmd->args);
    }
    else{
        waitpid(child1, &command,0);
        end_part[0] = 0x1a;
        write(fd[1], end_part, 1);
        close(fd[1]);
        ptr = cmd->args + position+1;
        strncpy(bin_file,ptr[0],sizeof(bin_file));
        memset(path,0x0,sizeof(path));
        temp = checker(bin_file,path);
        if(temp == -1) {
            printf("This second command is not founded!!!\n");
            return -1;
        }
        if((child2 = fork()) == 0){
            if(fd[0] != STDIN_FILENO){
                if(dup2(fd[0], STDIN_FILENO) == -1) {
                    printf("Redirect Standard In Error !\n");
                    return -1;
                }
                close(fd[0]);
            }
            execv(path, ptr);
        }
        else {
            waitpid(child2, NULL, 0);
        }
    }
    return 0;
}
/*
 * add a job
 */
int job_addition(struct job_t *jobs, pid_t pid, int state, char *cmdline){
    int i;
    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jobID = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].commandline, cmdline);
            return 1;
        }
    }
    return 0;
}

/*
 * return pid in foreground.
 */
pid_t pid_foreground(struct job_t *jobs) {
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FOREGROUND)
            return jobs[i].pid;
    return 0;
}

/*
 * block untile pid is not in foreground
 */
void block_foreground(pid_t pid){
    while(pid == pid_foreground(jobs)){
        sleep(0);
    }
}
//-------------------------------------------
//          Run command in foreground
//-------------------------------------------
void foregroundCMD(Cmd* cmd)
{
    int ret = 0;
    char path[128];
    memset(path,0x0,sizeof(path));
    ret = checker(cmd->tokenLine,path);
    if (-1 == ret){
        printf("This is command is not founded!\n");
        return;
    }

    if (-1 != findSymbol(cmd,PIPE_OP)){
        pipe_func(cmd);
        return;
    }

    if ((-1 != findSymbol(cmd,REDIRECT_OUT_OP)) || (-1 != findSymbol(cmd,REDIRECT_IN_OP))){
        redirect(cmd);
        return;
    }

    foregroundPid = fork();
    if(0 == foregroundPid) {
        execv(path, cmd->args);
    }
    else{
        job_addition(jobs,foregroundPid,FOREGROUND,cmd->line);
        block_foreground(foregroundPid);
    }
}

/*
 * return a job pid
 */
struct job_t *get_pid(struct job_t *jobs, pid_t pid) {
    if (pid < 1)
        return NULL;
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/*
 * return a job jid
 */
struct job_t *get_jid(struct job_t *jobs, int jid)
{
    if (jid < 1)
        return NULL;
    for (int i = 0; i < MAXJOBS; i++)
        if (jobs[i].jobID == jid)
            return &jobs[i];
    return NULL;
}
//--------------------------------------------
//          Run command in background
//--------------------------------------------
void backgroundCMG(Cmd* cmd)
{
    int ret = 0,status = 0, num_of_jobs = 0;
    pid_t pid = -1;
    char path[128];
    struct job_t *job;
    memset(path,0x0,sizeof(path));

    ret = checker(cmd->tokenLine,path);
    if (-1 == ret){
        printf("This is command is not founded!\n");
        return;
    }

    pid = fork();
    if(0 == pid) {
        execv(path, cmd->args);
        printf("son end");
    }
    else{
        job_addition(jobs,pid,BACKGROUND,cmd->line);
        num_of_jobs++;
        job = get_jid(jobs, num_of_jobs);
        waitpid(pid, &status, WNOHANG);
        printf("[%d] %d\n",job->jobID, pid);
    }
}

/*
 * return the largest jid.
 */
int max_jid(struct job_t *jobs)
{
    int i, max=0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jobID > max)
            max = jobs[i].jobID;
    return max;
}

/*
 * used in setup() and remove_jobs().
 */
void remove_helper(struct job_t *job){
    jobs->pid = 0;
    jobs->jobID = 0;
    jobs->state = UNKNOWN;
    jobs->commandline[0] = '\0';
}
/*
 * set up at the beginning.
 * initialize all variables in struct job_t.
 */
void setup(struct job_t *jobs) {
    int i=0;
    while(i < MAXJOBS){
        remove_helper(&jobs[i]);
        i++;
    }
}

/*
 * print the job list, which contains job id, pid, and state.
 */
void printjobs(struct job_t *jobs)
{
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] ", jobs[i].jobID);
            if(jobs[i].state == BACKGROUND)
                printf("Running ");
            else if(jobs[i].state == STOPPED)
                printf("Stopped ");
            else{
                printf("error happens.\n");
            }
            printf("%s", jobs[i].commandline);
        }
    }
}
/*
 * command 'bg job_id' resumes a stopped command,
 * the status should be Running.
 */
void bgCMD(Cmd *cmd)
{
    struct job_t *job;
    char *id =cmd->args[1];

    if(id==NULL){
        printf("argument error.\n");
        return;
    }
    if(!isdigit(id[0])){
        printf("command format should be 'bg <number>'\n");
        return;
    }
    else{
        job = get_jid(jobs, atoi(&id[0]));
        if(job == NULL){
            printf("there is no such job.\n");
            return;
        }
    }
    kill(-(job->pid),SIGCONT);
    if(!strcmp(cmd->tokenLine,"bg")){
        job->state = BACKGROUND;
        printjobs(job);
    }else{
        job->state = FOREGROUND;
        block_foreground(job->pid);
    }
    return;
}

/*
 * remove a job
 */
int remove_jobs(struct job_t *jobs, pid_t pid)
{
    if (pid < 1)
        return 0;
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            remove_helper(&jobs[i]);
            nextjid = max_jid(jobs)+1;
            return 1;
        }
    }
    return 0;
}
/*
 * When a child process stops or terminates, SIGCHLD is sent to the parent process.
 */
void sigchld_handler(int sig)
{
    pid_t pid;
    int status;
    while((pid = waitpid(-1,&status,WNOHANG|WUNTRACED))>0){
        if(WIFEXITED(status)){
            remove_jobs(jobs,pid);
        }
        if(WIFSIGNALED(status)){
            remove_jobs(jobs,pid);
        }
        if(WIFSTOPPED(status)){
            struct job_t *job = get_pid(jobs,pid);
            if(job !=NULL ){
                job->state = STOPPED;
            }
        }
    }
    return;
}

pid_t foregroundPid = 0;
struct job_t jobs[MAXJOBS];

/* Parses the command string contained in cmd->line.
 * Assumes all fields in cmd (except cmd->line) are initailized to zero.
 * On return, all fields of cmd are appropriatly populated. */
void parseCmd(Cmd* cmd)
{
    char* token;
    int i=0;
    strcpy(cmd->tokenLine, cmd->line);
    strtok(cmd->tokenLine, "\n");
    token = strtok(cmd->tokenLine, " ");
    while (token != NULL){
        if (*token == '\n') {
            cmd->args[i] = NULL;
        }
        else if (*token == REDIRECT_OUT_OP || *token == REDIRECT_IN_OP || *token == PIPE_OP || *token == BG_OP) {
            cmd->symbols[i] = token;
            cmd->args[i] = NULL;
        }
        else {
            cmd->args[i] = token;
        }
        token = strtok(NULL, " ");
        i++;
    }
    cmd->args[i] = NULL;
}

/* Signal handler for SIGTSTP (SIGnal - Terminal SToP),
* which is caused by the user pressing control+z. */
void sigtstpHandler(int sig_num)
{
    /* Reset handler to catch next SIGTSTP. */
    signal(SIGTSTP, sigtstpHandler);
    if (foregroundPid > 0) {
        /* Foward SIGTSTP to the currently running foreground process. */
        printf("[User presses control+z.]\n");
        kill(foregroundPid, SIGTSTP);
    }
}

int main(void)
{
    signal(SIGTSTP, sigtstpHandler);
    signal(SIGCHLD, sigchld_handler);
    setup(jobs);
    while (1) {
        printf("352> ");
        fflush(stdout);
        Cmd *cmd = (Cmd*) calloc(1, sizeof(Cmd));
        fgets(cmd->line, MAX_LINE, stdin);
        parseCmd(cmd);
        if (!cmd->args[0]) {
            free(cmd);
        }
        else if (strcmp(cmd->args[0], "exit") == 0) {
            free(cmd);
            exit(0);
        }
        else if(!strcmp(cmd->args[0],"jobs") ){
            printjobs(jobs);
        }
        else if(!(strcmp(cmd->args[0],"bg"))){
            bgCMD(cmd);
        }
        else{
            if (findSymbol(cmd, BG_OP) != -1) {
                backgroundCMG(cmd);
            }
            else {
                foregroundCMD(cmd);
            }
        }
    }
    return 0;
}
