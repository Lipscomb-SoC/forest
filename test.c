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

int main(int argc,char *argv[])
{
    if (argc < 2) die("usage: run_advent <command>");

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
        // copy stdin to inpipe[1] until end, then send command line
        // ignore outpipe[0] during stdin, copy outpipe to stdout
        char buf[4096];
        int len;
        while ((len=read(0,buf,sizeof(buf))) > 0) {
            if (write(inpipe[1],buf,len) != len) die("write to game stdin failed");
        }
        usleep(100000);

        while ((len=read(outpipe[0],buf,sizeof(buf))) > 0)
            ; //if (write(1,buf,len) != len) die("copying stdout failed");

        buf[0] = 0;
        for (int i=1; i<argc; i++) {
            strcat(buf,argv[i]);
            strcat(buf," ");
        }
        buf[strlen(buf)-1] = '\n';
        buf[strlen(buf)-1] = 0;
        if (write(inpipe[1],buf,strlen(buf)) != strlen(buf)) die("writing command failed");
        close(inpipe[1]);
        usleep(100000);

        while ((len=read(outpipe[0],buf,sizeof(buf))) > 0)
            if (write(1,buf,len) !=len) die("writing stdout failed");
        close(outpipe[0]);
        kill(pid,SIGKILL);
    }
    else {
        // child: read from inpipe[0]; write to outpipe[1]
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
