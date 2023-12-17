#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

void die(const char *msg)
{
    fprintf(stderr,"%s (%d)\n",msg,errno);
    exit(1);
}

char buf[16*1024];

int main()
{
    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) || pipe(outpipe)) die("pipe creation failed");

    // increase pipe size to avoid all I/O blocking issues
    fcntl(inpipe[0],F_SETPIPE_SZ,1024*1024);
    fcntl(outpipe[0],F_SETPIPE_SZ,1024*1024);
    if (fcntl(outpipe[0], F_SETFL, O_NONBLOCK) < 0) die("set nonblocking failed");

    pid_t pid = fork();
    if (pid < 0)
        die("fork failed");
    else if (pid > 0) {
        close(inpipe[0]);
        close(outpipe[1]);
        // parent: 
        // copy stdin to inpipe[1] except last line, ignore outpipe[0] 
        // send last line, copy outpipe to stdout
        
        // read all of stdin, find beginning of last line
        int len = read(0,buf,sizeof(buf));
        char *s = strrchr(buf,'\n');
        if (s[1]) {
            // next char is not null, so append a newline, s points to last line
            strcat(buf,"\n");
            s++;
        }
        else {
            // this is the end, so backup to beginning of line
            while (s>buf && s[-1]!='\n')
                s--;
            s++;
        }

        // write all commands to game input
        if (write(inpipe[1],buf,s-buf) != s-buf) die("write to game stdin failed");
        
        // give it chance to compute, ignore game output
        usleep(100000); // 0.1 seconds
        char dummy[1024];
        while ((len=read(outpipe[0],dummy,sizeof(dummy))) > 0)
            ; //if (write(1,buf,len) != len) die("copying stdout failed");

        // write last line to game
        if (write(inpipe[1],s,strlen(s)) != strlen(s)) die("writing final line failed");
        close(inpipe[1]);
        usleep(100000); // 0.1 seconds

        while ((len=read(outpipe[0],buf,sizeof(buf))) > 0)
            if (write(1,buf,len) !=len) die("writing stdout failed");
        close(outpipe[0]);
        kill(pid,SIGKILL);
    }
    else {
        // child:
        // read from inpipe[0]
        // write to outpipe[1]
        if (dup2(inpipe[0],0)==-1 || dup2(outpipe[1],1)==-1) die("file dup'ing failed");
        close(inpipe[1]);
        close(inpipe[0]);
        close(outpipe[0]);
        close(outpipe[1]);
        execl("forest","forest",NULL);
        die("exec failed");
    }

    return 0;
}
