#include <SystemProfiler.hh>
#include <CLI.hh>

class CLI_StringArguments : public CLI::CLI_Argument<std::string, 1, 10>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, std::string& value)
        {
            value = conversion;
            m_InUse = true;
            return true;
        }
};

int main(int argc, char* argv[])
{
    CLI::Parser parse("SystemProfiler", "Profile multi-process systems");

    CLI::CLI_StringArgument SystemName("-s", "Name of the system", true);

    CLI_StringArguments SystemProcesses("--processes", "Name of processes", true);

    parse
        .AddArg(SystemName)
        .AddArg(SystemProcesses);

    RETCODE parseRetcode = parse.ParseCommandLineArguments(argc, argv);
}