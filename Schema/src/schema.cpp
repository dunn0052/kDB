#include <schema.hh>
#include <fcntl.h>
#include <unistd.h>
#include <Constants.hh>
#include <dirent.h>
#include <bits/stdc++.h>
#include <ConfigValues.hh>

/* Object info */
RETCODE ParseObjectEntry(std::istringstream& line, OBJECT_SCHEMA& out_object)
{
    line >> out_object;
    if( line.fail() )
    {
        return RTN_NOT_FOUND;
    }

    LOG_DEBUG("OBJECT NUMBER: ", out_object.objectNumber, " OBJECT NAME: ", out_object.objectName, " NUMBER OF RECORDS: ", out_object.numberOfRecords);

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

    LOG_DEBUG("FIELD NUMBER: ", out_field.fieldNumber, " FIELD NAME: ", out_field.fieldName, " FIELD TYPE: ", out_field.fieldType, " NUMBER OF ELEMENTS: ", out_field.numElements);

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
    headerFile << "\n\n#include <OFRI.hh>\n#include <ObjectSchema.hh>\n"; // maybe..

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
    switch(field.fieldType)
    {
        case 'c': // Char
        {
            dataType = "char";
            break;
        }
        case 's': // String
        {
            dataType = "char";
            break;
        }
        case 'i': // signed integer
        {
            dataType = "int";
            break;
        }
        case 'I': // Unsigned integer
        {
            dataType = "unsigned int";
            break;
        }
        case '?': // Bool
        {
            dataType = "bool";
            break;
        }
        case 'B': // Unsigned char (byte)
        {
            dataType = "unsigned char";
            break;
        }
        case 'x':
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
        case 'c': // Char
        {
            field.fieldSize = sizeof(char);
            field.isMultiIndex = false;
            break;
        }
        case 's': //String
        {
            field.fieldSize = sizeof(char);
            field.isMultiIndex = true;
            break;
        }
        case 'i': // signed integer
        {
            field.fieldSize = sizeof(int);
            field.isMultiIndex = false;
            break;
        }
        case 'I': // Unsigned integer
        {
            field.fieldSize = sizeof(unsigned int);
            field.isMultiIndex = false;
            break;
        }
        case '?': // Bool
        {
            field.fieldSize = sizeof(bool);
            field.isMultiIndex = false;
            break;
        }
        case 'B': // Unsigned char (byte)
        {
            field.fieldSize = sizeof(unsigned char);
            field.isMultiIndex = false;
            break;
        }
        case 'x':
        {
            field.fieldSize = sizeof(unsigned char);
            field.isMultiIndex = false;
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

static bool IsInByteBounds(const OBJECT_SCHEMA& object, const FIELD_SCHEMA& field, unsigned int& out_extra_bytes)
{
    out_extra_bytes = ( object.objectSize + field.fieldSize ) % WORD_SIZE;

    // If this field doesn't end on a word boundary then we will need to add padding
    if( out_extra_bytes )
    {
        return false;
    }

    return true;
}

static RETCODE AddBytePadding(OBJECT_SCHEMA& object, unsigned int extra_bytes)
{
    // Implement byte padding
    return RTN_OK;
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

/*
    OBJECT.py
    class OBJECT:

        FORMAT = "C struct format"

        def __init__(self, data:tuple):
            if len(data) != number of expected members:
                return None
            self.member = data[0]
            self.member2 = data[1:4]
            etc ...

        def __str__(self):
            return f"member: {str(self.member)} member2:{str(self.member2)} etc..."
*/
static RETCODE GenerateObjectPythonFile(std::ofstream& pythonFile, OBJECT_SCHEMA& object)
{
    std::stringstream format;
    std::stringstream classDefine;
    std::stringstream classInitFunc;
    std::stringstream classVariables;
    std::stringstream strFunc;

    classDefine << "# THIS FILE WAS GENERATED. DO NOT MODIFY!\n\n";
    classDefine << "class " << std::uppercase << object.objectName << ":\n";
    classInitFunc << "    def __init__(self, data:tuple):\n";
    classInitFunc << "        if len(data) != ";
    strFunc << "\n\n    def __str__(self) -> str:\n";
    strFunc << "        return f\"";
    // OBJECT_FORMAT = "format"
    format << "\n    FORMAT = \"";
    size_t dataIndex = 0;
    for(const FIELD_SCHEMA& field : object.fields)
    {
        classVariables << "        self."
                       << field.fieldName
                       << " = data["
                       << dataIndex;


        if(field.numElements > 1)
        {
            format << field.numElements; // Add number of elements
            if(!field.isMultiIndex) // s (string) is compound so we consider it a single field even though it has a number of elements
            {
                dataIndex += field.numElements - 1; // dataIndex == starting so ending is - 1 that way we get data[start:end]
                classVariables << ":" << dataIndex;
            }
        }

        if(field.isMultiIndex) // Print like a string
        {
            strFunc << field.fieldName << ": { str(self." << field.fieldName << ", encoding = 'UTF-8').rstrip('\00) }";
        }
        else
        {

            strFunc << field.fieldName << ": {self." << field.fieldName << "} ";
        }

        classVariables << "]\n";

        switch(field.fieldType)
        {
            case 'I': // Unsigned integer
            {
                format << "I";
                break;
            }
            case 'c': // Char
            {
                format << "c";
                break;
            }
            case 's': // String
            {
                format << "s";
                break;
            }
            case 'i': // signed integer
            {
                format << "i";
                break;
            }
            case '?': // Bool
            {
                format << "?";
                break;
            }
            case 'B': // Unsigned char (byte)
            {
                format << ( field.numElements > 1  ? "p" : "B" );
                break;
            }
            case 'x':
            {
                // byte padding for struct alignment
                format << "x";
                break;
            }
            default:
            {
                LOG_ERROR("Invalid field type: ", field.fieldName);
                return RTN_BAD_ARG;
            }
        }

        dataIndex++;
    }

    format << "\"\n\n";
    strFunc << "\"";
    classInitFunc << dataIndex << ":\n";
    classInitFunc << "            return None\n\n";

    pythonFile << classDefine.str() << format.str()
               << classInitFunc.str() << classVariables.str()
               << strFunc.str();
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
        LOG_WARN("Failed to open or create ", path);
        retcode |=  RTN_NOT_FOUND;
    }

    if( ftruncate64(fd, fileSize) )
    {
        LOG_WARN("Failed to truncate ", path, " to size ", fileSize);
        retcode |= RTN_MALLOC_FAIL;
    }

    if( close(fd) )
    {
        LOG_WARN("Failed to close ", path);
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
    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        return RTN_NOT_FOUND;
    }
    allDBHeaderPath
        << INSTALL_DIR
        << COMMON_INC_PATH
        << ALL_DB_HEADER_NAME
        << HEADER_EXT;

    headerStream.open(allDBHeaderPath.str(),
        std::fstream::in | std::fstream::out | std::fstream::app);

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up ", allDBHeaderPath.str(), " for reading!");
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
    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        return RTN_NOT_FOUND;
    }
    allDBHeaderPath
        << INSTALL_DIR
        << COMMON_INC_PATH
        << ALL_DB_HEADER_NAME
        << HEADER_EXT;

    std::ofstream headerStream;
    headerStream.open(allDBHeaderPath.str());

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up ", allDBHeaderPath.str(), " for writing");
        return RTN_NOT_FOUND;
    }

    for( std::string& line : out_lines)
    {
        headerStream << line << "\n";
    }

    headerStream.close();

    return RTN_OK;
}


static RETCODE readAllDBPy(OBJECT_SCHEMA& object_entry, std::vector<std::string>& out_lines)
{
    std::string line;
    bool entry_replaced = false;
    size_t lineNum = 1;
    std::ifstream pyStream;
    std::stringstream allDBPyPath;
    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        return RTN_NOT_FOUND;
    }
    allDBPyPath
        << INSTALL_DIR
        << PYTHON_API_PATH
        << ALL_DB_HEADER_NAME
        << PY_EXT;

    pyStream.open(allDBPyPath.str(),
        std::fstream::in | std::fstream::out | std::fstream::app);

    if( !pyStream.is_open() )
    {
        LOG_WARN("Could not open up ", allDBPyPath.str(), " for reading!");
        return RTN_NOT_FOUND;
    }

    // Update entry
    while( std::getline(pyStream, line) )
    {
        if( object_entry.objectNumber == lineNum )
        {
            line = "import PythonAPI.db." + object_entry.objectName;
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

        out_lines.push_back("import PythonAPI.db." +  object_entry.objectName);
    }

    pyStream.close();

    return RTN_OK;
}

static RETCODE writeAllDBPy(std::vector<std::string>& out_lines)
{
    std::stringstream allDBHeaderPath;
    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        return RTN_NOT_FOUND;
    }
    allDBHeaderPath
        << INSTALL_DIR
        << PYTHON_API_PATH
        << ALL_DB_HEADER_NAME
        << PY_EXT;

    std::ofstream headerStream;
    headerStream.open(allDBHeaderPath.str());

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up ", allDBHeaderPath.str(), " for writing");
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

static RETCODE AddToAllDBPy(OBJECT_SCHEMA& object_entry)
{
    std::vector<std::string> out_lines;

    RETURN_RETCODE_IF_NOT_OK(readAllDBPy(object_entry, out_lines));

    RETURN_RETCODE_IF_NOT_OK(writeAllDBPy(out_lines));

    return RTN_OK;
}

static RETCODE OpenDBMapFile(std::ofstream& headerStream)
{
    std::stringstream dbMapHeaderPath;
    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        return RTN_NOT_FOUND;
    }
    dbMapHeaderPath
        << INSTALL_DIR
        << COMMON_INC_PATH
        << DB_MAP_HEADER_NAME
        << HEADER_EXT;

    headerStream.open(dbMapHeaderPath.str(),
        std::fstream::in | std::fstream::out | std::fstream::trunc);

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up ", dbMapHeaderPath.str(), " for writing");
        return RTN_NOT_FOUND;
    }

    return RTN_OK;
}

