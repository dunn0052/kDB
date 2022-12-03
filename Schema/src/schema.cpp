#include <schema.hh>
#include <fcntl.h>
#include <unistd.h>
#include <compiler_defines.hh>
#include <Constants.hh>
#include <dirent.h>

/* Object info */
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
/* Field info */
RETCODE ParseFieldEntry(std::istringstream& line, FIELD_SCHEMA& out_field)
{
    line >> out_field;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_DEBUG("FIELD NUMBER: %u FIELD NAME: %s FIELD TYPE: %c NUMBER OF ELEMENTS: %u", out_field.fieldNumber, out_field.fieldName.c_str(), out_field.fieldType, out_field.numElements);

    return RTN_OK;
}

inline bool isComment(char firstChar)
{
    return '#' == firstChar;
}

/* Sentinal value for end of object definition is 0 */
inline bool isEndOfObject(char firstChar)
{
   return  '0' == firstChar;
}

static RETCODE GenerateObjectHeader(OBJECT_SCHEMA& object, std::ofstream& headerFile)
{
    /* Header guard */
    headerFile << "#ifndef " << std::uppercase << object.objectName << "__HH";
    headerFile << "\n#define " << std::uppercase << object.objectName << "__HH";
    headerFile << "\n\n#include <DOFRI.hh>\n"; // maybe..

    headerFile << "\n\nstatic const size_t O_" << std::uppercase << object.objectName << "_NUM_RECORDS = " << object.numberOfRecords << ";";
    headerFile << "\n\n#pragma pack (4)";
    headerFile << "\nstruct " << std::uppercase << object.objectName << "\n{";

    if( headerFile.bad() )
    {
        return RTN_FAIL;
    }

    return RTN_OK;
}

static bool TryGenerateDataType(FIELD_SCHEMA& field, std::string& dataType)
{
    // Sizings are not accurate because of struct padding
    switch(field.fieldType)
    {
        case 'D': // Databse innacurate because its a string alias
        {
            dataType = "DATABASE";
            break;
        }
        case 'O': // Object
        {
            dataType = "OBJECT";
            break;
        }
        case 'F': // Field
        {
            dataType = "FIELD";
            break;
        }
        case 'R': // Record
        {
            dataType = "RECORD";
            break;
        }
        case 'I': // Index
        {
            dataType  = "INDEX";
            break;
        }
        case 'C': // Char
        {
            dataType = "char";
            break;
        }
        case 'N': // signed integer
        {
            dataType = "int";
            break;
        }
        case 'U': // Unsigned integer
        {
            dataType = "unsigned int";
            break;
        }
        case 'B': // Bool
        {
            dataType = "bool";
            break;
        }
        case 'Y': // Unsigned char (byte)
        {
            dataType = "unsigned char";
            break;
        }
        default:
        {
            return false;
        }
    }

    return true;
}

static bool TrySetFieldSize(FIELD_SCHEMA& field)
{
    // Sizings are not accurate because of struct padding
    switch(field.fieldType)
    {
        case 'D': // Databse innacurate because its a string alias
        {
            field.fieldSize = sizeof(DATABASE);
            break;
        }
        case 'O': // Object
        {
            field.fieldSize = sizeof(OBJECT);
            break;
        }
        case 'F': // Field
        {
            field.fieldSize = sizeof(FIELD);
            break;
        }
        case 'R': // Record
        {
            field.fieldSize = sizeof(RECORD);
            break;
        }
        case 'I': // Index
        {
            field.fieldSize = sizeof(INDEX);
            break;
        }
        case 'C': // Char
        {
            field.fieldSize = sizeof(char);
            break;
        }
        case 'N': // signed integer
        {
            field.fieldSize = sizeof(int);
            break;
        }
        case 'U': // Unsigned integer
        {
            field.fieldSize = sizeof(unsigned int);
            break;
        }
        case 'B': // Bool
        {
            field.fieldSize = sizeof(bool);
            break;
        }
        case 'Y': // Unsigned char (byte)
        {
            field.fieldSize = sizeof(unsigned char);
            break;
        }
        default:
        {
            return false;
        }
    }

    field.fieldSize *= field.numElements;

    return true;
}

