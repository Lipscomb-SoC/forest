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
#include <algorithm>
using std::string;
using std::string_view;
using std::map;
using std::set;
using std::queue;
using std::vector;
using std::pair;
using std::ranges::count;

void die(const char *msg)
{
    fprintf(stderr,"%s (%d)\n",msg,errno);
    exit(1);
}

struct state_t {
    string      location;   // description
    set<string> items;      // list of items

    std::strong_ordering operator<=>(const state_t &other) const {
        if (auto c = location <=> other.location; c!=0) return c;
        return items <=> other.items;
    };
};

struct response_t {
    string      response;   // response to last command
    state_t     state;

    std::strong_ordering operator<=>(const response_t &other) const {
        if (auto c = response <=> other.response; c!=0) return c;
        return state <=> other.state;
    };
};

string exchange(int in,int out,string msg,int extra=0)
{
    assert(msg=="" || msg.back()=='\n');

    // write msg to game input (inpipe[1])
    if (write(in,msg.c_str(),msg.length()) != (int)msg.length()) die("writing to game failed");

    // poll for data until the prompt shows up
    const string prompt = "\ncommand> ";
    string response;
    int loops = 0;
    while (1) {
        char buf[4096];
        ssize_t len = read(out,buf,sizeof(buf)-1);
        if (len > 0) {
            buf[len] = 0;
            response += buf;
        }
        else
            loops++;
        if (loops>extra && response.ends_with(prompt))
            return response.substr(0,response.length()-prompt.length());
        usleep(10000); // 0.01 seconds
    }
}

response_t run(string setup,string command)
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

    response_t r;
    string items;
    pid_t pid = fork();
    if (pid < 0)
        die("fork failed");
    else if (pid > 0) {
        close(inpipe[0]);
        close(outpipe[1]);
        // parent:

        // write setup commands to game input and ignore results
        exchange(inpipe[1],outpipe[0],setup,2);

        // write new command to game
        r.response = exchange(inpipe[1],outpipe[0],command);

        // write look command to game (to get current location)
        r.state.location = exchange(inpipe[1],outpipe[0],"look\n");

        // write inventory command to game (to get item list)
        items = exchange(inpipe[1],outpipe[0],"inv\n");

        // we're done
        close(inpipe[1]);
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

    // parse items string: "Inventory:\n \t ITEM \n \t ITEM \n ..."
    while (!items.empty()) {
        auto n = items.find("\t");
        if (n == string::npos) break;

        // remove upto the first \t
        items = items.substr(n+1);
        // upto \n is the next item
        string item = items.substr(0,items.find("\n"));
        if (item != "nothing.")
            r.state.items.insert(item);
    }

    return r;
}

/*
s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); })); 
s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
*/

struct value_t {
    string  cmds;       // how we got here
    string  response;   // last response
    bool    success;    // was it "good"?
};

map<state_t,value_t> visited;
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
    visited.insert(pair{state_t{"",{}},""});

    while (!todo.empty()) {
        auto setup = todo.front(); todo.pop();
        std::cout << count(setup,'\n') << " " << std::flush;

        for (auto c:cmds) {
            auto newcmd = c.cmd+"\n";
            auto r = run(setup,newcmd);
            if (verbose)
                std::cout << newcmd << "--\n" << r.response << "========\n";
            if (!visited.contains(r.state)) {
                auto good = c.success(r.response);
                visited.insert(pair{r.state,value_t{setup+newcmd,r.response,good}});
                if (good && count(setup,'\n') < depth)
                    todo.push(setup+newcmd);
            }
        }
    }
    std::cout << std::endl << std::endl;

    for (auto const &x:visited) {
        string seq{x.second.cmds};
        std::ranges::replace(seq,'\n',';');
        std::cout << seq << " ==> " << x.second.response << x.first.location << "Items:";
        for (auto const &y:x.first.items)
            std::cout << ", " << y;
        std::cout << "\n------\n";
    }

    std::cout << "\n\ntotal places: " << visited.size() << "\n";
}