static RETCODE OpenDBMapPyFile(std::ofstream& headerStream)
{
    std::stringstream dbMapPyPath;
    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" == INSTALL_DIR)
    {
        return RTN_NOT_FOUND;
    }
    dbMapPyPath
        << INSTALL_DIR
        << PYTHON_API_PATH
        << DB_MAP_HEADER_NAME
        << PY_EXT;

    headerStream.open(dbMapPyPath.str(),
        std::fstream::in | std::fstream::out | std::fstream::trunc);

    if( !headerStream.is_open() )
    {
        LOG_WARN("Could not open up ", dbMapPyPath.str(), " for writing");
        return RTN_NOT_FOUND;
    }

    return RTN_OK;
}

static RETCODE WriteDBMapHeader(std::ofstream& headerStream)
{
    headerStream << "//GENERATED FILE! DO NOT MODIFY\n";
    headerStream << "#ifndef __DB_MAP_HH\n#define __DB_MAP_HH\n";

    headerStream <<
        "#include <string>\n#include <map>\n\n#include <retcode.hh>\n#include <allDBs.hh>\n";

    headerStream <<
        "\nstatic std::map<std::string, OBJECT_SCHEMA> dbSizes =\n    {";

    return RTN_OK;

}

static RETCODE WriteDBMapPyHeader(std::ofstream& headerStream)
{
    // DBMap.py contains a dictionary of all DB objects
    headerStream << "#GENERATED FILE! DO NOT MODIFY\n";

    headerStream <<
        "import PythonAPI.allDBs as allDBs\n\n";

    headerStream <<
        "ALL_OBJECTS = {";

    return RTN_OK;

}

