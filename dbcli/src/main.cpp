#include <Database.hh>
#include <CLI.hh>
#include <DOFRI.hh>
#include <allDBs.hh>

class CLI_ObjectArgs : public CLI::CLI_Argument<OBJECT, 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, OBJECT& value)
        {
            value = conversion;
            return true;
        }
};

int main(int argc, char* argv[])
{
    CLI::Parser parse("dbcli", "Modify DB object");
    CLI_ObjectArgs objArg("-o", "Object to update", true);
}