#include <schema.hh>


RETCODE ParseDatabaseEntry(std::istringstream & line)
{
    std::string databaseName;
    size_t databaseNumber;
    line >> databaseNumber >> databaseName;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }
    LOG_INFO("DATABASE NAME: %s DATABSE NUMBER: %u", databaseName.c_str(), databaseNumber);
    return RTN_OK;
}

RETCODE ParseObjectEntry(std::istringstream& line)
{
    size_t objectNumber = 0;
    std::string objectName;
    size_t numberOfRecords = 0;

    line >> objectNumber >> objectName >> numberOfRecords;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_INFO("OBJECT NUMBER: %u OBJECT NAME: %s NUMBER OF RECORDS: %u", objectNumber, objectName.c_str(), numberOfRecords);

    return RTN_OK;
}

RETCODE ParseFieldEntry(std::istringstream& line)
{
    size_t fieldNumber = 0;
    std::string fieldName;
    std::string fieldType;

    line >> fieldNumber >> fieldName >> fieldType;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_INFO("FIELD NUMBER: %u FIELD NAME: %s FIELD TYPE: %s", fieldNumber, fieldName.c_str(), fieldType.c_str());

    return RTN_OK;
}

inline bool isComment(char firstChar)
{
    return '#' == firstChar;
}

inline bool isEndOfObject(char firstChar)
{
   return  '0' == firstChar;
}

RETCODE LoadDatabase(const DATABASE& databaseName, std::ifstream& database)
{
    RETCODE retcode = RTN_OK;
    std::string line;
    size_t firstNonEmptyChar = 0;
    char firstChar = 0;
    bool foundDatabaseEntry = false;
    bool parsingObjectEntry = false;
    database.open(databaseName);

    if( !database.is_open() )
    {
        LOG_WARN("Could not open %s", databaseName.c_str());
        return RTN_NOT_FOUND;
    }

    while( std::getline(database, line) )
    {

        LOG_DEBUG("%s", line.c_str());
        firstNonEmptyChar = line.find_first_not_of(' ');
        firstChar = line.at(firstNonEmptyChar);

        if( isComment(firstChar) )
        {
            continue;
        }

        std::istringstream lineStream(line);

        if( !foundDatabaseEntry )
        {
            retcode |= ParseDatabaseEntry(lineStream);
            foundDatabaseEntry = true;
            continue;
        }

        if( isEndOfObject(firstChar) )
        {
            parsingObjectEntry = false;
            continue;
        }

        if( !parsingObjectEntry )
        {
            retcode |= ParseObjectEntry(lineStream);
            parsingObjectEntry = true;
            continue;
        }
        else
        {
            retcode |= ParseFieldEntry(lineStream);
            continue;
        }
    }

    if( database.bad() )
    {
        LOG_WARN("Error reading %s", databaseName.c_str());
        retcode |= RTN_FAIL;
    }

    database.close();

    return retcode;
}

RETCODE ReadDatabase(std::ifstream& database)
{
    return RTN_OK;
}

RETCODE VerifySchema(std::ifstream& database)
{
    return RTN_OK;
}

RETCODE GenerateHeaders(std::ifstream& database)
{
    return RTN_OK;
}

RETCODE GenerateDatabaseFile(std::ifstream& database)
{
    return RTN_OK;
}

RETCODE ModifyDatabase(std::ifstream& database)
{
    return RTN_OK;
}