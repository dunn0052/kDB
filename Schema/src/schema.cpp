#include <schema.hh>
#include <fcntl.h>
#include <unistd.h>
#include <compiler_defines.hh>
#include <Constants.hh>
#include <dirent.h>
#include <bits/stdc++.h>

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
    headerFile << "// GENERATED: DO NOT MODIFY!!\n";
    /* Header guard */
    headerFile << "#ifndef " << std::uppercase << object.objectName << "__HH";
    headerFile << "\n#define " << std::uppercase << object.objectName << "__HH";
    headerFile << "\n\n#include <DOFRI.hh>\n#include<ObjectSchema.hh>\n"; // maybe..

    headerFile
        << "\nstruct "
        << std::uppercase
        << object.objectName
        << "\n{";

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

RETCODE GenerateObjectInfo(std::ofstream& headerFile, OBJECT_SCHEMA& object)
{
    headerFile << "\nstatic const OBJECT_SCHEMA O_" << std::uppercase << object.objectName << "_INFO =\n"
        << "    {\n"
        << "        .objectNumber = " << object.objectNumber << ",\n"
        << "        .objectName = \"" << std::uppercase << object.objectName << "\",\n"
        << "        .numberOfRecords = " << object.numberOfRecords << ",\n"
        << "        .fields =\n        {";

    for(FIELD_SCHEMA& field : object.fields)
    {
        headerFile
            << "\n            {\n"
            << "                .fieldNumber = " << field.fieldNumber << ",\n"
            << "                .fieldName = \"" << field.fieldName << "\",\n"
            << "                .fieldType = \'" << field.fieldType << "\',\n"
            << "                .numElements = " << field.numElements << ",\n"
            << "                .fieldSize = " << field.fieldSize << ",\n"
            << "                .fieldOffset = offsetof(" << std::uppercase << object.objectName <<"," << field.fieldName << ")\n"
            << "            },";
    }

    headerFile
        << "\n        },\n"
        << "        .objectSize = sizeof("
        << std::uppercase << object.objectName
        << ")"
        << "\n    };";

    return RTN_OK;
}

RETCODE WriteObjectEnd( std::ofstream& headerFile, OBJECT_SCHEMA& object )
{
    headerFile << "\n};";
    std::stringstream upperCaseSStream;
    upperCaseSStream << std::uppercase << std::string(object.objectName);
    const std::string& objName = upperCaseSStream.str();

    /* Object name */
    headerFile
        << "\nstatic const OBJECT O_"
        << objName
        << "_NAME = \""
        << objName
        << "\";";

    GenerateObjectInfo(headerFile, object);

    headerFile << "\n\n#endif";

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
    allDBHeaderPath
        << INSTALL_DIR
        << COMMON_INC_PATH
        << ALL_DB_HEADER_NAME
        << HEADER_EXT;

    headerStream.open(allDBHeaderPath.str(),
        std::fstream::in | std::fstream::out | std::fstream::app);

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up %s for reading!",
            allDBHeaderPath.str().c_str());
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
    allDBHeaderPath
        << INSTALL_DIR
        << COMMON_INC_PATH
        << ALL_DB_HEADER_NAME
        << HEADER_EXT;

    std::ofstream headerStream;
    headerStream.open(allDBHeaderPath.str());

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up %s for writing", allDBHeaderPath.str().c_str());
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

    RETURN_RETCODE_IF_NOT_OK(readAllDBHeader(object_entry, out_lines));

    RETURN_RETCODE_IF_NOT_OK(writeAllDBHeader(out_lines));

    return RTN_OK;
}

static RETCODE OpenDBMapFile(std::ofstream& headerStream)
{
    std::stringstream dbMapHeaderPath;
    dbMapHeaderPath
        << INSTALL_DIR
        << COMMON_INC_PATH
        << DB_MAP_HEADER_NAME
        << HEADER_EXT;

    headerStream.open(dbMapHeaderPath.str(),
        std::fstream::in | std::fstream::out | std::fstream::trunc);

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up %s for writing", dbMapHeaderPath.str().c_str());
        return RTN_NOT_FOUND;
    }

    return RTN_OK;
}

