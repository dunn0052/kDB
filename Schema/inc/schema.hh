#ifndef SCHEMA__HH
#define SCHEMA__HH

#include <CLI.hh>
#include <DOFRI.hh>
#include <retcode.hh>

#include <vector>
#include <fstream>
#include <sstream>

struct FIELD_SCHEMA
{
    size_t fieldNumber;
    std::string fieldName;
    char fieldType;
    size_t numElements;
    size_t fieldSize;
};

inline std::istream& operator >> (std::istream& input_stream, FIELD_SCHEMA& field_entry)
{
    return input_stream >> field_entry.fieldNumber >> field_entry.fieldName >> field_entry.fieldType >> field_entry.numElements;
}

struct OBJECT_SCHEMA
{
    size_t objectNumber;
    std::string objectName;
    size_t numberOfRecords;
    std::vector<FIELD_SCHEMA> fields;
    size_t objectSize;
};

inline std::istream& operator >> (std::istream& input_stream, OBJECT_SCHEMA& object_entry)
{
    return input_stream >> object_entry.objectNumber >> object_entry.objectName >> object_entry.numberOfRecords;
}

struct DATABASE_SCHEMA
{
    std::string databaseName;
    size_t databaseNumber;
    std::string headerName;
    size_t totalSize;
    std::vector<OBJECT_SCHEMA> objects;
};

inline std::istream& operator >> (std::istream& input_stream, DATABASE_SCHEMA& database_entry)
{
    return input_stream >> database_entry.databaseNumber >>  database_entry.databaseName;
}

RETCODE GenerateDatabase(const DATABASE& databaseName);

#endif