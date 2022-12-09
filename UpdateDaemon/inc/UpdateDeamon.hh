#ifndef __UPDATE_DAEMON_HH
#define __UPDATE_DAEMON_HH
#include <DBMap.hh>
#include <DaemonThread.hh>
#include <DatabaseAccess.hh>
#include <INETMessenger.hh>

#include <map>
#include <iostream>

static bool PrintField (const FIELD_SCHEMA& field, char* p_object)
{
    std::stringstream fieldStream;
    char* p_fieldAddress = (p_object + field.fieldOffset);

    switch(field.fieldType)
    {
        case 'D': // Databse innacurate because its a string alias
        {
            fieldStream <<
                *reinterpret_cast<DATABASE*>(p_fieldAddress);
            break;
        }
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

class MonitorThread: public DaemonThread<const OBJECT&, RECORD, RECORD>
{

public:
    void execute(const OBJECT& objectName, RECORD min_record, RECORD max_record)
    {
        std::stringstream thread_write_stream;
        thread_write_stream << "Range: " << min_record << " -> " << max_record << "\n";
        std::cout << thread_write_stream.str();
        OBJECT_SCHEMA object_info;
        if(RTN_OK != TryGetObjectInfo(std::string(objectName), object_info))
        {
            LOG_WARN("Could not open %s! Exiting!", objectName);
            return;
        }

        DatabaseAccess db_object = DatabaseAccess(objectName);

        // Get pointer to beginning of db
        char* p_read_pointer = db_object.Get(min_record);
#if 0
        // Get pointer to first record
        char* p_write_pointer = p_read_pointer + object_info.objectSize;
        char* p_write_pointer_base = p_read_pointer_base;
#endif
        char* p_read_pointer_base = p_read_pointer;

        char* p_read_pointers[8] =
            {
                p_read_pointer,
                p_read_pointer + object_info.objectSize,
                p_read_pointer + (object_info.objectSize * 2),
                p_read_pointer + (object_info.objectSize * 3),
                p_read_pointer + (object_info.objectSize * 4),
                p_read_pointer + (object_info.objectSize * 5),
                p_read_pointer + (object_info.objectSize * 6),
                p_read_pointer + (object_info.objectSize * 7),
            };

        BASS copy = {'a','l','e','c'};

        //RECORD current_record = min_record;
        RECORD record_end = std::min(object_info.numberOfRecords, max_record);
        m_TotalRecs = 0;

        m_Hooks[171] = std::vector<int>{1, 2, 3};

        while (StopRequested() == false)
        {

            for(RECORD current_record = min_record; current_record < record_end; current_record += 8)
            {
                for(char index = 0; index < 8; index++)
                {
                    memcpy(p_read_pointers[index], &copy, sizeof(BASS));
                    m_TotalRecs++;
                }

                for(char index = 0; index < 8; index++)
                {
                    p_read_pointers[index] += object_info.objectSize;
                }

                current_record += 8;
            }

            for(char index = 0; index < 8; index++)
            {
                p_read_pointers[index] = p_read_pointer_base +
                    (object_info.objectSize) * index;
            }
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
#if 0
            /* [min_record, max_record) */
            if(current_record > record_end)
            {
                current_record = min_record;
                //p_read_pointer = p_read_pointer_base;
                for(char index = 0; index < 8; index++)
                {
                    p_read_pointers[index] = p_read_pointer_base +
                        (object_info.objectSize) * index;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }


            if(RTN_OK != Read(object_info, p_read_pointer, current_record))
            {
                LOG_WARN("Something went wrong..");
                return;
            }

            for(char index = 0; index < 8; index++)
            {
                memcpy(p_read_pointers[index], &copy, sizeof(BASS));
                m_TotalRecs++;
            }

            for(char index = 0; index < 8; index++)
            {
                p_read_pointers[index] += object_info.objectSize;
            }

            current_record += 8;
#endif
        }

        thread_write_stream.str("");
        thread_write_stream << "Monitored " << m_TotalRecs << " record(s)!\n";
        std::cout << thread_write_stream.str();
    }

    RETCODE Read(const OBJECT_SCHEMA& object, char* p_object, RECORD record)
    {
        std::map<RECORD, std::vector<int>>::iterator it = m_Hooks.find(record);
        if(it != m_Hooks.end())
        {
            for(int& socket: it->second)
            {
                m_ReadRecs++;
            }
        }

        return RTN_OK;
    }

    long long m_TotalRecs;
    RECORD m_ReadRecs;
    RECORD m_WroteRecs;
    std::map<RECORD, std::vector<int>> m_Hooks;
};

#endif