#include <Messenger.hh>
#include <sys/ipc.h>
#include <sys/msg.h>

RETCODE Messenger::Send(const SOCKET_NAME& name, const MESSAGE_HEADER& message)
{
    key_t key;
    int msgid;

    // ftok to generate unique key
    key = ftok(name, 65);

    // msgget creates a message queue
    // and returns identifier
    msgid = msgget(key, 0666 | IPC_CREAT);

    msgsnd(msgid, &message, sizeof(MESSAGE_HEADER) + sizeof(message.size), 0);

    return RTN_OK;
}