#ifndef __UPDATE_DAEMON_HH
#define __UPDATE_DAEMON_HH
#include <DBMap.hh>
#include <ObjectReader.hh>
#include <DaemonThread.hh>
#include <DatabaseAccess.hh>
#include <INETMessenger.hh>
#include <Logger.hh>
#include <TasQ.hh>

#include <map>
#include <set>
#include <iostream>


class MonitorThread: public DaemonThread<TasQ<INET_PACKAGE*>*, TasQ<INET_PACKAGE*>*>
{

public:
    void execute(TasQ<INET_PACKAGE*>* incoming_objects, TasQ<INET_PACKAGE*>* outgoing_objects)
    {
        //DatabaseAccess db_object = DatabaseAccess(objectName);

        // Get pointer to first record N
        //char* p_read_pointer = db_object.Get(min_record);

        INET_PACKAGE* incoming_request;
        char* incoming_value = nullptr;
        unsigned long long data_recv = 0;
        unsigned long long data_sent = 0;

        while (StopRequested() == false)
        {
            while(incoming_objects->TryPop(incoming_request))
            {
                data_recv += incoming_request->header.message_size;
                LOG_DEBUG("Total bytes recevied: ", data_recv);
                OFRI ofri = {0};
                memcpy(&ofri, incoming_request->payload, sizeof(OFRI));
                LOG_INFO("GOT OFRI: ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i);

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

                // Check if a value was included which is extra data
                if(incoming_request->header.message_size > sizeof(OFRI))
                {
                    incoming_value = new char[incoming_request->header.message_size - sizeof(OFRI) + 1]; // +1 for null terminator
                    memcpy(incoming_value, incoming_request->payload + sizeof(OFRI), sizeof(incoming_value));
                    incoming_value[sizeof(incoming_value)] = '\0'; // For printing
                }

                DatabaseAccess& access = m_MonitoredObjects.at(ofri.o);
                char* p_read_pointer = access.Get(ofri.r);
                if(nullptr == p_read_pointer)
                {
                    LOG_WARN("Could not find record: ", ofri.r);
                    continue;
                }

                if(nullptr != incoming_value)
                {

                    if(IS_RETCODE_OK(access.WriteValue(ofri, incoming_value)))
                    {
                        LOG_INFO("Updated ", ofri.o, ".", ofri.f, ".",
                                  ofri.r, ".", ofri.i, " = ",
                                  std::string(incoming_value));
                    }
                    else
                    {
                        LOG_INFO("Failed to update ", ofri.o, ".", ofri.f, ".",
                                  ofri.r, ".", ofri.i, " with ",
                                  std::string(incoming_value));
                    }

                    delete[] incoming_value;
                    incoming_value = nullptr;
                }

                OBJECT_SCHEMA object_info;
                if(RTN_OK != TryGetObjectInfo(std::string(ofri.o), object_info))
                {
                    LOG_WARN("Could not find object: ", ofri.o);
                    continue;
                }

                PrintDBObject(object_info, p_read_pointer, ofri.r);

                /* create and send back info */
                if(RTN_OK != NotifySubscribers(object_info, ofri, p_read_pointer, outgoing_objects, data_sent))
                {
                    LOG_WARN("Could not notify subscribers of changes to: ", ofri.o, ".", ofri.r);
                }

                LOG_DEBUG("Total bytes sent: ", data_sent);

                delete incoming_request;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    }

private:

    RETCODE NotifySubscribers(const OBJECT_SCHEMA& object_info, const OFRI& ofri, char* updated_object, TasQ<INET_PACKAGE*>* outgoing_objects, unsigned long long& out_data_total)
    {
        std::unordered_map<OFRI, std::vector<CONNECTION>>::iterator subscribers = m_Subscriptions.find(ofri);
        if(m_Subscriptions.end() == subscribers)
        {
            return RTN_NOT_FOUND;
        }

        for(CONNECTION& connection : subscribers->second)
        {
            // Need to make new copies as Send() deletes them
            // @TODO: make this more efficient
            INET_PACKAGE* outgoing_package =
                reinterpret_cast<INET_PACKAGE*>(
                    new char[sizeof(INET_HEADER) +
                    object_info.objectSize]);

            outgoing_package->header.connection = connection;
            outgoing_package->header.data_type = MESSAGE_TYPE::DB;
            outgoing_package->header.message_size = object_info.objectSize;

            memcpy(outgoing_package->payload, updated_object, object_info.objectSize);

            LOG_DEBUG("Queue object update: ", ofri.o, ".", ofri.r, " to: ", connection.address, ":", connection.port );
            outgoing_objects->Push(outgoing_package);

            out_data_total += outgoing_package->header.message_size;
        }

        return RTN_OK;
    }

    RETCODE RemoveSubscriber(CONNECTION& removal_connection, const OFRI& ofri)
    {

        std::unordered_map<OFRI, std::vector<CONNECTION>>::iterator subscribers = m_Subscriptions.find(ofri);
        if(m_Subscriptions.end() == subscribers)
        {
            return RTN_NOT_FOUND;
        }

        /* Remove this connection from ofri */
        subscribers->second.erase(std::remove(
            subscribers->second.begin(),
            subscribers->second.end(),
            removal_connection), subscribers->second.end()
            );

        return RTN_OK;
    }

    std::unordered_map<std::string, DatabaseAccess> m_MonitoredObjects;
    std::unordered_map<OFRI, std::vector<CONNECTION>> m_Subscriptions;
};


#endif