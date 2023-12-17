#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <vector>
using std::string;
using std::map;
using std::set;
using std::queue;
using std::vector;

void die(const char *msg)
{
    fprintf(stderr,"%s (%d)\n",msg,errno);
    exit(1);
}

string run(string setup,string command)
{
    assert(setup=="" || setup.back()=='\n');
    assert(command.back()=='\n');

    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) || pipe(outpipe)) die("pipe creation failed");

    // increase pipe size to avoid all I/O blocking issues
    fcntl(inpipe[0],F_SETPIPE_SZ,1024*1024);
    fcntl(outpipe[0],F_SETPIPE_SZ,1024*1024);
    if (fcntl(outpipe[0], F_SETFL, O_NONBLOCK) < 0) die("set nonblocking failed");

    string result;
    pid_t pid = fork();
    if (pid < 0)
        die("fork failed");
    else if (pid > 0) {
        close(inpipe[0]);
        close(outpipe[1]);
        // parent: 
        // send setup to inpipe[1], ignore outpipe[0] 
        // send command, copy outpipe to result
        char buf[4096];

        // write all commands to game input
        if (write(inpipe[1],setup.c_str(),setup.length()) != (int)setup.length()) die("writing setup failed");
        
        // give it a chance to compute, then ignore game output
        usleep(100000); // 0.1 seconds
        while (read(outpipe[0],buf,sizeof(buf)) > 0)
            ;

        // write command to game
        if (write(inpipe[1],command.c_str(),command.length()) != (int)command.length()) die("writing command failed");
        close(inpipe[1]);
        usleep(100000); // 0.1 seconds

        while (auto len=read(outpipe[0],buf,sizeof(buf)-1)) {
            buf[len] = 0;
            result += buf;
        }
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

    return result;
}

set<size_t> visited;
queue<string> todo;
vector<string> cmds {
    "n",
    "e",
    "s",
    "w",
    "take coin",
    "take statuette",
    "take hammer",
    "take sword",
    "take ladder",
    "take key",
    "take hamburger",
    "take secpter",
    "use coin",
    "use statuette",
    "use hammer",
    "use sword",
    "use ladder",
    "use key",
    "use hamburger",
    "use secpter",
    "find",
};

int main(int,char *[])
{
    todo.push("");
    visited.insert(std::hash<string>{}(run("","look\n")));

    while (!todo.empty()) {
        auto setup = todo.front(); todo.pop();
        std::cout << setup.length() << " " << std::flush;
        for (auto c:cmds) {
            c = c + "\n";
            auto s = run(setup,c);
            //std::cout << s;
            auto h = std::hash<string>{}(s);
            if (visited.find(h) == visited.end()) {
                visited.insert(h);
                todo.push(setup+c);
            }
        }

        // if (visited.size() > 100) break;
    }
    std::cout << "\n\ntotal places: " << visited.size() << "\n";
    //string s = run("n\nn\nn\ne\nw\nw\nn\ntake coin\ns\ne\n","e\n");
    //std::cout << s;
    //std::cout << str_hash << "\n";
}