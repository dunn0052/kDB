#include <schema.hh>
#include <fcntl.h>
#include <unistd.h>

RETCODE ParseDatabaseEntry(std::istringstream & line, DATABASE_SCHEMA& out_database)
{
    line >> out_database;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_DEBUG("DATABASE NAME: %s DATABSE NUMBER: %u", out_database.databaseName.c_str(), out_database.databaseNumber);

    return RTN_OK;
}

RETCODE ParseObjectEntry(std::istringstream& line, OBJECT_SCHEMA& out_object)
{
    line >> out_object;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_DEBUG("OBJECT NUMBER: %u OBJECT NAME: %s NUMBER OF RECORDS: %u", out_object.objectNumber, out_object.objectName.c_str(), out_object.numberOfRecords);

    return RTN_OK;
}

RETCODE ParseFieldEntry(std::istringstream& line, FIELD_SCHEMA& out_field)
{
    line >> out_field;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_DEBUG("FIELD NUMBER: %u FIELD NAME: %s FIELD TYPE: %c", out_field.fieldNumber, out_field.fieldName.c_str(), out_field.fieldType);

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

static RETCODE GenerateDatabaseHeader(DATABASE_SCHEMA& database, std::ofstream& headerFile)
{
    headerFile << "\n#define D_" << std::uppercase << database.databaseName << "_DB_NUM " << database.databaseNumber;
    headerFile << "\n#define D_" << std::uppercase << database.databaseName << "_NAME " << "\"" << database.databaseName << "\"\n";
    headerFile << "\n#define D_" << std::uppercase << database.databaseName << "_NUMBER_OBJECTS " << database.objects.size();
    headerFile << "\n\nsize_t D_" << std::uppercase << database.databaseName << "_OBJECT_SIZES[" <<
        "D_" << std::uppercase << database.databaseName << "_NUMBER_OBJECTS] =\n{";
    for(OBJECT_SCHEMA& object : database.objects)
    {
        headerFile << "\n    O_" << std::uppercase << object.objectName  << "_NUM_RECORDS,";
    }
    headerFile << "\n};";

    return RTN_OK;
}

static RETCODE GenerateObjectHeader(OBJECT_SCHEMA& object, std::ofstream& headerFile)
{
    headerFile << "\n\n#define O_" << std::uppercase << object.objectName << "_NUM_RECORDS " << object.numberOfRecords;
    headerFile << "\n\nstruct O_" << std::uppercase << object.objectName << "\n{";
    headerFile << "\n    RECORD record;";

    return RTN_OK;
}

static bool generateDataType(FIELD_SCHEMA& field, std::string& dataType)
{
    switch(field.fieldType)
    {
        case 'D': // Databse
        {
            dataType = "DATABASE";
            field.fieldSize = sizeof(DATABASE);
            break;
        }
        case 'O': // Object
        {
            dataType = "OBJECT";
            field.fieldSize = sizeof(OBJECT);
            break;
        }
        case 'F': // Field
        {
            dataType = "FIELD";
            field.fieldSize = sizeof(FIELD);
            break;
        }
        case 'R': // Record
        {
            dataType = "RECORD";
            field.fieldSize = sizeof(RECORD);
            break;
        }
        case 'I': // Index
        {
            dataType  = "INDEX";
            field.fieldSize = sizeof(INDEX);
            break;
        }
        case 'C': // Char
        {
            dataType = "char";
            field.fieldSize = sizeof(char);
            break;
        }
        case 'N': // signed integer
        {
            dataType = "int";
            field.fieldSize = sizeof(int);
            break;
        }
        case 'U': // Unsigned integer
        {
            dataType = "unsigned int";
            field.fieldSize = sizeof(unsigned int);
            break;
        }
        case 'B': // Bool
        {
            dataType = "bool";
            field.fieldSize = sizeof(bool);
            break;
        }
        case 'Y': // Unsigned char (byte)
        {
            dataType = "unsigned char";
            field.fieldSize = sizeof(unsigned char);
            break;
        }
        default:
        {
            return false;
        }
    }

    return true;
}

static RETCODE GenerateFieldHeader(FIELD_SCHEMA& field, std::ofstream& headerFile)
{
    std::string dataType;

    if( !generateDataType(field, dataType) )
    {
        return RTN_NOT_FOUND;
    }

    if(field.numElements > 1)
    {
            headerFile << "\n    " << dataType << " " << field.fieldName << "[" << field.numElements << "];";
    }
    else
    {
            headerFile << "\n    " << dataType << " " << field.fieldName << ";";
    }

    return RTN_OK;
}

RETCODE WriteObjectEnd( std::ofstream& headerFile )
{
    headerFile << "\n};";
    return RTN_OK;
}

// Probably doesn't work yet.. Need to account for adding includes
static RETCODE GenerateAllDBHeader(DATABASE_SCHEMA & database_entry)
{
    std::fstream allDBHeader("../../allDBs.hh");
    std::stringstream includeEntry;
    includeEntry << "#include <" << database_entry.databaseName << ".hh>\n";

    // + 3 = header guard + 1 new line
    size_t includeIndex = database_entry.databaseNumber + 3;
    allDBHeader.seekg(std::fstream::beg);
    for(size_t  findex=0; findex < includeIndex - 1; ++findex)
    {
        allDBHeader.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }

    allDBHeader << includeEntry.str();

    // add footer for header guard
    includeEntry.str("\n#endif");
    allDBHeader.seekg(std::fstream::end);
    allDBHeader << includeEntry.str();
    return RTN_OK;
}

static RETCODE  GenerateDatabaseFile(DATABASE_SCHEMA& database_entry, const std::string& database_name)
{
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << "./db/db/" <<  database_name << ".db";
    const std::string path = filepath.str();

    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if( 0 > fd )
    {
        LOG_WARN("Failed to open or create %s", path.c_str());
        retcode |=  RTN_NOT_FOUND;
    }

    if( ftruncate64(fd, database_entry.totalSize) )
    {
        LOG_WARN("Failed to truncate %s to size %u", path.c_str(), database_entry.totalSize);
        retcode |= RTN_MALLOC_FAIL;
    }

    LOG_DEBUG("Truncated %s to size %u", path.c_str(), database_entry.totalSize);

    if( close(fd) )
    {
        LOG_WARN("Failed to close %s", path.c_str());
        retcode |= RTN_FAIL;
    }

    return retcode;
}

/*
 * Database syntax:
 * Line by line
 * # lines starting with # are comments (ignored)
 * Must start with DATABSE NAME, DATABASE NUMBER
 * Objects must follow with OBJECT NUMBER, OBJECT NAME, NUMBER OF RECORDs
 * Within each object must be fields which are in the form of:
 * Record number, Record Name, Type of value
 * End an object with a 0
 *
 * This function creates a header and db file from a schema file.
 * Size calculation:
 * Calculate size of each field in object (size of type * number of elements)
 * Sum size of every field in object and multiply by number of object records
 * Sum the object record size totals and create database file of that size.
*/

RETCODE GenerateDatabase(const DATABASE& databaseName)
{
    RETCODE retcode = RTN_OK;
    std::string line;
    size_t firstNonEmptyChar = 0;
    char firstChar = 0;
    bool foundDatabaseEntry = false;
    bool parsingObjectEntry = false;
    size_t currentLineNum = 0;

    std::string skmPath("./db/skm/");
    std::string incPath("./db/inc/");
    std::ifstream schemaFile;
    std::ofstream headerFile;

    schemaFile.open(skmPath + databaseName +".skm");
    headerFile.open(incPath + databaseName + ".hh", std::ofstream::trunc | std::ofstream::out );

    DATABASE_SCHEMA database_entry;

    if( !schemaFile.is_open() )
    {
        LOG_WARN("Could not open %s%s.skm", skmPath.c_str(), databaseName.c_str());
        return RTN_NOT_FOUND;
    }

    if( !headerFile.is_open() )
    {
        LOG_WARN("Could not open %s%s.hh", incPath.c_str(),  databaseName.c_str());
        return RTN_NOT_FOUND;
    }

    headerFile << "#ifndef " << std::uppercase << databaseName << "__HH";
    headerFile << "\n#define " << std::uppercase << databaseName << "__HH";
    headerFile << "\n\n#include \"../../common_inc/DOFRI.hh\"\n";

    while( std::getline(schemaFile, line) )
    {
        currentLineNum++;
        LOG_INFO("%s", line.c_str());
        firstNonEmptyChar = line.find_first_not_of(' ');
        firstChar = line.at(firstNonEmptyChar);

        if( isComment(firstChar) )
        {
            continue;
        }

        std::istringstream lineStream(line);

        if( !foundDatabaseEntry )
        {
            database_entry.totalSize = 0;
            retcode |= ParseDatabaseEntry(lineStream, database_entry);
            if(RTN_OK != retcode)
            {
                LOG_WARN("Error reading database entry: %s%s.skm:%u", skmPath.c_str(), databaseName.c_str(), currentLineNum);
                return retcode;
            }

            foundDatabaseEntry = true;
            continue;
        }

        if( isEndOfObject(firstChar) )
        {
            WriteObjectEnd(headerFile);
            database_entry.totalSize += ( database_entry.objects.back().objectSize * database_entry.objects.back().numberOfRecords );
            parsingObjectEntry = false;
            continue;
        }

        if( !parsingObjectEntry )
        {
            OBJECT_SCHEMA object_entry;
            object_entry.objectSize = 0;

            database_entry.objects.push_back(object_entry);
            retcode |= ParseObjectEntry(lineStream, database_entry.objects.back());
            if( RTN_OK != retcode )
            {
                LOG_WARN("Error reading object entry: %s%s.skm:%d", skmPath.c_str(), databaseName.c_str(), currentLineNum);
                return retcode;
            }

            retcode |= GenerateObjectHeader(database_entry.objects.back(), headerFile);
            parsingObjectEntry = true;
            continue;
        }
        else
        {
            FIELD_SCHEMA field_entry;
            database_entry.objects.back().fields.push_back(field_entry);
            retcode |= ParseFieldEntry(lineStream, database_entry.objects.back().fields.back());

            if( RTN_OK != retcode )
            {
                LOG_WARN("Error reading field entry: %s%s.skm:%d", skmPath.c_str(), databaseName.c_str(), currentLineNum);
                return retcode;
            }
            retcode |= GenerateFieldHeader(database_entry.objects.back().fields.back(), headerFile);
            database_entry.objects.back().objectSize += database_entry.objects.back().fields.back().fieldSize;
            continue;
        }
    }

    retcode |= GenerateDatabaseHeader(database_entry, headerFile);

    headerFile << "\n\n#endif";

    if( schemaFile.bad() )
    {
        LOG_WARN("Error reading %s%s.skm", skmPath.c_str(), databaseName.c_str());
        retcode |= RTN_FAIL;
    }

    if( headerFile.bad() )
    {
        LOG_WARN("Error writing %s%s.hh", incPath.c_str(), databaseName.c_str());
        retcode |= RTN_FAIL;
    }

    schemaFile.close();
    headerFile.close();

    retcode |= GenerateDatabaseFile(database_entry, databaseName);

    return retcode;
}