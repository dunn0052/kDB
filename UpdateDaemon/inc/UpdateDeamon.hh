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

                ProcessIncomingRequest(incoming_request);

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

    RETCODE GetOFRIFromPayload(INET_PACKAGE* package, OFRI* out_ofri)
    {

        if(sizeof(OFRI) < package->header.message_size)
        {
            return RTN_BAD_ARG;
        }

        out_ofri = reinterpret_cast<OFRI*>(package->payload);

        return RTN_OK;
    }

    RETCODE GetValueFromPayload(INET_PACKAGE* package, char* out_value)
    {
        if(sizeof(OFRI) <= package->header.message_size)
        {
            return RTN_BAD_ARG;
        }

        out_value = package->payload + sizeof(OFRI);
    }

    RETCODE ProcessIncomingRequest(INET_PACKAGE* incoming_request)
    {

        RETCODE retcode = RTN_OK;
        switch(incoming_request->header.data_type)
        {
            case MESSAGE_TYPE::DB_READ:
            {
                OFRI *p_ofri = nullptr;
                retcode |= GetOFRIFromPayload(incoming_request, p_ofri);

                if(!IS_RETCODE_OK(retcode))
                {
                    LOG_WARN("Could not translate OFRI from: ", incoming_request->header.connection.address);
                }
            }
            case MESSAGE_TYPE::DB_WRITE:
            {
                OFRI *p_ofri = nullptr;
                char* incoming_value = nullptr;
                retcode |= GetOFRIFromPayload(incoming_request, p_ofri);
                if(!IS_RETCODE_OK(retcode))
                {
                    LOG_WARN("Could not translate OFRI from: ", incoming_request->header.connection.address);
                }

                retcode |= GetValueFromPayload(incoming_request, incoming_value);
                if(!IS_RETCODE_OK(retcode))
                {
                    LOG_WARN("Could not get updated value for OFRI: ", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i);
                }

                retcode |= UpdateValue(p_ofri, incoming_value);
                if(!IS_RETCODE_OK(retcode))
                {
                    LOG_WARN("Could not update value for OFRI:", ofri.o, ".", ofri.f, ".", ofri.r, ".", ofri.i);
                }

                /* create and send back info */
                if(RTN_OK != NotifySubscribers(object_info, ofri, p_read_pointer, outgoing_objects, data_sent))
                {
                    LOG_WARN("Could not notify subscribers of changes to: ", ofri.o, ".", ofri.r);
                }
            }
            case MESSAGE_TYPE::TEXT:
            case MESSAGE_TYPE::NONE:
            default:
            {
                LOG_DEBUG("Received an invalid request type: ", incoming_request->header.data_type);
                retcode |= RTN_BAD_ARG;
            }

            return retcode;
        }

    }

    RETCODE TryGetDatabaseAccess(const OFRI& ofri, DatabaseAccess& out_access)
    {
        const std::unordered_map<std::string, DatabaseAccess>::iterator& monitoredObject
            = m_MonitoredObjects.find(ofri.o);

        if(m_MonitoredObjects.end() == monitoredObject)
        {
            DatabaseAccess access = DatabaseAccess(ofri.o);
            if(!access.IsValid())
            {
                return RTN_NOT_FOUND;
            }

            m_MonitoredObjects.emplace(ofri.o, access);
        }

        out_access = m_MonitoredObjects.at(ofri.o);

        return RTN_OK;
    }

    RETCODE UpdateValue(const OFRI* p_ofri, char* updated_value)
    {
        DatabaseAccess access;

        RETCODE retcode = TryGetDatabaseAccess(*p_ofri, access);

        if(IS_RETCODE_OK(access.WriteValue(*p_ofri, updated_value)))
        {
            LOG_INFO("Updated ", p_ofri->o, ".", p_ofri->f, ".",
                        p_ofri->r, ".", p_ofri->i, " = ",
                        std::string(updated_value));
        }
        else
        {
            LOG_INFO("Failed to update ", p_ofri->o, ".", p_ofri->f, ".",
                        p_ofri->r, ".", p_ofri->i, " with ",
                        std::string(updated_value));
        }

        return RTN_OK;
    }

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
            outgoing_package->header.data_type = MESSAGE_TYPE::DB_READ;
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