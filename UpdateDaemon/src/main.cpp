#include <CLI.hh>

#include <UpdateDeamon.hh>


int main(int argc, char* argv[])
{
    CLI::Parser parse("UpdateDaemon", "Manages reading and writing of DBs");
    CLI::CLI_OBJECTArgument objectArg("--object", "Object to monitor", true);

    parse.AddArg(objectArg);

}