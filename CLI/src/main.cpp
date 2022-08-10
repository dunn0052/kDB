#include <CLI.hh>
#include <iostream>
#include <unistd.h>


int main(int argc, char* argv[])
{
    CLI::Parser parse("CLI Handler", "This is a description of this CLI!");
    CLI::CLI_FlagArgument test_arg("-d", "Debug option");
    CLI::CLI_IntArgument test_int("test", "Integer argument");

    test_arg.Required();

    parse.AddArg(test_arg);
    parse.AddArg(test_int);
    parse.ParseCommandLineArguments(argc, argv);

    if(test_arg.IsInUse())
    {
        std::cout << "Using -d option!" << std::endl;
    }

    if(test_int.IsInUse())
    {
        std::cout << "Using the --test option!" << std::endl;
    }
}