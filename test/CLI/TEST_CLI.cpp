#include <CLI.hh>

#include <gtest/gtest.h>

static const std::string CLI_NAME = "TEST";
static const std::string CLI_DESCRIPTION = "Description of CLI";

// Demonstrate some basic assertions.
TEST(CLI_Test, ParseArgument_Test)
{

    // Setup
    const int NUM_ARGS = 4;
    int argc = NUM_ARGS;

    const char* PROG_NAME = "ParseArgument_Test";
    const char* STRING_ARG = "-s";
    const char* STRING_VAL = "string value";
    const char* INT_ARG = "-i";
    const char* INT_VAL = "1";

    const char* argv[NUM_ARGS + 1] = {PROG_NAME, STRING_ARG, STRING_VAL, INT_ARG, INT_VAL};

    const char* STRING_ARG_DESC = "string argument description";
    const char* INT_ARG_DESC = "int argument description";

    // Test
    CLI::Parser parse(CLI_NAME, CLI_DESCRIPTION);
    CLI::CLI_StringArgument string_argument(STRING_ARG, STRING_ARG_DESC);
    CLI::CLI_IntArgument int_argument(INT_ARG, INT_ARG_DESC);

    parse
        .AddArg(string_argument)
        .AddArg(int_argument);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);

    // Assert

    ASSERT_EQ(RTN_OK, retcode);

    ASSERT_TRUE(string_argument.IsInUse());

    ASSERT_TRUE(int_argument.IsInUse());

}