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
        RETCODE retcode = RTN_OK;
        INET_PACKAGE* incoming_request;
        INET_PACKAGE* outgoing_request;

        unsigned long long data_recv = 0;
        unsigned long long data_sent = 0;

        while (StopRequested() == false)
        {
            while(incoming_objects->TryPop(incoming_request))
            {
                data_recv += incoming_request->header.message_size;
                LOG_DEBUG("Total bytes recevied: ", data_recv);

                retcode = ProcessIncomingRequest(incoming_request, outgoing_request);
                if(not IS_RETCODE_OK(retcode))
                {
                    LOG_WARN("Could not gather returned info");
                    delete incoming_request;
                    continue;
                }

                outgoing_objects->Push(outgoing_request);
                LOG_DEBUG("Total bytes sent: ", data_sent);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        LOG_INFO("Ending update daemon thread!");

    }

private:

    RETCODE GetOFRIFromPayload(INET_PACKAGE* package, OFRI& out_ofri)
    {

        if(sizeof(OFRI) < package->header.message_size)
        {
            return RTN_BAD_ARG;
        }

        out_ofri = *reinterpret_cast<OFRI*>(package->payload);

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

    RETCODE ProcessIncomingRequest(INET_PACKAGE* incoming_request, INET_PACKAGE* outgoing_request)
    {

        RETCODE retcode = RTN_OK;
        switch(incoming_request->header.data_type)
        {
            case MESSAGE_TYPE::DB_READ:
            {
                OFRI ofri = {0};
                char* p_value = nullptr;

                retcode |= GetOFRIFromPayload(incoming_request, ofri);
                if(!IS_RETCODE_OK(retcode))
                {
                    LOG_WARN("Could not translate OFRI from: ", incoming_request->header.connection.address);
                    return retcode;
                }

                DatabaseAccess access;
                if(not TryGetDatabaseAccess(ofri, access))
                {
                    LOG_WARN("Could not open database for object: ", ofri.o);
                    return retcode & RTN_NOT_FOUND;
                }

                p_value = access.Get(ofri);
                if(nullptr == p_value)
                {
                    LOG_WARN("Could not get value for ofri: ", ofri);
                    return retcode & RTN_NULL_OBJ;
                }

                OBJECT_SCHEMA object_schema;
                if(not TryGetObjectInfo(ofri.o, object_schema))
                {
                    LOG_WARN("Could not get object schema for ofri: ", ofri);
                    return retcode & RTN_NOT_FOUND;
                }

                object_schema.fields[ofri.f].fieldSize;
                INET_PACKAGE* outgoing_request = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_PACKAGE) + sizeof(object_schema.fields[ofri.f].fieldSize)]);
                memcpy(&outgoing_request->header.connection, &incoming_request->header.connection, sizeof(INET_HEADER));
                outgoing_request->header.data_type = MESSAGE_TYPE::DB_READ;
                outgoing_request->header.message_size = object_schema.fields[ofri.f].fieldSize;
                memcpy(&outgoing_request->payload, p_value, sizeof(object_schema.fields[ofri.f].fieldSize));


                return retcode;
            }
            case MESSAGE_TYPE::DB_WRITE:
            {
                LOG_INFO("Got write request from", incoming_request->header.connection);
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

#if 0

    RETCODE NotifySubscribers(const OFRI& ofri, TasQ<INET_PACKAGE*>* outgoing_objects, unsigned long long& out_data_total)
    {
        std::unordered_map<OFRI, std::vector<CONNECTION>>::iterator subscribers = m_Subscriptions.find(ofri);
        if(m_Subscriptions.end() == subscribers)
        {
            return RTN_NOT_FOUND;
        }


        OBJECT_SCHEMA object_info;
        TryGetObjectInfo(ofri.o, object_info);

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
#endif
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