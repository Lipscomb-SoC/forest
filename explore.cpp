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

struct state_t {
    string      location;   // room name
    set<string> items;      // inventory

    std::strong_ordering operator<=>(const state_t &other) const {
        if (auto c = location <=> other.location; c!=0) return c;
        return std::lexicographical_compare_three_way(items.begin(),items.end(),other.items.begin(),other.items.end());
    };
};

struct response_t {
    string      response;   // response to last command
    state_t     state;      // game state (where & what)

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
        auto s = exchange(inpipe[1],outpipe[0],"look\n");
        r.state.location = s.substr(1,s.find('\n',2)-1);  // fmt: \nPlace Name\n

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

vector<string> cmds {
    "find",
    "take coin",
    "take ladder",
    "take sword",
    "take statuette",
    "take key",
    "take scepter",

    "use hamburger",
    "use hammer",

    "use red coin kiosk",
    "use blue coin kiosk",
    "use green coin kiosk",
    "use ladder",
    "use statuette",
    "use sword statue",
    "use key gate",
    "use scepter",

    "n",
    "e",
    "s",
    "w",
};

vector<string> bad_responses{
    "^\\nYou cannot walk (north|south|east|west)\\.\\n$",
    "^\\nThere is no [^\\s]+ here\\.\\n$",
    "^\\nYou can't do that\\.\\n$",
    "^\\nYou carefully search the area. You didn't find anything\\.\\n$",
    "^\\nYou stealthily inspect the items on the tables\\. You didn't find anything\\.\\n$",
    "^\\nYou rummage through the junk pile\\. You didn't find anything\\.\\n$"
    "^\\nYou browse around the shop, looking for merchandise\\. You didn't find anything\\.\\n$"
    "^\\nThe [^\\s]+ coin does not fit into the [^\\s]+ slot\\.\\n$",
};
vector<regex> bad_patterns;

vector<string> good_responses{
    // walking/moving
    "^\\nYou walk (north|south|east|west)\\.\\n$",
    "^\\nYou walk east into the thicket",
    "^\\nYou walk down the path",
    "^\\nYou walk (down|up) the stairs",
    "^\\nYou (slowly )?walk deeper ",
    "^\\nYou (crawl|climb) through ",
    "^\\nYou (enter|leave|exit) the ",
    "^\\nYou follow the river ",

    // find/take items
    " You found the .+!",
    "^\nYou took the .+\\.",

    // use items
    " It opened up a path you can", // hammer
    "^\nYou slide the .+ coin into the slot", 
};
vector<regex> good_patterns;

// TODO add initial item locations tracking
// TODO add "look ITEM" description

string use_commas(string s)
{
    std::ranges::replace(s,'\n',';');
    return s;
}

map<string,string> known_responses;     // command sequence ==> recognition pattern
map<string,string> unknown_responses;   // command sequence ==> response
map<state_t,string> visited;            // game state ==> command sequence (that got us here)
queue<string> todo;

int main(int argc,char *argv[])
{
    bool verbose            = false;
    bool help               = false;
    bool print_locations    = false;
    bool print_items        = false;
    bool print_paths        = false;
    bool print_responses    = false;
    bool print_stats        = false;
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
        else if (arg == "-l")
            print_locations = true;
        else if (arg == "-i")
            print_items = true;
        else if (arg == "-p")
            print_paths = true;
        else if (arg == "-r")
            print_responses = true;
        else if (arg == "-s")
            print_stats = true;
        else
            help = true;
    }

    //(void)save;

    if (help) {
        std::cout << "usage explore [options]\n"
                     "  -h     print this help message, stop\n"
                     "  -v     be verbose, trace execution\n"
                     "  -n #   maximum number of commands to use\n"
                     "  -l     print unique locations discovered\n"
                     "  -i     print items discovered\n"
                     "  -p     print successful paths discovered\n"
                     "  -r     print paths with unknown responses\n"
                     "  -s     print statistics\n";
        exit(1);
    }

    for (auto const &x:good_responses)
        good_patterns.push_back(regex(x));
    for (auto const &x:bad_responses)
        bad_patterns.push_back(regex(x));

    todo.push("");
    visited.insert(pair{state_t{"",{}},""});

    while (!todo.empty()) {
        auto setup = todo.front(); todo.pop();
        if (verbose)
            std::cout << "considering " << use_commas(setup) << ", queue size = " << todo.size() << std::endl;

        for (auto const &cmd:cmds) {
            auto newcmd = cmd+"\n";
            auto r = run(setup,newcmd);
            if (verbose)
                std::cout << use_commas(newcmd) << "----\n" << r.response << "====\n";

            int good_index = -1;
            for (size_t i=0; good_index<0 && i<good_patterns.size(); i++)
                if (std::regex_search(r.response,good_patterns[i]))
                    good_index = i;

            int bad_index = -1;
            for (size_t i=0; bad_index<0 && i<bad_patterns.size(); i++)
                if (std::regex_search(r.response,bad_patterns[i]))
                    bad_index = i;

            if (good_index>=0)
                known_responses.insert(pair{setup+newcmd,good_responses[good_index]});
            else if (bad_index>=0)
                known_responses.insert(pair{setup+newcmd,bad_responses[bad_index]});
            else
                unknown_responses.insert(pair{setup+newcmd,r.response});

            if (!visited.contains(r.state)) {
                visited.insert(pair{r.state,setup+newcmd});
                if (good_index<(int)good_patterns.size() && count(setup,'\n') < depth)
                    todo.push(setup+newcmd);
            }
        }
    }
    if (verbose)
        std::cout << std::endl << std::endl;

    // create summaries
    set<string> items;
    for (auto const &x:visited)
        for (auto const &y:x.first.items)
            items.insert(y);

    set<string> paths;
    for (auto const &x:visited)
        paths.insert(x.second);

    set<string> locations;
    for (auto const &x:visited)
        locations.insert(x.first.location);

    // reports 
    if (print_items) {
        for (auto const &x:items)
            std::cout << x << "\n";
        std::cout << "----\n";
    }

    if (print_paths) {
        for (auto const &x:known_responses)
            std::cout << use_commas(x.first) << "\n" << x.second << "\n";
        std::cout << "----\n";
    }

    if (print_locations) {
        std::cout << "\nLocations:\n";
        for (auto const &x:locations)
            std::cout << x << "\n";
    }

    if (print_responses) {
        std::cout << "\nUnknown responses:\n";
        for (auto const &x:unknown_responses)
            std::cout << use_commas(x.first) << x.second << "----\n";
    }

    // consider game states (locations + item sets)

    if (print_stats) {
        std::cout << "items: "              << items.size() << std::endl;
        std::cout << "paths: "              << paths.size() << std::endl;
        std::cout << "locations: "          << locations.size() << std::endl;
        std::cout << "game states: "        << visited.size() << std::endl;
        std::cout << "unknown responses: "  << unknown_responses.size() << std::endl;
    }
}
