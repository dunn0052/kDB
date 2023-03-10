#ifndef __OBJECT_READER_HH
#define __OBJECT_READER_HH

#include <ObjectSchema.hh>
#include <OFRI.hh>

#include <iostream>
#include <sstream>

static bool PrintField (const FIELD_SCHEMA& field, char* p_object)
{
    std::stringstream fieldStream;
    char* p_fieldAddress = (p_object + field.fieldOffset);

    switch(field.fieldType)
    {
        case 'O': // Object
        {
            fieldStream <<
                *reinterpret_cast<OBJECT*>(p_fieldAddress);
            break;
        }
        case 'F': // Field
        {
            fieldStream <<
                *reinterpret_cast<FIELD*>(p_fieldAddress);
            break;
        }
        case 'R': // Record
        {
            fieldStream <<
                *reinterpret_cast<RECORD*>(p_fieldAddress);
            break;
        }
        case 'I': // Index
        {
            fieldStream <<
                *reinterpret_cast<INDEX*>(p_fieldAddress);
            break;
        }
        case 'C': // Char
        {
            fieldStream <<
                *reinterpret_cast<char*>(p_fieldAddress);
            break;
        }
        case 'S': // String
        {
            fieldStream.rdbuf()->sputn(reinterpret_cast<char*>(p_fieldAddress),
                        sizeof(char) * field.numElements);
            break;
        }
        case 'N': // signed integer
        {
            fieldStream <<
                *reinterpret_cast<int*>(p_fieldAddress);
            break;
        }
        case 'U': // Unsigned integer
        {
            fieldStream <<
                *reinterpret_cast<unsigned int*>(p_fieldAddress);
            break;
        }
        case 'B': // Bool
        {
            fieldStream <<
                *reinterpret_cast<bool*>(p_fieldAddress);
            break;
        }
        case 'Y': // Unsigned char (byte)
        {
            fieldStream <<
                *reinterpret_cast<unsigned char*>(p_fieldAddress);
            break;
        }
        default:
        {
            return false;
        }
    }

    std::cout << " " << fieldStream.str();
    return true;
}

void PrintDBObject(const OBJECT_SCHEMA& object, char* p_object, RECORD rec_num)
{
    std::cout << "Name: " << object.objectName << "\n";
    std::cout << "Number: " << object.objectNumber << "\n";
    std::cout << "Record: " << rec_num << "\n";
    for(const FIELD_SCHEMA& field : object.fields)
    {
        std::cout
            << "    "
            << field.fieldName;

        PrintField(field, p_object);
        std::cout << "\n";
    }
    std::cout << "\n";
}

#endif