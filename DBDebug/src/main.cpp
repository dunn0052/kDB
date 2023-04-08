#include <DBMap.hh>
#include <ObjectReader.hh>
#include <Logger.hh>
#include <OFRI.hh>
#include <CLI.hh>
#include <Constants.hh>

static RETCODE PrintObjectInfo(const OBJECT&);

int main(int argc, char* argv[])
{
    CLI::Parser parser("Debug kDB database and schema");
    CLI::CLI_OBJECTArgument objArg("-o", "Name of object");
    CLI::CLI_FlagArgument allArg("-a", "Get info on all registered objects");
    RETCODE retcode = RTN_OK;


    parser
        .AddArg(objArg)
        .AddArg(allArg);

    retcode = parser.ParseCommandLineArguments(argc, argv);

    if(objArg.IsInUse())
    {
        LOG_INFO("-- DBDebug object report --");
        retcode = PrintObjectInfo(objArg.GetValue());
        if(RTN_NOT_FOUND == retcode)
        {
            LOG_ERROR("Could not find object: ", objArg.GetValue());
        }
        LOG_INFO("-- DBDebug end report --\n");

    }
    else if(allArg.IsInUse())
    {
        LOG_INFO("-- DBDebug all report --\n");
        std::map<std::string, OBJECT_SCHEMA>::iterator it;
        OBJECT currentObject;
        for (it = dbSizes.begin(); it != dbSizes.end(); it++)
        {
            strncpy(currentObject, it->first.c_str(), sizeof(currentObject));
            retcode = PrintObjectInfo(currentObject);
            if(RTN_NOT_FOUND == retcode)
            {
                LOG_ERROR("Could not find object: ", currentObject);
            }
            std::cout << "\n"; // space for next object
        }

        LOG_INFO("-- DBDebug end report --\n");
    }
    else
    {
        retcode = RTN_BAD_ARG;
        parser.Usage();
    }

    return retcode;
}


static RETCODE PrintObjectInfo(const OBJECT& obj)
{

    RETCODE retcode = RTN_OK;
    OBJECT_SCHEMA objSchema;
    RETURN_RETCODE_IF_NOT_OK(TryGetObjectInfo(obj, objSchema));

    LOG_INFO("Members of ", objSchema.objectName);
    bool errorPrinted = false;
    size_t fieldSum = 0;
    size_t paddingSum = 0;
    for(const FIELD_SCHEMA& field : objSchema.fields)
    {
            LOG_INFO(field.fieldName,
                " field size: ",
                field.fieldSize,
                " field offset: ",
                field.fieldOffset);
            if(field.fieldOffset != fieldSum)
            {
                size_t addedPadding = field.fieldOffset - fieldSum;
                LOG_WARN("Padding of ", addedPadding, " was added by the compiler to the end of field: ", field.fieldName);
                fieldSum += addedPadding;
                paddingSum += addedPadding;
                retcode |= RTN_BAD_ARG;
            }

            fieldSum += field.fieldSize;
    }

    LOG_INFO(obj, " sum of record sizes: ", (fieldSum - paddingSum)); // Needed since it's added during parsing
    LOG_INFO(obj, " actual struct size: ", objSchema.objectSize);

    if(paddingSum)
    {
        LOG_ERROR("Padding or rearrangement of ", paddingSum, " bytes in ", objSchema.objectName, SKM_EXT, " are needed!");
    }

    return retcode;

}