static RETCODE WriteDBMapObject(std::ofstream& headerStream, const OBJECT_SCHEMA& object_entry)
{
    std::stringstream upperCaseSStream;
    upperCaseSStream << std::uppercase << object_entry.objectName;
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

static RETCODE WriteDBMapPyObject(std::ofstream& pyStream, const OBJECT_SCHEMA& object_entry)
{
    // "OBJECT":allDBs.PythonAPI.db.OBJECT.OBJEC,
    std::stringstream upperCaseSStream;
    upperCaseSStream << std::uppercase << object_entry.objectName;
    const std::string& objName = upperCaseSStream.str();
    pyStream
        << "\n    \"" << objName << "\":allDBs.PythonAPI.db." << objName << "." << objName << ",";

    return RTN_OK;
}

static RETCODE WriteDBMapFooter(std::ofstream& headerStream)
{
    headerStream << "\n    };";

    headerStream
        << "\n\nstatic RETCODE TryGetObjectInfo(const std::string& objectName, OBJECT_SCHEMA& object)\n"
        << "{\n"
        <<"    std::map<std::string, OBJECT_SCHEMA>::iterator it = dbSizes.find(std::string(objectName));\n"
        <<"    if(it != dbSizes.end())\n"
        <<"    {\n"
        <<"        object = it->second;\n"
        <<"        return RTN_OK;\n"
        <<"    }\n"
        <<"\n"
        <<"    return RTN_NOT_FOUND;\n"
        <<"}\n";

    headerStream << "\n#endif";

    return RTN_OK;
}

static RETCODE WriteDBMapPyFooter(std::ofstream& pyStream)
{
    pyStream << "\n}";

    return RTN_OK;
}

static RETCODE GenerateObject(const OBJECT& objectName, const std::string& skmPath, OBJECT_SCHEMA& out_object_entry, bool strict)
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
        LOG_WARN("Could not open ", schema_path.str());
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
                LOG_WARN("Error reading object entry: ", skmPath, objectName, ".skm:", currentLineNum);
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
                LOG_WARN("Error reading field entry: ", skmPath, objectName, ".skm:", currentLineNum);
                return retcode;
            }

            if(!TrySetFieldSize(field_entry))
            {
                LOG_WARN("Invalid field entry: ", skmPath  , objectName, ".skm:", currentLineNum);
                retcode |= RTN_NOT_FOUND;
                return retcode;
            }

            unsigned int bytes_needed_for_byte_bounds;
            if(!IsInByteBounds(out_object_entry, field_entry, bytes_needed_for_byte_bounds))
            {
                if(strict)
                {
                    LOG_WARN("Field: ", field_entry.fieldName,
                        " is outside of byte bounds.\nAdd ",
                        bytes_needed_for_byte_bounds,
                        " bytes at the end of the field");
                    return RTN_BAD_ARG;
                }

                retcode |= AddBytePadding(out_object_entry, bytes_needed_for_byte_bounds);

            }

            out_object_entry.objectSize += field_entry.fieldSize;
            out_object_entry.fields.push_back(field_entry);

            continue;
        }
    }

    if( schemaFile.bad() )
    {
        LOG_WARN("Error reading ", skmPath, objectName, ".skm");
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
        LOG_ERROR("Could not open ", header_file_path);
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

static RETCODE GeneratePythonFile(OBJECT_SCHEMA& object_entry, const std::string& py_file_path)
{
    std::ofstream pythonFile;
    RETCODE retcode = RTN_OK;

    pythonFile.open(py_file_path);
    if( !pythonFile.is_open() )
    {
        LOG_ERROR("Could not open ", py_file_path);
    }

    retcode |= GenerateObjectPythonFile(pythonFile, object_entry);

    pythonFile.close();

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
    const std::string& pyPath,
    std::ofstream& dbMapStream,
    std::ofstream& dbMapPyStream,
    bool strict)
{
    OBJECT_SCHEMA object_entry;
    RETCODE retcode = GenerateObject(objectName, skmPath, object_entry, strict);
    if( RTN_OK != retcode )
    {
        return retcode;
    }

    std::stringstream header_path;
    header_path << incPath << objectName << HEADER_EXT;
    retcode |= GenerateHeaderFile(object_entry, header_path.str());
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error generating ", objectName, HEADER_EXT);
        return retcode;
    }
    LOG_INFO("Generated ", objectName, HEADER_EXT);

    std::stringstream python_path;
    python_path << pyPath << objectName << PY_EXT;
    retcode |= GeneratePythonFile(object_entry, python_path.str());
    if( RTN_OK != retcode )
    {
        LOG_ERROR("Error genearating", objectName, PY_EXT);
    }
    LOG_INFO("Generated ", objectName, PY_EXT);

    std::string INSTALL_DIR =
        ConfigValues::Instance().Get(KDB_INSTALL_DIR);
    if("" != INSTALL_DIR)
    {
        retcode |= GenerateDatabaseFile(object_entry, objectName, INSTALL_DIR + DB_DB_DIR);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error generating ", object_entry.objectName, DB_EXT);
            return retcode;
        }
        LOG_INFO("Generated ", object_entry.objectName, DB_EXT);
    }
    else
    {
        LOG_WARN("Error generating ", object_entry.objectName, DB_EXT);
    }

    retcode |= AddToAllDBHeader(object_entry);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error adding ", object_entry.objectName, " to allHeader", HEADER_EXT);
        return retcode;
    }
    LOG_INFO("Added ", object_entry.objectName, " to allHeader", HEADER_EXT);

    retcode |= AddToAllDBPy(object_entry);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error adding ", object_entry.objectName, " to allHeader", PY_EXT);
        return retcode;
    }
    LOG_INFO("Added ", object_entry.objectName, " to allHeader", PY_EXT);

    retcode |= WriteDBMapObject(dbMapStream, object_entry);
    if( RTN_OK != retcode )
    {
        LOG_WARN("Error adding ", object_entry.objectName, " to DBMap.hh");
        return retcode;
    }

    retcode |= WriteDBMapPyObject(dbMapPyStream, object_entry);
    if(RTN_OK != retcode )
    {
        LOG_WARN("Error adding ", object_entry.objectName, " to DBMap.py");
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

RETCODE GenerateAllDBFiles(const std::string& skmPath, const std::string& incPath, const std::string& pyPath, bool strict)
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
                LOG_DEBUG("Found schema file: ", foundFile);
                schema_files.push_back(objectName);
            }
            else
            {
                LOG_WARN("File: ", foundFile, " does not have a ", SKM_EXT, " extension");
                retcode = RTN_OK;
            }
        }

        closedir (directory);

        std::ofstream dbMapStream;
        retcode |= OpenDBMapFile(dbMapStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error opening ", DB_MAP_HEADER_NAME, HEADER_EXT);
            return retcode;
        }

        retcode |= WriteDBMapHeader(dbMapStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error writing ", DB_MAP_HEADER_NAME, HEADER_EXT);
            return retcode;
        }

        std::ofstream dbMapPyStream;
        retcode |= OpenDBMapPyFile(dbMapPyStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error opening ", DB_MAP_HEADER_NAME, PY_EXT);
        }

        retcode |= WriteDBMapPyHeader(dbMapPyStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error writing ", DB_MAP_HEADER_NAME, PY_EXT);
            return retcode;
        }

        for(std::string& schema : schema_files)
        {
            OBJECT objName = {0};
            strncpy(objName, schema.c_str(), sizeof(objName));
            retcode |= GenerateObjectDBFiles(objName, skmPath, incPath,
                pyPath, dbMapStream, dbMapPyStream, strict);
            if(RTN_OK != retcode)
            {
                LOG_WARN("Error generating ", objName, DB_EXT);
                return retcode;
            }
            LOG_INFO("Generated ", objName, DB_EXT);
        }

        retcode |= WriteDBMapFooter(dbMapStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error writing ", DB_MAP_HEADER_NAME, HEADER_EXT);
            return retcode;
        }
        LOG_INFO("Generated ", DB_MAP_HEADER_NAME, HEADER_EXT);

        retcode |= WriteDBMapPyFooter(dbMapPyStream);
        if( RTN_OK != retcode )
        {
            LOG_WARN("Error writing ", DB_MAP_HEADER_NAME, PY_EXT);
            return retcode;
        }
        LOG_INFO("Generated ", DB_MAP_HEADER_NAME, HEADER_EXT);

        return retcode;
    }
    else
    {
        LOG_ERROR("Could not find schema path: ", skmPath);
        /* could not open directory */
        return RTN_NOT_FOUND;
    }
}