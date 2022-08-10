#ifndef SCHEMA__HH
#define SCHEMA__HH

#include <CLI.hh>
#include <DOFRI.hh>
#include <retcode.hh>

#include <fstream>
#include <sstream>

class CLI_DatabaseArgs : public CLI::CLI_Argument<DATABASE, 1, 1>
{
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, DATABASE& value)
        {
            value = conversion;
            return true;
        }
};

RETCODE LoadDatabase(const DATABASE& databaseName, std::ifstream& database);
RETCODE ReadDatabase(std::ifstream& database);
RETCODE VerifySchema(std::ifstream& database);
RETCODE GenerateHeaders(std::ifstream& database);
RETCODE GenerateDatabaseFile(std::ifstream& database);

RETCODE ModifyDatabase(std::ifstream& database);

#endif