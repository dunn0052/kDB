#ifndef _DATABASE_SUBSCRIPTION_HH
#define _DATABASE_SUBSCRIPTION_HH

#include <INETMessenger.hh>

template <class OBJECT>
class DatabaseSubscription
{

public:

    DatabaseSubscription()
    {
        if(!DatabaseSubscription::s_DatabaseConnection.IsListening())
        {
            s_DatabaseConnection.Connect("127.0.0.1", "5000");
        }
    }

    void Update()
    {
        s_DatabaseConnection.Recieve(m_LocalObject, sizeof(OBJECT));
    }

private:

        static INETMessenger s_DatabaseConnection;
        char m_LocalObject[sizeof(OBJECT)] = '\0';

};

#ifndef _DATABASE_SUBSCRIPTION_STATIC_MEMBERS
#define _DATABASE_SUBSCRIPTION_STATIC_MEMBERS

template <class OBJECT>
INETMessenger DatabaseSubscription<OBJECT>::s_DatabaseConnection{};

#endif

#endif