static RETCODE WriteDBMapHeader(std::ofstream& headerStream)
{
    headerStream << "//GENERATED FILE! DO NOT MODIFY\n";
    headerStream << "#ifndef __DB_MAP_HH\n#define __DB_MAP_HH\n";

    headerStream <<
        "#include <string>\n#include <map>\n\n#include <allDBs.hh>\n";

    headerStream <<
        "\nstatic std::map<std::string, OBJECT_SCHEMA> dbSizes =\n    {";

    return RTN_OK;

}

static RETCODE WriteDBMapObject(std::ofstream& headerStream, const OBJECT_SCHEMA& object_entry)
{
    std::stringstream upperCaseSStream;
    upperCaseSStream << std::uppercase << std::string(object_entry.objectName);
    const std::string& objName = upperCaseSStream.str();
    headerStream
        << "\n        {"
        << "O_"
        << objName
        << "_NAME, O_"
        << objName
        << "_INFO},";

    return RTN_OK;
}

static RETCODE WriteDBMapFooter(std::ofstream& headerStream)
{
    headerStream << "\n    };\n\n#endif";
    return RTN_OK;
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
                LOG_WARN("Error reading object entry: %s%s.skm:%d",
                    skmPath.c_str(), objectName, currentLineNum);
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
                LOG_WARN("Error reading field entry: %s%s.skm:%d",
                    skmPath.c_str(), objectName, currentLineNum);
                return retcode;
            }

            if(!TrySetFieldSize(field_entry))
            {
                LOG_WARN("Invalid field entry: %s%s.skm:%d",
                    skmPath.c_str(), objectName, currentLineNum);
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

    retcode |= WriteObjectEnd(headerFile, object_entry);

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

RETCODE GenerateObjectDBFiles(const OBJECT& objectName,
    const std::string& skmPath,
    const std::string& incPath,
    std::ofstream& dbMapStream)
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
        LOG_WARN("Error adding %s to allHeader.hh",
            object_entry.objectName.c_str());
        return retcode;
    }
    LOG_INFO("Added to %s to allHeader.hh", object_entry.objectName.c_str());

    retcode |= WriteDBMapObject(dbMapStream, object_entry);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error adding %s to DBMap.hh",
            object_entry.objectName.c_str());
        return retcode;
    }

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

    size_t extension_index = skmFileName.rfind('.', skmFileName.length());
    if (extension_index != std::string::npos)
    {
        std::string ext_name = skmFileName.substr(extension_index,
            skmFileName.length() - extension_index);

        if(SKM_EXT == ext_name)
        {
            out_ObjectName = skmFileName.substr(0, extension_index);
            return RTN_OK;
        }


    }

    return RTN_NOT_FOUND;
}

RETCODE GenerateAllDBFiles(const std::string& skmPath, const std::string& incPath)
{
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

        std::ofstream dbMapStream;
        retcode |= OpenDBMapFile(dbMapStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error opening DBMap.hh");
            return retcode;
        }

        retcode |= WriteDBMapHeader(dbMapStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error writing DBMap.hh header");
            return retcode;
        }

        for(std::string& schema : schema_files)
        {
            OBJECT objName = {0};
            strncpy(objName, schema.c_str(), sizeof(objName));
            retcode |= GenerateObjectDBFiles(objName, skmPath, incPath, dbMapStream);
        }

        retcode |= WriteDBMapFooter(dbMapStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error writing DBMap.hh header");
            return retcode;
        }
        LOG_INFO("Generated DBMap.hh");

        return retcode;
    }
    else
    {
        /* could not open directory */
        return RTN_NOT_FOUND;
    }
}