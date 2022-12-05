#ifndef __UPDATE_DAEMON_HH
#define __UPDATE_DAEMON_HH
#include <DBMap.hh>
#include <DaemonThread.hh>
#include <DatabaseAccess.hh>

class MonitorThread: public DaemonThread<const OBJECT&>
{

public:
    // Function to be executed by thread function
    void execute(const OBJECT& objectName)
    {
        OBJECT_SCHEMA object_info;
        TryGetObjectInfo(std::string(objectName), object_info);

        DatabaseAccess db_object = DatabaseAccess(objectName);

        // Get pointer to beginning of db
        char* p_read_pointer = db_object.Get(0);
        // Get pointer to first record
        char* p_write_pointer = p_read_pointer + object_info.objectSize;

        while (StopRequested() == false)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }

        std::cout << "Stopped " << objectName << " Update thread!\n";

    }
};

#endif