static RETCODE GenerateFieldHeader(FIELD_SCHEMA& field, std::ofstream& headerFile)
{
    std::string dataType;

    if( !TryGenerateDataType(field, dataType) )
    {
        return RTN_NOT_FOUND;
    }

    if(field.numElements > 1)
    {
        /* Array of elements */
        headerFile
            << "\n    "
            << dataType
            << " "
            << field.fieldName
            << "["
            << field.numElements
            << "];";
    }
    else
    {
        headerFile
            << "\n    "
            << dataType
            << " "
            << field.fieldName
            << ";";
    }

    if( headerFile.bad() )
    {
        return RTN_FAIL;
    }

    return RTN_OK;
}

RETCODE WriteObjectEnd( std::ofstream& headerFile )
{
    headerFile << "\n};\n\n#endif";
    if( headerFile.bad() )
    {
        return RTN_FAIL;
    }

    return RTN_OK;
}

static RETCODE GenerateDatabaseFile(OBJECT_SCHEMA& object_entry, const std::string& object_name, const std::string& dbPath)
{
    RETCODE retcode = RTN_OK;
    std::stringstream filepath;
    filepath << dbPath <<  object_entry.objectName << DB_EXT;
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
    std::stringstream allDBHeaderPath;
    allDBHeaderPath << INSTALL_DIR << COMMON_INC_PATH << ALL_DB_HEADER_NAME << HEADER_EXT;
    headerStream.open(allDBHeaderPath.str());

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
    std::stringstream allDBHeaderPath;
    allDBHeaderPath << INSTALL_DIR << COMMON_INC_PATH << ALL_DB_HEADER_NAME << HEADER_EXT;
    std::ofstream headerStream;
    headerStream.open(allDBHeaderPath.str());

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
    std::stringstream allDBHeaderPath;
    allDBHeaderPath << INSTALL_DIR << COMMON_INC_PATH << ALL_DB_HEADER_NAME << HEADER_EXT;
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not open up %sfor reading!", allDBHeaderPath.str().c_str());
        return retcode;
    }

    retcode = writeAllDBHeader(out_lines);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not open up %s for writing", allDBHeaderPath.str().c_str());
        return retcode;
    }

    return retcode;
}

static RETCODE GenerateObject(const OBJECT& objectName, const std::string& skmPath, OBJECT_SCHEMA& out_object_entry)
{
    RETCODE retcode = RTN_OK;
    std::string line;
    size_t firstNonEmptyChar = 0;
    char firstChar = 0;
    bool parsingObjectEntry = false;
    size_t currentLineNum = 0;

    std::ifstream schemaFile;
    std::stringstream schema_path;

    schema_path << skmPath << objectName << SKM_EXT;


    schemaFile.open(schema_path.str());

    out_object_entry.objectSize = 0;

    if( !schemaFile.is_open() )
    {
        LOG_WARN("Could not open %s", schema_path.str().c_str());
        return RTN_NOT_FOUND;
    }



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
            parsingObjectEntry = false;
            continue;
        }

        /* Start of object generation */
        if( !parsingObjectEntry )
        {
            retcode |= ParseObjectEntry(lineStream, out_object_entry);
            if( RTN_OK != retcode )
            {
                LOG_WARN("Error reading object entry: %s%s.skm:%d", skmPath.c_str(), objectName, currentLineNum);
                return retcode;
            }

            parsingObjectEntry = true;
            continue;
        }
        else
        {
            FIELD_SCHEMA field_entry;
            retcode |= ParseFieldEntry(lineStream, field_entry);

            if( RTN_OK != retcode )
            {
                LOG_WARN("Error reading field entry: %s%s.skm:%d", skmPath.c_str(), objectName, currentLineNum);
                return retcode;
            }

            if(!TrySetFieldSize(field_entry))
            {
                LOG_WARN("Invalid field entry: %s%s.skm:%d", skmPath.c_str(), objectName, currentLineNum);
                retcode |= RTN_NOT_FOUND;
                return retcode;
            }

            out_object_entry.objectSize += field_entry.fieldSize;
            out_object_entry.fields.push_back(field_entry);
            continue;
        }
    }

    if( schemaFile.bad() )
    {
        LOG_WARN("Error reading %s%s.skm", skmPath.c_str(), objectName);
        retcode |= RTN_FAIL;
    }

    schemaFile.close();

    return retcode;
}

