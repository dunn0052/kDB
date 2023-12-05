#include <sys/mman.h>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <sys/syscall.h> // __NR_getid
#include <DatabaseAccess.hh>
#include <random>
#include <sys/wait.h>
#include <sys/shm.h>

int64_t TIME = 1;

std::string random_char()
{
     std::string str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");

     std::random_device rd;
     std::mt19937 generator(rd());


     std::shuffle(str.begin(), str.end(), generator);

     return str.substr(0, 1);
}
void writeValue(OBJECT& db, RECORD id, long long* totals)
{

    DatabaseAccess database = DatabaseAccess(db);
    std::stringstream g;
    pid_t pid = syscall(__NR_getpid);
    g << "Starting writing process: " << pid << "\n";
    //std::cout << g.str();
    std::vector<std::tuple<OFRI, std::string>> vals = {
        {{"BASS", 0, id, 0}, random_char()},
        {{"BASS", 1, id, 0}, random_char()},
        {{"BASS", 2, id, 0}, random_char()},
        {{"BASS", 3, id, 0}, random_char()},
    };

    g.str("");
    g << "Writing: " << std::get<1>(vals[0]) << std::get<1>(vals[1]) << std::get<1>(vals[2]) << std::get<1>(vals[3])<< " to BASS " << id << "\n";
    std::cout << g.str();
#if 0
    for(int i = 0; i < 1000000; i++)
    {
        RETCODE ret = database.WriteValue(vals);
        if(RTN_OK != ret)
        {
            std::cout << "Invalid WRITE due to: " << ret << "\n";
            break;
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#endif
    bool running = true;

    std::chrono::time_point start = std::chrono::steady_clock::now();
    while(running)
    {
        database.WriteValue(vals);
        (*totals)++;
        if(std::chrono::steady_clock::now() - start > std::chrono::seconds(TIME))
        {
            running = false;
        }
    }
    std::cout << "Wrote values for process: " << pid << " " << *totals << " times\n";

    std::stringstream s;
    s << "Ending writing process: " << pid << "\n";
    //std::cout << s.str();
    return;
}

void readValue(OBJECT& db, RECORD id, long long* totals)
{
    std::stringstream g;
    g << "Starting reading process: " << syscall(__NR_getpid) << "\n";
    //std::cout << g.str();

    DatabaseAccess database = DatabaseAccess(db);
    std::vector<std::string> output;
    std::vector<std::string> oldOutput;
    pid_t pid = syscall(__NR_getpid);
    size_t updateCount = 0;

    std::vector<OFRI> keys = {
        {"BASS", 0, id, 0},
        {"BASS", 1, id, 0},
        {"BASS", 2, id, 0},
        {"BASS", 3, id, 0}
    };
#if 0
    for(int i = 0; i < 1000000; i++)
    {
        RETCODE ret = database.ReadValue(keys, output);
        if(RTN_OK != ret)
        {
            std::cout << "Invalid read due to: " << ret << "\n";
            break;
        }
        //std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
#endif
    bool running = true;

    std::chrono::time_point start = std::chrono::steady_clock::now();
    while(running)
    {
        database.ReadValue(keys, output);
        (*totals)++;
        if(std::chrono::steady_clock::now() - start > std::chrono::seconds(TIME))
        {
            running = false;
        }
    }

    std::cout << "Read values for process: " << pid << " " << *totals << " times\n";

    std::stringstream s;
    s << "Ending reading process: " << syscall(__NR_getpid) << "\n";
    //std::cout << s.str();
    return;
}

int main(int argc, char* argv[])
{

    if(argc == 2)
    {
        TIME = std::stoi(argv[1]);
    }

    OBJECT db = {"BASS"};
    DatabaseAccess database = DatabaseAccess(db);
    RECORD total = database.NumRecords() * 2;
    key_t key;
    int shm_id;
    long long* totals = (long long*)shmat(shm_id, NULL, 0);
    key=ftok("~/.bashrc",1);
    shm_id = shmget(key, total*sizeof(long long), 0666 | IPC_CREAT);
    totals = (long long*)shmat(shm_id, NULL, 0);
    memset(totals, 0, total * sizeof(long long));

    pid_t* pids = new pid_t[total];

    for(int i = 0; i < total; i++)
    {
        if(i % 2)
        {
            static RECORD id = 0;
            pids[i] = fork();
            if(pids[i] == 0)
            {
                readValue(db, id % database.NumRecords(), &totals[i]);
                return 0;
            }
            else
            {
                id++;
                continue;
            }
        }
        else
        {
            static RECORD id = 0;
            pids[i] = fork();
            if(pids[i] == 0)
            {
                writeValue(db, id % database.NumRecords(), &totals[i]);
                return 0;
            }
            else
            {
                id++;
                continue;
            }
        }
    }

    for (int i = 0; i < total; ++i) {
        int status;
        while (-1 == waitpid(pids[i], &status, 0));
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::cerr << "Process " << i << " (pid " << pids[i] << ") failed\n";
            exit(1);
        }
    }

    long long totalReads = 0;
    long long totalWrites = 0;

    for(int i = 0; i < total; i++)
    {
        if(i %2)
        {
            totalReads += totals[i];
        }
        else
        {
            totalWrites += totals[i];

        }
    }

    std::cout << "Wrote: " << totalWrites << " entries\n";
    std::cout << "Read: " << totalReads << " entries\n";
    std::cout << "Total transactions: " << totalReads + totalWrites << " in " << TIME << " second(s)\n";

    return 0;
}