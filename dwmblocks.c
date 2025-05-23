#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include<errno.h>
#ifndef NO_X
#include<X11/Xlib.h>
#endif
#ifdef __OpenBSD__
#define SIGPLUS			SIGUSR1+1
#define SIGMINUS		SIGUSR1-1
#else
#define SIGPLUS			SIGRTMIN
#define SIGMINUS		SIGRTMIN
#endif
#define LENGTH(X)               (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH		50
#define MIN( a, b ) ( ( a < b) ? a : b )
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

typedef struct {
	char* icon;
	char* command;
	unsigned int interval;
	unsigned int signal;
} Block;
#ifndef __OpenBSD__
void dummysighandler(int num);
#endif
void sighandler(int num);
void buttonhandler(int sig, siginfo_t *si, void *ucontext);
void remove_all(char *str, char to_remove);
void getcmds(int time);
void getsigcmds(unsigned int signal);
void setupsignals();
void sighandler(int signum);
int getstatus(char *str, char *last);
void statusloop();
void termhandler(int num);
void pstdout();
#ifndef NO_X
void setroot();
static void (*writestatus) () = setroot;
static int setupX();
static Display *dpy;
static int screen;
static Window root;
#else
static void (*writestatus) () = pstdout;
#endif


#include "blocks.h"

static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;
//static int returnStatus = 0;

void remove_all(char *str, char to_remove) {
    char *read = str;
    char *write = str;
    while (*read) {
        if (*read != to_remove) {
            *write++ = *read;
        }
        ++read;
    }
    *write = '\0';
}


//opens process *cmd and stores output in *output
void getcmd(const Block *block, char *output)
{
	if (block->signal)
    {
        output[0] = block->signal;
        output++;
    }
    char *cmd = block->command;
    FILE *cmdf = popen(cmd,"r");
    if (!cmdf){
        //printf("failed to run: %s, %d\n", block->command, errno);
        return;
    }
    char tmpstr[CMDLENGTH] = "";
    // TODO decide whether its better to use the last value till next time or just keep trying while the error was the interrupt
    // this keeps trying to read if it got nothing and the error was an interrupt
    //  could also just read to a separate buffer and not move the data over if interrupted
    //  this way will take longer trying to complete 1 thing but will get it done
    //  the other way will move on to keep going with everything and the part that failed to read will be wrong till its updated again
    // either way you have to save the data to a temp buffer because when it fails it writes nothing and then then it gets displayed before this finishes
    char * s;
    int e;
    do {
        errno = 0;
        s = fgets(tmpstr, CMDLENGTH-(strlen(delim)+1), cmdf);
        e = errno;
    } while (!s && e == EINTR);
    pclose(cmdf);
    int i = strlen(block->icon);
    strcpy(output, block->icon);
    strcpy(output+i, tmpstr);
    remove_all(output, '\n');
    i = strlen(output);
    if ((i > 0 && block != &blocks[LENGTH(blocks)])){
        strcat(output, delim);
    }
    i+=strlen(delim);
    output[i++] = '\0';

}

void getcmds(int time)
{
	const Block* current;
	for (unsigned int i = 0; i < LENGTH(blocks); i++) {
		current = blocks + i;
		if ((current->interval != 0 && time % current->interval == 0) || time == -1)
			getcmd(current,statusbar[i]);
	}
}

void getsigcmds(unsigned int signal)
{
	const Block *current;
	for (unsigned int i = 0; i < LENGTH(blocks); i++) {
		current = blocks + i;
		if (current->signal == signal)
			getcmd(current,statusbar[i]);
	}
}

void setupsignals()
{
#ifndef __OpenBSD__
	    /* initialize all real time signals with dummy handler */
    for (int i = SIGRTMIN; i <= SIGRTMAX; i++)
        signal(i, dummysighandler);
#endif

    struct sigaction sa;
	for (unsigned int i = 0; i < LENGTH(blocks); i++) {
		if (blocks[i].signal > 0) {
			signal(SIGMINUS+blocks[i].signal, sighandler);
            sigaddset(&sa.sa_mask, SIGRTMIN+blocks[i].signal);
        }
	}
    sa.sa_sigaction = buttonhandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);
    struct sigaction sigchld_action = {
        .sa_handler = SIG_DFL,
        .sa_flags = SA_NOCLDWAIT
    };
    sigaction(SIGCHLD, &sigchld_action, NULL);

}

int getstatus(char *str, char *last)
{
	strcpy(last, str);
	str[0] = '\0';
	for (unsigned int i = 0; i < LENGTH(blocks); i++)
		strcat(str, statusbar[i]);
	str[strlen(str)-strlen(delim)] = '\0';
	return strcmp(str, last);//0 if they are the same
}

#ifndef NO_X
void setroot()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only set root if text has changed.
		return;
	XStoreName(dpy, root, statusstr[0]);
	XFlush(dpy);
}

int setupX()
{
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "dwmblocks: Failed to open display\n");
		return 0;
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	return 1;
}
#endif

void pstdout()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only write out if text has changed.
		return;
	printf("%s\n",statusstr[0]);
	fflush(stdout);
}


void statusloop()
{
	setupsignals();
	int i = 0;
	getcmds(-1);
	while (1) {
		getcmds(i++);
		writestatus();
		if (!statusContinue)
			break;
		sleep(1.0);
	}
}

#ifndef __OpenBSD__
/* this signal handler should do nothing */
void dummysighandler(int signum)
{
    return;
}
#endif

void sighandler(int signum)
{
	getsigcmds(signum-SIGPLUS);
	writestatus();
}

// Adding clickable blocks
void buttonhandler(int sig, siginfo_t *si, void *ucontext)
{
    char button[2] = {'0' + (si->si_value.sival_int & 0xff), '\0'};
    pid_t process_id = getpid();
    sig = si->si_value.sival_int >> 8;
    if (fork() == 0) {
        const Block *current;
        for (int i = 0; i < LENGTH(blocks); i++) {
            current = blocks + i;
            if (current->signal == sig)
                break;
        }
        char shcmd[1024];
        sprintf(shcmd,"%s && kill -%d %d",current->command, current->signal+34,process_id);
        char *command[] = { "/bin/sh", "-c", shcmd, NULL };
        setenv("BLOCK_BUTTON", button, 1);
        setsid();
        execvp(command[0], command);
        exit(EXIT_SUCCESS);
    }
}


void termhandler(int num)
{
	statusContinue = 0;
}

int main(int argc, char** argv)
{
	for (int i = 0; i < argc; i++) {//Handle command line arguments
		if (!strcmp("-d",argv[i]))
			strncpy(delim, argv[++i], delimLen);
		else if (!strcmp("-p",argv[i]))
			writestatus = pstdout;
	}
#ifndef NO_X
	if (!setupX())
		return 1;
#endif
	delimLen = MIN(delimLen, strlen(delim));
	delim[delimLen++] = '\0';
	signal(SIGTERM, termhandler);
	signal(SIGINT, termhandler);
	statusloop();
#ifndef NO_X
	XCloseDisplay(dpy);
#endif
	return 0;
}