static RETCODE GenerateHeaderFile(OBJECT_SCHEMA& object_entry, const std::string& header_file_path)
{
    std::ofstream headerFile;
    RETCODE retcode = RTN_OK;

    headerFile.open(header_file_path);
    if( !headerFile.is_open() )
    {
        LOG_WARN("Could not open %s", header_file_path.c_str());
        return RTN_NOT_FOUND;
    }

    retcode |= GenerateObjectHeader(object_entry, headerFile);
    for(FIELD_SCHEMA& field : object_entry.fields)
    {
        retcode |= GenerateFieldHeader(field, headerFile);
    }

    retcode |= WriteObjectEnd(headerFile);

    headerFile.close();

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

RETCODE GenerateObjectDBFiles(const OBJECT& objectName, const std::string& skmPath, const std::string& incPath)
{
    OBJECT_SCHEMA object_entry;
    RETCODE retcode = GenerateObject(objectName, skmPath, object_entry);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error generating %s", object_entry.objectName.c_str());
        return retcode;
    }

    std::stringstream header_path;
    header_path << incPath << objectName << HEADER_EXT;
    retcode |= GenerateHeaderFile(object_entry, header_path.str());
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error generating %s.hh", object_entry.objectName.c_str());
        return retcode;
    }
    LOG_INFO("Generated %s.hh", object_entry.objectName.c_str());

#if 0
    retcode |= GenerateDatabaseFile(object_entry, objectName, dbPath);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error generating %s.db", object_entry.objectName.c_str());
        return retcode;
    }
    LOG_INFO("Generated %s.db", object_entry.objectName.c_str());
#endif

    retcode |= AddToAllDBHeader(object_entry);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error adding %s to allHeader.hh", object_entry.objectName.c_str());
        return retcode;
    }
    LOG_INFO("Added to %s to allHeader.hh", object_entry.objectName.c_str());

    return retcode;
}

RETCODE GetSchemaFileObjectName(const std::string& skmFileName, std::string& out_ObjectName)
{
    if("." == skmFileName || ".." == skmFileName)
    {
        out_ObjectName = skmFileName;
        // current and up directory can be skipped
        return RTN_OK;
    }

    size_t ext_index = skmFileName.rfind('.', skmFileName.length());
    if (ext_index != std::string::npos)
    {
        std::string ext_name =
            skmFileName.substr(ext_index, skmFileName.length() - ext_index);

        if(SKM_EXT == ext_name)
        {
            out_ObjectName = skmFileName.substr(0, ext_index);
            return RTN_OK;
        }


    }

    return RTN_NOT_FOUND;
}

RETCODE GenerateAllDBFiles(const std::string& skmPath, const std::string& incPath)
{
    LOG_DEBUG("Schema path: %s", skmPath.c_str());
    LOG_DEBUG("Include path: %s", incPath.c_str());
    RETCODE retcode = RTN_OK;
    std::vector<std::string> schema_files;
    DIR *directory;
    struct dirent *fileName;
    if ((directory = opendir (skmPath.c_str())) != NULL)
    {
        /* print all the files and directories within directory */
        while ((fileName = readdir (directory)) != NULL)
        {
            std::string objectName;
            std::string foundFile = std::string(fileName->d_name);
            retcode |= GetSchemaFileObjectName(
                foundFile, objectName);

            if("." == foundFile || ".." == foundFile)
            {
                // current and up directory can be skipped
                continue;
            }

            if(RTN_OK == retcode)
            {
                LOG_DEBUG("Found schema file: %s", foundFile.c_str());
                schema_files.push_back(objectName);
            }
            else
            {
                LOG_WARN("File: %s does not have a %s extension",
                    foundFile.c_str(),  SKM_EXT.c_str());
            }
        }

        closedir (directory);

        for(std::string& schema : schema_files)
        {
            OBJECT objName = {0};
            strncpy(objName, schema.c_str(), sizeof(objName));
            retcode |= GenerateObjectDBFiles(objName, skmPath, incPath);
        }

        return retcode;
    }
    else
    {
        /* could not open directory */
        return RTN_NOT_FOUND;
    }
}