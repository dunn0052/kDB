#ifndef MESSENGER__HH
#define MESSENGER__HH
#include <retcode.hh>
#include <stddef.h>
#include <DOFRI.hh>
#include <sstream>
#include <sys/socket.h>
#include <un.h>

constexpr unsigned short SOCKET_NAME_SIZE = 60;

typedef char SOCKET_NAME[SOCKET_NAME_SIZE];

struct MESSAGE_HEADER
{
    size_t size;
    SOCKET_NAME return_address;
};

class Messenger
{
public:
    Messenger(SOCKET_NAME name)
        : m_MessageID(0)
    {
        strncpy(m_SockeName, name, sizeof(m_SockeName));
        CreateQueue();
    }

    ~Messenger()
    {
        DestroyQueue();
    }

    RETCODE Send(const SOCKET_NAME& name, const MESSAGE_HEADER& message)
    {
        msgsnd(m_MessageID, &message, message.size + sizeof(MESSAGE_HEADER), 0);
    }

    RETCODE Recieve(MESSAGE_HEADER& message)
    {
        char* body = NULL;
        msgrcv(m_MessageID, &message, sizeof(MESSAGE_HEADER), 1, 0);
        msgrcv(m_MessageID, body, message.size, 1, 0);
    }

private:
    SOCKET_NAME m_SockeName;
    struct sockaddr_un m_Address;

    void CreateQueue()
    {
        key_t key;

        // Probably should make subfolder for queue
        // ftok to generate unique key
        key = ftok(m_SockeName, 65);

        // msgget creates a message queue
        // and returns identifier
        m_MessageID = msgget(key, 0666 | IPC_CREAT);
    }

    void DestroyQueue()
    {
        msgctl(m_MessageID, IPC_RMID, NULL);
    }
};

#endif