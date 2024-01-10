#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <vector>
#include <algorithm>
#include <regex>
using std::string;
using std::string_view;
using std::map;
using std::set;
using std::queue;
using std::vector;
using std::pair;
using std::regex;
using std::ranges::count;

void die(const char *msg)
{
    fprintf(stderr,"%s (%d)\n",msg,errno);
    exit(1);
}

string exchange(int in,int out,string msg,int extra=0)
{
    assert(msg=="" || msg.back()=='\n');

    // write msg to game input (inpipe[1])
    if (write(in,msg.c_str(),msg.length()) != (int)msg.length())
        return "";

    // poll for data until the prompt shows up, or EOF
    const string prompt = "\ncommand> ";
    string response;
    int loops = 0;
    while (1) {
        char buf[4096];
        ssize_t len = read(out,buf,sizeof(buf)-1);
        if (len >= 0) {
            buf[len] = 0;
            response += buf;
        }
        else if (len < 0)
            loops++;
        if (loops >= extra) {
            if (len == 0)
                return response;
            if (response.ends_with(prompt))
                return response.substr(0,response.length()-prompt.length());
        }
        usleep(1000); // 0.001 seconds
    }
}

struct outcome_t {
    string      response;   // response to last command
    string      state;      // game state (where & what)
};

outcome_t run(string setup,string command)
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

    outcome_t r;
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

        // write look/inv commands to game (to get current location)
        r.state = exchange(inpipe[1],outpipe[0],"look\ninv\n");

        // we're done
        close(inpipe[1]);
        close(outpipe[0]);
        kill(pid,SIGKILL); // could send a 'q' command?
        waitpid(pid,NULL,0);
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

    return r;
}

struct cmd_t {
    string  cmd;
    int     used;
};

vector<cmd_t> cmds {
    {"find"},
    {"take coin"},
    {"take ladder"},

    {"use hamburger blacksmith"},
    {"use hamburger statue"},
    {"use hammer"},
    {"use red coin kiosk"},
    {"use blue coin kiosk"},
    {"use green coin kiosk"},
    {"use ladder"},
    {"use statuette blacksmith"},
    {"use sword statue"},
    {"use key gate"},
    {"use scepter statue"},
    {"use lever"},

    {"n"},
    {"e"},
    {"s"},
    {"w"},
};

struct pattern_t {
    string  pattern;
    bool    stop;
    int     used;
    regex   re;
};

vector<pattern_t> patterns{
    {"^\\nYou cannot walk (north|south|east|west)\\.\\n$",true},
    {"^\\nThere is no [^\\s]+ here\\.\\n$",true},
    {"^\\nYou can't do that\\.\\n$",true},
    {"^\\nYou cannot take the ladder!\\n$",true},
    {"^\\nYou carefully search the area. You didn't find anything\\.\\n$",true},
    {" You didn't find anything\\.\\n$",true},
    {"^\\nThe [^\\s]+ coin does not fit into the [^\\s]+ slot\\.\\n$",true},
    {"^\\nHe doesn't want to eat that\\.\\n$",true},
    {"The way north is blocked!",true},
    {"CONGRATULATIONS!",true},
    {"GAME OVER: ",true},

    {"^\\nYou walk (north|south|east|west)\\.\\n",false},
    {"^\\nYou walk east into the thicket",false},
    {"^\\nYou walk down the path",false},
    {"^\\nYou walk through the gate\\.",false},
    {"^\\nYou walk (down|up) the stairs",false},
    {"^\\nYou (slowly )?walk deeper ",false},
    {"^\\nYou climb through the passage (east|west)\\.\\n",false},
    {"^\\nYou climb the ladder up the rockslide\\.\\n",false},
    {"^\\nYou (enter|leave|exit) the ",false},
    {"^\\nYou follow the river ",false},
    {"^\\nYou took the [^\\s]+ [^\\s]+\\.\\n$",false},
    {"\\. You found the [^\\s]+ [^\\s]+!\\n",false},
    {" It opened up a path you can",false},
    {"^\\nYou (slide|slip) the [^\\s]+ coin into the slot",false},
    {"^\\nThe blacksmith sees the statuette and his eyes get wide",false},
    {"^\\nYou swing the giant sword at the statue\\.",false},
    {"^\\nYou walk into the (north|west|east) thicket\\.",false},
    {"an underground tunnel that leads north",false},
    {"^\\nYou climb down the ladder and head (south|north) through the tunnel",false},
    {" You can now travel (east|north)!",false},
    {"^\\nYou try to climb down the rocky hill",false},
    {"allowing you to ascend the tower to the (east|west)!\\n",false},
    {"^\\nYou exit through the gate.",false},
    {"you sure this is a good idea?",false},
};

// TODO add initial item locations tracking
// TODO add "look ITEM" description

string pipes(string s)
{
    std::ranges::replace(s,'\n','|');
    return s;
}

string newlines(string s)
{
    std::ranges::replace(s,'|','\n');
    return s;
}

map<string,string> results;     // command sequence ==> recognition pattern
map<string,string> unknowns;    // command sequence ==> response
set<string> visited;            // game states (where & what)
queue<string> todo;             // command sequences to explore

