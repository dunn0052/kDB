#ifndef __OBJECT_SCHEMA_HH
#define __OBJECT_SCHEMA_HH

#include <string>
#include <iostream>
#include <vector>

struct FIELD_SCHEMA
{
    size_t fieldNumber;
    std::string fieldName;
    char fieldType;
    size_t numElements;
    size_t fieldSize;
    size_t fieldOffset;
    bool isMultiIndex;
};

inline std::istream& operator >> (std::istream& input_stream,
    FIELD_SCHEMA& field_entry)
{
    return input_stream 
        >> field_entry.fieldNumber
        >> field_entry.fieldName
        >> field_entry.fieldType
        >> field_entry.numElements;
}

inline std::ostream& operator << (std::ostream& output_stream,
    FIELD_SCHEMA& field_entry)
{
    return output_stream 
        << field_entry.fieldNumber
        << field_entry.fieldName
        << field_entry.fieldType
        << field_entry.numElements;
}

struct OBJECT_SCHEMA
{
    size_t objectNumber;
    std::string objectName;
    size_t numberOfRecords;
    std::vector<FIELD_SCHEMA> fields;
    size_t objectSize;
};

inline std::istream& operator >> (std::istream& input_stream,
    OBJECT_SCHEMA& object_entry)
{
    return input_stream
        >> object_entry.objectNumber
        >> object_entry.objectName
        >> object_entry.numberOfRecords;
}

inline std::ostream& operator << (std::ostream& output_stream,
    const OBJECT_SCHEMA& object_entry)
{
    return output_stream
        << object_entry.objectNumber
        << object_entry.objectName
        << object_entry.numberOfRecords;
}

#endif