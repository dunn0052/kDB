#ifndef __UPDATE_DAEMON_HH
#define __UPDATE_DAEMON_HH
#include <DBMap.hh>
#include <DaemonThread.hh>
#include <DatabaseAccess.hh>
#include <INETMessenger.hh>
#include <Logger.hh>
#include <TasQ.hh>

#include <map>
#include <set>
#include <iostream>

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

class MonitorThread: public DaemonThread<TasQ<INET_PACKAGE*>*, TasQ<INET_PACKAGE*>*>
{

public:
    void execute(TasQ<INET_PACKAGE*>* incoming_objects, TasQ<INET_PACKAGE*>* outgoing_objects)
    {
        //DatabaseAccess db_object = DatabaseAccess(objectName);

        // Get pointer to first record N
        //char* p_read_pointer = db_object.Get(min_record);

        INET_PACKAGE* incoming_request;
        while (StopRequested() == false)
        {
            while(incoming_objects->TryPop(incoming_request))
            {
                LOG_INFO("Monitor thread received message");
                OFRI ofri = {0};
                memcpy(&ofri, incoming_request->payload, sizeof(OFRI));
                LOG_INFO("GOT OFRI: ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i);


                OBJECT_SCHEMA object_info;
                if(RTN_OK != TryGetObjectInfo(std::string(ofri.o), object_info))
                {
                    LOG_WARN("Could not find object: ", ofri.o);
                    continue;
                }

                // Try and get DB access otherwise fail
                if(m_MonitoredObjects.find(ofri.o) == m_MonitoredObjects.end())
                {
                    LOG_DEBUG("Did not find object ", ofri.o, ". Adding to monitored objects");

                    DatabaseAccess db_access = DatabaseAccess(ofri.o);
                    if(db_access.IsValid())
                    {
                        m_MonitoredObjects.emplace(ofri.o, DatabaseAccess(ofri.o));
                    }
                    else
                    {
                        LOG_WARN("Could not open object: ", ofri.o);
                        continue;
                    }
                }
                DatabaseAccess& access = m_MonitoredObjects.at(ofri.o);
                char* p_read_pointer = access.Get(ofri.r);

                PrintDBObject(object_info, p_read_pointer, ofri.r);

                INET_PACKAGE* outgoing_package = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_HEADER) + object_info.objectSize]);
                memcpy(outgoing_package, &(incoming_request->header), sizeof(INET_HEADER));
                memcpy(outgoing_package->payload, p_read_pointer, object_info.objectSize);
                outgoing_objects->Push(outgoing_package);
                delete incoming_request;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    }

    std::map<std::string, DatabaseAccess> m_MonitoredObjects;
    std::map<OFRI, std::vector<CONNECTION>> m_Monitors;
};


#endif