int main(int argc,char *argv[])
{
    bool verbose            = false;
    bool help               = false;
    bool print_locations    = false;
    //bool print_items        = false;
    bool print_paths        = false;
    bool print_unknowns     = false;
    bool print_stats        = false;
    bool test_paths         = false;
    int depth = 100;

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
        else if (arg == "-l")
            print_locations = true;
        // else if (arg == "-i")
        //     print_items = true;
        else if (arg == "-p")
            print_paths = true;
        else if (arg == "-u")
            print_unknowns = true;
        else if (arg == "-s")
            print_stats = true;
        else if (arg == "-t")
            test_paths = true;
        else
            help = true;
    }

    if (!help
    &&  !print_locations
    &&  !print_unknowns
    &&  !print_paths
    &&  !print_stats
    &&  !test_paths
    &&  !verbose)
        help = true;

    if (test_paths && (print_locations || print_paths || print_stats || print_unknowns))
        die("incompatible options\n");

    if (help) {
        std::cout << "usage explore -t|[options]\n"
                     "  -h     print this help message, stop\n"
                     "  -v     be verbose, trace execution\n"
                     "  -n #   maximum number of commands to use\n"
                     "  -l     print unique locations discovered\n"
                     //"  -i     print items discovered\n"
                     "  -p     print successful paths discovered\n"
                     "  -u     print paths with unknown responses\n"
                     "  -s     print statistics\n"
                     "  -t     test enumerated paths from stdin";
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    
    for (auto &r:patterns)
        r.re = r.pattern;

    if (test_paths) {
        string cmds,pattern;
        while (getline(std::cin,cmds) && getline(std::cin,pattern)) {
            auto i=cmds.rfind('|',cmds.length()-2);
            auto setup = cmds.substr(0,i+1);
            auto cmd = cmds.substr(i+1);
            if (verbose)
                std::cout << setup << " " << cmd << "\n" << pattern << "\n";
            auto r = run(newlines(setup),newlines(cmd));
            if (!std::regex_search(r.response,regex{pattern})) {
                std::cout << setup << " " << cmd << " produced " << r.response << pattern << " was expected\n\n";
                exit(1);
            }
        }
    }
    else {
        // go exploring
        todo.push("");
        while (!todo.empty()) {
            auto setup = todo.front(); todo.pop();
            if (verbose)
                std::cout << "sequence length: " << count(setup,'\n') << " , queue size: " << todo.size() << std::endl;

            for (auto &c:cmds) {
                auto newcmd = c.cmd+"\n";
                auto r = run(setup,newcmd);
                //if (verbose)
                //    std::cout << semicolons(newcmd) << "----\n" << r.response << "====\n";

                auto cmds = setup+newcmd;
                pattern_t * p = nullptr;
                for (size_t i=0; !p && i<patterns.size(); i++)
                    if (std::regex_search(r.response,patterns[i].re))
                        p = &patterns[i];

                if (p) {
                    results.insert({cmds,p->pattern});
                    p->used++;
                    if (!p->stop)
                        c.used++;
                }
                else
                    unknowns.insert({cmds,r.response});

                if (!visited.contains(r.state)) {
                    visited.insert(r.state);
                    if (p && !p->stop && count(cmds,'\n') < depth)
                        todo.push(cmds);
                }
            }
        }
    }
    if (verbose)
        std::cout << std::endl << std::endl;

    // create summaries

    // set<string> items;
    // for (auto const &x:visited)
    //     for (auto const &y:x.first.items)
    //         items.insert(y);
    // regex: Inventory:\n \t item \n \t ...

    map<string,int> locations;
    for (auto const &s:visited)
        if (s.length() > 0)
            locations[s.substr(1,s.find('\n',2)-1)]++;  // fmt: \nPlace Name\n

    // reports 

    // if (print_items) {
    //     std::cout << "\nItems:\n";
    //     for (auto const &x:items)
    //         std::cout << x << "\n";
    //     std::cout << "----\n";
    // }

    if (print_paths) {
        for (auto const &x:results)
            std::cout << pipes(x.first) << "\n" << x.second << "\n";
    }

    if (print_locations) {
        for (auto const &x:locations)
            std::cout << x.second << " " << x.first << "\n";
    }

    if (print_unknowns) {
        for (auto const &x:unknowns)
            std::cout << pipes(x.first) << "\n" << pipes(x.second) << "\n";
    }

    // consider game states (locations + item sets)

    if (print_stats) {
        std::cout << "\n";
        //std::cout << "items: "              << items.size() << std::endl;
        std::cout << "discovered paths: "   << results.size() << std::endl;
        std::cout << "locations: "          << locations.size() << std::endl;
        std::cout << "game states: "        << visited.size() << std::endl;
        std::cout << "unknown responses: "  << unknowns.size() << std::endl;
        std::cout << "\ncommand usage:\n";
        for (auto const &x:cmds)
            std::cout << "  " << x.used << " " << x.cmd << "\n";
        std::cout << "\npattern usage:\n";
        for (auto const &x:patterns)
            std::cout << "  " << x.used << " " << x.pattern << "\n";
    }
}
