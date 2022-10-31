#include <schema.hh>
#include <fcntl.h>
#include <unistd.h>
#include <compiler_defines.hh>

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

static RETCODE GenerateObjectHeader(OBJECT_SCHEMA& object, std::ofstream& headerFile)
{
    headerFile << "\n\n#define O_" << std::uppercase << object.objectName << "_NUM_RECORDS " << object.numberOfRecords;
    headerFile << "\n\nstruct " << std::uppercase << object.objectName << "\n{";

    return RTN_OK;
}

static bool generateDataType(FIELD_SCHEMA& field, std::string& dataType)
{
    // Sizings are not accurate because of struct padding
    switch(field.fieldType)
    {
        case 'D': // Databse innacurate because its a string alias
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
            field.fieldSize *= field.numElements;
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

static RETCODE  GenerateDatabaseFile(OBJECT_SCHEMA& object_entry, const std::string& object_name)
{
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << "./db/db/" <<  object_entry.objectName << ".db";
    const std::string path = filepath.str();
    size_t fileSize = object_entry.objectSize * object_entry.numberOfRecords;

    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if( 0 > fd )
    {
        LOG_WARN("Failed to open or create %s", path.c_str());
        retcode |=  RTN_NOT_FOUND;
    }

    if( ftruncate64(fd, fileSize) )
    {
        LOG_WARN("Failed to truncate %s to size %u", path.c_str(), fileSize);
        retcode |= RTN_MALLOC_FAIL;
    }

    if( close(fd) )
    {
        LOG_WARN("Failed to close %s", path.c_str());
        retcode |= RTN_FAIL;
    }

    return retcode;
}

static RETCODE readAllDBHeader(OBJECT_SCHEMA& object_entry, std::vector<std::string>& out_lines)
{
    std::string line;
    bool entry_replaced = false;
    size_t lineNum = 1;
    std::ifstream headerStream;
    headerStream.open("./common_inc/allDBs.hh");

    if( !headerStream.is_open() )
    {
        return RTN_NOT_FOUND;
    }

    // Update entry
    while( std::getline(headerStream, line) )
    {
        if( object_entry.objectNumber == lineNum )
        {
            line = "#include <" + object_entry.objectName + ".hh>";
            entry_replaced = true;
        }
        out_lines.push_back(line);
        lineNum++;
    }

    // New entry that extends the number of lines in the header
    if( !entry_replaced )
    {
        for(; lineNum != object_entry.objectNumber; ++lineNum)
        {
            out_lines.push_back("");
        }

        out_lines.push_back("#include <" +  object_entry.objectName + ".hh>");
    }

    headerStream.close();

    return RTN_OK;
}

static RETCODE writeAllDBHeader(std::vector<std::string>& out_lines)
{
    std::ofstream headerStream;
    headerStream.open("./common_inc/allDBs.hh");

    if( !headerStream.is_open() )
    {
        return RTN_NOT_FOUND;
    }

    for( std::string& line : out_lines)
    {
        headerStream << line << "\n";
    }

    headerStream.close();

    return RTN_OK;
}


static RETCODE AddToAllDBHeader(OBJECT_SCHEMA& object_entry)
{
    std::vector<std::string> out_lines;
    RETCODE retcode = RTN_OK;

    retcode = readAllDBHeader(object_entry, out_lines);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not open up allDBs.hh for reading!");
        return retcode;
    }

    retcode = writeAllDBHeader(out_lines);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not open up allDBs.hh for writing")
        return retcode;
    }

    return retcode;
}

/*
 * Database syntax:
 * Line by line
 * # lines starting with # are comments (ignored)
 * Objects must follow with OBJECT NUMBER, OBJECT NAME, NUMBER OF RECORDs
 * Within each object must be fields which are in the form of:
 * Record number, Record Name, Type of value
 * End an object with a 0
 *
 * This function creates a header and db file from a schema file.
 * Size calculation:
 * Calculate size of each field in object (size of type * number of elements)
 * Sum size of every field in object and multiply by number of object records
 *
*/

RETCODE GenerateObjectDBFiles(const OBJECT& objectName)
{
    RETCODE retcode = RTN_OK;
    std::string line;
    size_t firstNonEmptyChar = 0;
    char firstChar = 0;
    bool parsingObjectEntry = false;
    size_t currentLineNum = 0;

    std::string skmPath("./db/skm/");
    std::string incPath("./db/inc/");
    std::ifstream schemaFile;
    std::ofstream headerFile;

    schemaFile.open(skmPath + objectName +".skm");
    headerFile.open(incPath + objectName + ".hh", std::ofstream::trunc | std::ofstream::out );

    OBJECT_SCHEMA object_entry;
    object_entry.objectSize = 0;

    if( !schemaFile.is_open() )
    {
        LOG_WARN("Could not open %s%s.skm", skmPath.c_str(), objectName.c_str());
        return RTN_NOT_FOUND;
    }

    if( !headerFile.is_open() )
    {
        LOG_WARN("Could not open %s%s.hh", incPath.c_str(),  objectName.c_str());
        return RTN_NOT_FOUND;
    }

    headerFile << "#ifndef " << std::uppercase << objectName << "__HH";
    headerFile << "\n#define " << std::uppercase << objectName << "__HH";
    headerFile << "\n\n#include \"../../common_inc/DOFRI.hh\"\n";

    while( std::getline(schemaFile, line) )
    {
        currentLineNum++;
        firstNonEmptyChar = line.find_first_not_of(' ');
        firstChar = line.at(firstNonEmptyChar);

        if( isComment(firstChar) )
        {
            continue;
        }

        std::istringstream lineStream(line);

        if( isEndOfObject(firstChar) )
        {
            WriteObjectEnd(headerFile);
            parsingObjectEntry = false;
            continue;
        }

        if( !parsingObjectEntry )
        {
            retcode |= ParseObjectEntry(lineStream, object_entry);
            if( RTN_OK != retcode )
            {
                LOG_WARN("Error reading object entry: %s%s.skm:%d", skmPath.c_str(), objectName.c_str(), currentLineNum);
                return retcode;
            }

            retcode |= GenerateObjectHeader(object_entry, headerFile);
            parsingObjectEntry = true;
            continue;
        }
        else
        {
            FIELD_SCHEMA field_entry;
            retcode |= ParseFieldEntry(lineStream, field_entry);

            if( RTN_OK != retcode )
            {
                LOG_WARN("Error reading field entry: %s%s.skm:%d", skmPath.c_str(), objectName.c_str(), currentLineNum);
                return retcode;
            }
            retcode |= GenerateFieldHeader(field_entry, headerFile);
            object_entry.objectSize += field_entry.fieldSize;
            continue;
        }
    }

    headerFile << "\n\n#endif";

    if( schemaFile.bad() )
    {
        LOG_WARN("Error reading %s%s.skm", skmPath.c_str(), objectName.c_str());
        retcode |= RTN_FAIL;
    }

    if( headerFile.bad() )
    {
        LOG_WARN("Error writing %s%s.hh", incPath.c_str(), objectName.c_str());
        retcode |= RTN_FAIL;
    }

    schemaFile.close();
    headerFile.close();

    LOG_INFO("Genrated %s.hh", object_entry.objectName.c_str());

    retcode |= AddToAllDBHeader(object_entry);

    //retcode |= GenerateDatabaseFile(object_entry, objectName);
    if( RTN_OK == retcode )
    {
            LOG_INFO("Generated %s.db", object_entry.objectName.c_str());
    }

    return retcode;
}