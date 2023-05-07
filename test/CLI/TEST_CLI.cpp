#include <CLI.hh>

#include <gtest/gtest.h>

static const std::string CLI_NAME = "TEST";
static const std::string CLI_DESCRIPTION = "Description of CLI";
static const std::string PROG_NAME = "ParseArgument_Test";

static const std::string STRING_ARG = "-s";
static const std::string STRING_VAL = "string value";
static const std::string STRING_ARG_DESC = "string argument description";

static const std::string INT_ARG = "-i";
static const std::string INT_VAL = "1";
static const std::string INT_ARG_DESC = "int argument description";

static const std::string FLAG_ARG = "-f";
static const std::string FLAG_ARG_DESC = "flag argument description";

static const std::string OBJ_ARG = "-o";
static const std::string OBJ_ARG_DESC = "object argument description";
static const std::string OBJ_VAL = "OBJECT";

static const std::string OFRI_ARG = "-r";
static const std::string OFRI_ARG_DESC = "OFRI argument description";
static const std::string OFRI_VAL = "OBJECT.2.3.4";

static const std::string OR_ARG = "-O";
static const std::string OR_ARG_DESC = "OR argument description";
static const std::string OR_VAL = "OBJECT.1";

// Demonstrate some basic assertions.
TEST(CLI_Test, ParseArgument_Test)
{

    // Setup
    const int NUM_ARGS = 11;
    int argc = NUM_ARGS;

    const char* argv[NUM_ARGS] = {
            STRING_ARG.c_str(),STRING_VAL.c_str(),
            INT_ARG.c_str(), INT_VAL.c_str(),
            FLAG_ARG.c_str(),
            OBJ_ARG.c_str(), OBJ_VAL.c_str(),
            OFRI_ARG.c_str(), OFRI_VAL.c_str(),
            OR_ARG.c_str(), OR_VAL.c_str()
            };

    // Test
    CLI::Parser parse(CLI_NAME, CLI_DESCRIPTION);
    CLI::CLI_StringArgument string_argument(STRING_ARG, STRING_ARG_DESC);
    CLI::CLI_IntArgument int_argument(INT_ARG, INT_ARG_DESC);
    CLI::CLI_FlagArgument flag_argument(FLAG_ARG, FLAG_ARG_DESC);
    CLI::CLI_OBJECTArgument object_argument(OBJ_ARG, OBJ_ARG_DESC);
    CLI::CLI_OFRIArgument ofri_argument(OFRI_ARG, OFRI_ARG_DESC);
    CLI::CLI_ORArgument or_argument(OR_ARG, OR_ARG_DESC);

    parse
        .AddArg(string_argument)
        .AddArg(int_argument)
        .AddArg(flag_argument)
        .AddArg(object_argument)
        .AddArg(ofri_argument)
        .AddArg(or_argument);

    RETCODE retcode = parse.ParseCommandLineArguments(argc, argv);

    // Assert

    ASSERT_EQ(RTN_OK, retcode);

    ASSERT_TRUE(string_argument.IsInUse());

    ASSERT_TRUE(int_argument.IsInUse());

    ASSERT_TRUE(flag_argument.IsInUse());

    ASSERT_TRUE(object_argument.IsInUse());

    ASSERT_TRUE(ofri_argument.IsInUse());

    ASSERT_TRUE(or_argument.IsInUse());

}