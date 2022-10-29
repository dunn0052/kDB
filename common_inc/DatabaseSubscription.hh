#ifndef _DATABASE_SUBSCRIPTION_HH
#define _DATABASE_SUBSCRIPTION_HH

#include <INETMessenger.hh>
#include <DOFRI.hh>

template <class OBJECT>
class DatabaseSubscription
{

public:

    DatabaseSubscription()
    {
        if(!DatabaseSubscription::s_DatabaseConnection.IsListening())
        {
            // Need to figure out some configuration here to the server
            s_DatabaseConnection.Connect("127.0.0.1", "5000");
        }
    }

    // Check for any new info about the OBJECT
    void Update()
    {
        s_DatabaseConnection.Receive(m_LocalObject, sizeof(OBJECT));
    }


    // Copy updated_object values to the DB
    RETCODE Set(OBJECT updated_object)
    {
        // Should send to specific IP:PORT??
        return s_DatabaseConnection.Send(&updated_object);
    }

private:

        static INETMessenger s_DatabaseConnection;
        char m_LocalObject[sizeof(OBJECT)] = '\0';

        // C++17 Only feature. Otherwise define in separate .cpp file
        inline static INETMessenger DatabaseSubscription<OBJECT>::s_DatabaseConnection{};
};

#endif