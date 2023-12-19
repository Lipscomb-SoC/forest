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
using std::string_view;
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

set<string> visited;
queue<string> todo;

bool you_walk(string_view s) { return s.find("You cannot walk ")==string::npos; }
bool you_take(string_view s) { return s.find("There is no ")==string::npos; }
bool use_item(string_view s) { return s.find("You can't do ")==string::npos; }
bool you_find(string_view s) { return s.find("You didn't find")==string::npos; }
struct action {
    string  cmd;
    bool    (*success)(string_view s);
};

vector<action> cmds {
    {"n"                ,you_walk},
    {"e"                ,you_walk},
    {"s"                ,you_walk},
    {"w"                ,you_walk},
    {"take coin"        ,you_take},
    {"take statuette"   ,you_take},
    {"take hammer"      ,you_take},
    {"take sword"       ,you_take},
    {"take ladder"      ,you_take},
    {"take key"         ,you_take},
    {"take hamburger"   ,you_take},
    {"take secpter"     ,you_take},
    {"use coin"         ,use_item},
    {"use statuette"    ,use_item},
    {"use hammer"       ,use_item},
    {"use sword"        ,use_item},
    {"use ladder"       ,use_item},
    {"use key"          ,use_item},
    {"use hamburger"    ,use_item},
    {"use secpter"      ,use_item},
    {"find"             ,you_find},
};

int main(int argc,char *argv[])
{
    bool verbose = false;
    bool help = false;
    bool save = false;
    int depth = 9999;

    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg == "-v")
            verbose = true;
        else if (arg == "-n") {
            if (i == argc-1)
                help = true;
            else {
                depth = std::stoi(argv[i+1]);
                i++;
            }
        }
        else if (arg == "-h")
            help = true;
        else if (arg == "-s")
            save = true;
    }

    (void)save;

    if (help) {
        std::cout << "usage explore [options]\n"
                     "  -h     print this help message, stop\n"
                     "  -v     be verbose, trace execution\n"
                     "  -s     save results for later comparision\n"
                     "  -n #   maximum number of commands to use\n";
        exit(1);
    }

    todo.push("");
    //visited.insert(std::hash<string>{}(run("","look\n")));
    visited.insert(run("","look\n"));

    int d = 0;
    while (!todo.empty()) {
        auto setup = todo.front(); todo.pop();
        std::cout << setup; //.length() << " " << std::flush;

        for (auto c:cmds) {
            string newcmd = c.cmd + "\n";
            auto s = run(setup,newcmd);
            if (verbose)
                std::cout << setup << newcmd << "--\n" << s << "========\n";
            if (c.success(s)) {
                s = s.substr(s.find("\n",1));
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
                //auto h = std::hash<string>{}(s);
                if (!visited.contains(s)) {
                    visited.insert(s);
                    todo.push(setup+newcmd);
                }
            }
        }

        d++;
        if (d >= depth) break;
    }

    for (auto x:visited)
        std::cout << x << "\n------\n";

    std::cout << "\n\ntotal places: " << visited.size() << "\n";
}
