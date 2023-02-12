#ifndef INETMESSENGER__HH
#define INETMESSENGER__HH

// Update version for production
constexpr int _SERVER_VERSION = 1;

#include <retcode.hh>
#include <OFRI.hh>
#include <DaemonThread.hh>
#include <TasQ.hh>
#include <Hook.hh>
#include "../Profiler/inc/profiler.hh"

#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <queue>
#include <signal.h>
#include <sys/mman.h>
#include <sstream>

struct CONNECTION
{
    int port;
    char address[INET6_ADDRSTRLEN];

    bool operator == (const CONNECTION& other) const
    {
        return !strncmp(address, other.address, INET6_ADDRSTRLEN) &&
                port == other.port;
    }
};

namespace std
{
    template<>
    struct hash<CONNECTION>
    {
        size_t operator() (const CONNECTION& key) const
        {
            return std::hash<std::string>()(key.address) << 1 ^ key.port;
        }
    };
}

struct INET_HEADER
{
    CONNECTION connection;
    size_t message_size;
    size_t data_type;
};

struct INET_PACKAGE
{
    INET_HEADER header;
    char payload[0];
};

struct ACKNOWLEDGE
{
    size_t server_version;
};

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


static RETCODE ReceiveAck(int socket, ACKNOWLEDGE& acknowledge)
{
    // Timeout at 1 second to wait for recv
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    int bytes_received =
        recv(socket, static_cast<void*>(&acknowledge), sizeof(acknowledge), 0);

    // Reset
    tv.tv_sec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    if( sizeof(acknowledge) != bytes_received ||
        _SERVER_VERSION != acknowledge.server_version)
    {
        return RTN_CONNECTION_FAIL;
    }

    return RTN_OK;
}

static RETCODE SendAck(int socket, ACKNOWLEDGE& acknowledge)
{
    if(-1 == send(socket, static_cast<void*>(&acknowledge), sizeof(acknowledge), 0))
    {
        return RTN_CONNECTION_FAIL;
    }

    return RTN_OK;
}

typedef void (*ConnectDelegate)(const CONNECTION&);
typedef void (*DisconnectDelegate)(const CONNECTION&);
typedef void (*MessageDelegate)(const INET_PACKAGE*);
typedef void (*StopDelegate)(void);

// @TODO: Figure out how to pass queue reference rather than pointer
class PollThread: public DaemonThread<int>
{

public:

    RETCODE SendAll(INET_PACKAGE* package)
    {
        PROFILE_FUNCTION();
        for(std::unordered_map<CONNECTION,int>::iterator iter = m_ConnectionMap.begin(); iter != m_ConnectionMap.end(); ++iter)
        {
            INET_PACKAGE* message = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_PACKAGE) + package->header.message_size]);
            message->header.message_size = package->header.message_size;
            message->header.connection = iter->first;
            memcpy(message->payload, package->payload, message->header.message_size);
            m_SendQueue.Push(message);
        }

        return RTN_OK;
    }

    // Use your own memory
    RETCODE Send(INET_PACKAGE* package)
    {
        PROFILE_FUNCTION();
        // Wait until send goes through
        m_SendQueue.Push(package);
        return RTN_OK;
    }

    // Send packed data -- structs without other references
    template<class DATA>
    RETCODE Send(const DATA& data, size_t connection)
    {
        PROFILE_FUNCTION();
        INET_PACKAGE* package = new char[sizeof(DATA) + sizeof(INET_PACKAGE)];
        package->header.message_size = sizeof(DATA);
        package->header.connection = connection;
        memcpy(&(package->payload[0]), *data, sizeof(DATA));
        m_SendQueue.Push(package);
        return RTN_OK;
    }

    RETCODE Receive(INET_PACKAGE* message)
    {
        PROFILE_FUNCTION();
        // Try to get a value
        return m_ReceiveQueue.TryPop(message) ? RTN_OK : RTN_NOT_FOUND;
    }

    void execute(int dummy = 0)
    {
        PROFILE_FUNCTION();
        RETCODE retcode = RTN_OK;
        int num_poll_events = 0;
        int err = 0;
        int ret = 0;
        int timeout = 10; /* in milliseconds */
        int maxevents = 64;
        struct epoll_event events[64];

        while(StopRequested() == false)
        {
            retcode = HandleSends();
            /*
            * I sleep on `epoll_wait` and the kernel will wake me up
            * when event comes to my monitored file descriptors, or
            * when the timeout reached.
            */
            num_poll_events = epoll_wait(m_PollFD, events, maxevents, timeout);


            if (num_poll_events == 0)
            {
                /*
                *`epoll_wait` reached its timeout
                */
                //printf("I don't see any event within %d milliseconds\n", timeout);
                continue;
            }


            if (num_poll_events == -1)
            {
                err = errno;
                if (err == EINTR)
                {
                    std::cout << "Interrupted during epoll!\n";
                    continue;
                }

                /* Error */
                ret = -1;
                std::cout << "Error in epoll wait: " << strerror(err) << "\n";
                break;
            }


            for (int i = 0; i < num_poll_events; i++)
            {
                int fd = events[i].data.fd;

                if (m_TCPSocket == fd)
                {
                    /*
                    * A new client is connecting to us...
                    */
                    if (RTN_OK != AcceptNewClient())
                    {
                        std::cout << "Failed to accept client socket: " << fd << "\n";
                    }
                    continue;
                }


                /*
                * We have event(s) from client, let's call `recv()` to read it.
                */
                CONNECTION connection = m_FDMap[fd];
                retcode = HandleEvent(connection, fd, events[i].events);
                if(RTN_CONNECTION_FAIL == retcode)
                {
                    // Send signal that it's not available for sending??
                }


                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }


        }

        std::cout << "Stopped polling thread\n";

    }

    PollThread(const std::string& portNumber = "") :
        m_Ready(false), m_PollFD(-1), m_TCPSocket(-1), m_Port(portNumber),
        m_Address(), m_SendQueue(), m_ReceiveQueue()
    {
        PROFILE_FUNCTION();
        RETCODE retcode = GetConnectionForSelf();
        retcode |= InitPoll();
        retcode |= AddFDToPoll(m_TCPSocket, EPOLLIN | EPOLLPRI);
        if(RTN_OK == retcode)
        {
            // Ignore broken pipe signal to prevent send/read from causing errors
            signal(SIGPIPE, SIG_IGN);

            Start(0);

            m_Ready = true;
        }

    }

    RETCODE GetConnectionForSelf(void)
    {
        PROFILE_FUNCTION();
        struct addrinfo hints = {0};
        struct addrinfo *returnedAddrInfo = nullptr;
        struct addrinfo *currentAddrInfo = nullptr;
        int getInfoStatus = 0;
        int yes = 1;
        // Set how we want the results to come as
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC; // IPV4 or IPV6
        hints.ai_socktype = SOCK_STREAM; // slower, yet reliable
        hints.ai_flags = AI_PASSIVE; // fill in IP for me

        // Get address for self
        if((getInfoStatus = getaddrinfo(nullptr, m_Port.c_str(), &hints, &returnedAddrInfo)) != 0)
        {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getInfoStatus));
            return RTN_NOT_FOUND;
        }
        else
        {
            char accepted_address[INET6_ADDRSTRLEN];

            // Get llist of connections
            inet_ntop(returnedAddrInfo->ai_addr->sa_family,
                      get_in_addr((struct sockaddr *)&returnedAddrInfo),
                      accepted_address,
                      sizeof(accepted_address));

            m_Address = accepted_address;
        }

        // Find connection address for us
        for(currentAddrInfo = returnedAddrInfo; currentAddrInfo != NULL; currentAddrInfo = currentAddrInfo->ai_next)
        {

            if ((m_TCPSocket = socket(currentAddrInfo->ai_family, currentAddrInfo->ai_socktype,
                    currentAddrInfo->ai_protocol)) == -1)
            {
                perror("server: socket");
                continue;
            }

            if (setsockopt(m_TCPSocket, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1)
            {
                return RTN_CONNECTION_FAIL;
            }


            // We got one, so bind!
            if (bind(m_TCPSocket, currentAddrInfo->ai_addr, currentAddrInfo->ai_addrlen) == -1)
            {
                close(m_TCPSocket);
                perror("server: bind");
                continue;
            }

            break;
        }

        // Start listening for connections
        if(-1 == listen(m_TCPSocket, 10))
        {
            return RTN_CONNECTION_FAIL;
        }

        // Cleanup
        freeaddrinfo(returnedAddrInfo);
        if(nullptr == currentAddrInfo)
        {
            return RTN_MALLOC_FAIL;
        }

        return RTN_OK;
    }


    RETCODE Connect(const std::string& address, const std::string& port)
    {
        PROFILE_FUNCTION();
        struct addrinfo hints = {0};
        struct addrinfo *returnedAddrInfo = nullptr;
        struct addrinfo *currentAddrInfo = nullptr;
        int rv = -1;
        int connectedSocket = -1;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((rv = getaddrinfo(address.c_str(), port.c_str(), &hints, &returnedAddrInfo)) != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return RTN_NOT_FOUND;
        }

        // loop through all the results and connect to the first good one
        for(currentAddrInfo = returnedAddrInfo;
            currentAddrInfo != NULL;
            currentAddrInfo = currentAddrInfo->ai_next)
        {
            if ((connectedSocket =
                 socket(currentAddrInfo->ai_family,
                 currentAddrInfo->ai_socktype,
                 currentAddrInfo->ai_protocol)) == -1)
            {
                perror("client: socket");
                continue;
            }

            // Send initial connection
            if (connect(connectedSocket,
                        currentAddrInfo->ai_addr,
                        currentAddrInfo->ai_addrlen) == -1)
            {
                close(connectedSocket);
                perror("client: connect");
                continue;
            }

            break;
        }

        if (currentAddrInfo == NULL)
        {
            return RTN_CONNECTION_FAIL;
        }

        char accepted_address[INET6_ADDRSTRLEN];

        inet_ntop(returnedAddrInfo->ai_addr->sa_family,
                    get_in_addr((struct sockaddr *)&returnedAddrInfo),
                    accepted_address,
                    sizeof(accepted_address));

        freeaddrinfo(returnedAddrInfo);

        // We must send CLIENT_TO_SERVER, but we make requests on what type
        ACKNOWLEDGE ack = {_SERVER_VERSION};
        CONNECTION conn = {0, '\0'};

        // Send our server version to server to match
        RETCODE retcode = SendAck(connectedSocket, ack);
        if(RTN_OK == retcode)
        {
            // Non-block set for smooth receives and sends
            if(fcntl(connectedSocket, F_SETFL, fcntl(connectedSocket, F_GETFL) | O_NONBLOCK) < 0)
            {
                return RTN_FAIL;
            }

            memcpy(conn.address, accepted_address, sizeof(conn.address));
            std::stringstream portstream(port);
            portstream >> conn.port;
            AddConnection(connectedSocket, conn, EPOLLIN | EPOLLPRI);
            m_ConnectionMap[conn] = connectedSocket;
            m_FDMap[connectedSocket] = conn;
        }
        else
        {
            std::cout
                << "Failed sending acknowledgement to server: "
                << address
                << " on port: "
                << port
                << "\n";
        }

        return retcode;
    }

    RETCODE StopListeningForAccepts()
    {
        PROFILE_FUNCTION();
        if(m_Ready)
        {
            m_Ready = false;
            Stop();
            close(m_TCPSocket);
            return RTN_OK;
        }

        return RTN_OK;
    }

    RETCODE HandleEvent(const CONNECTION& connection, int fd, uint32_t revents)
    {
        PROFILE_FUNCTION();
        RETCODE retcode = RTN_OK;
        int err;
        ssize_t recv_ret;
        INET_HEADER inet_header = {};
        char* payload = nullptr;
        const uint32_t err_mask = EPOLLERR | EPOLLHUP;

        // Need to get connection that matches fd to call disconnect delegate
        if (revents & err_mask)
        {
            retcode = RemoveConnection(fd, connection);
            return RTN_CONNECTION_FAIL;
        }

        recv_ret = recv(fd, &(inet_header), sizeof(INET_HEADER), 0);
        if (recv_ret == 0)
        {
            RemoveConnection(fd, connection);
            return RTN_CONNECTION_FAIL;
        }

        if (recv_ret < 0)
        {
            err = errno;
            if (err == EAGAIN)
            {
                return RTN_OK;
            }

            /* Error */
            std::cout << "Error receving data from socket: " << fd << "\n";
            RemoveConnection(fd, connection);
            return RTN_CONNECTION_FAIL;
        }

        int remaining_message = inet_header.message_size;
        INET_PACKAGE* package = reinterpret_cast<INET_PACKAGE*>(new char[sizeof(INET_PACKAGE) + remaining_message]);
        memcpy(&(package->header), &inet_header, sizeof(INET_HEADER));

        while(0 < (recv_ret = recv(fd, package->payload + inet_header.message_size - remaining_message, remaining_message, 0)))
        {
            remaining_message -= recv_ret;
        }

        if (recv_ret < 0)
        {
            err = errno;
            if (err != EAGAIN)
            {
                /* Error */
                std::cout << "Error receving data from socket: " << fd << "\n";
                RemoveConnection(fd, connection);
                return RTN_CONNECTION_FAIL;
            }

        }


        m_OnReceive.Invoke(package);
        delete[] package;
        return RTN_OK;
    }

    RETCODE HandleSends(void)
    {
        PROFILE_FUNCTION();
        int message_length = 0;
        int socket;
        INET_PACKAGE* packet;

        while(m_SendQueue.TryPop(packet))
        {
            std::unordered_map<CONNECTION,int>::iterator connection = m_ConnectionMap.find(packet->header.connection);
            if(connection != m_ConnectionMap.end())
            {
                message_length = packet->header.message_size + sizeof(INET_PACKAGE);
                while(message_length > 0)
                {
                    message_length -= send(connection->second, packet, message_length, 0);
                }
            }
            else
            {
                std::cout << "Could not find: " << packet->header.connection.address << "sending failed!\n";
            }

            delete packet;
        }

        return RTN_OK;
    }

    RETCODE AcceptNewClient()
    {
        PROFILE_FUNCTION();
        RETCODE retcode = RTN_OK;
        struct sockaddr incoming_accepted_address;
        socklen_t incoming_address_size = sizeof(incoming_accepted_address);
        int accept_socket = -1;
        char accepted_address[INET6_ADDRSTRLEN];
        int err = 0;

        CONNECTION connection = {0};
        ACKNOWLEDGE acknowledge = {0};

        accept_socket = accept(m_TCPSocket,
                            (struct sockaddr *)&incoming_accepted_address,
                            &incoming_address_size);

        if(0 < accept_socket)
        {
            inet_ntop(incoming_accepted_address.sa_family,
            get_in_addr((struct sockaddr *)&incoming_accepted_address),
                accepted_address, sizeof(accepted_address));

            // Wait for client to send ack
            retcode = ReceiveAck(accept_socket, acknowledge);

            if(RTN_OK == retcode && _SERVER_VERSION == acknowledge.server_version)
            {
                memcpy(connection.address, accepted_address, sizeof(connection.address));
                std::stringstream portstream(m_Port);
                portstream >> connection.port;

                // Non-block set for smooth receives and sends
                if(fcntl(accept_socket, F_SETFL, fcntl(accept_socket, F_GETFL) | O_NONBLOCK) < 0)
                {
                    return RTN_FAIL;
                }

                AddConnection(accept_socket, connection, EPOLLIN | EPOLLPRI);

                return RTN_OK;
            }
            else
            {
                close(accept_socket);
                m_OnDisconnect.Invoke(connection);
                return RTN_CONNECTION_FAIL;
            }
        }
        else
        {
            err = errno;
            if (err == EAGAIN)
            {
                return RTN_OK;
            }

            /* Error */
            std::cout << "Error in accept(): " << strerror(err) << "\n";
            return RTN_FAIL;
        }

    }

    RETCODE AddFDToPoll(int fd, uint32_t events)
    {
        PROFILE_FUNCTION();
        int err;
        struct epoll_event event;

        /* Shut the valgrind up! */
        memset(&event, 0, sizeof(struct epoll_event));

        event.events = events;
        event.data.fd = fd;
        if (0 > epoll_ctl(m_PollFD, EPOLL_CTL_ADD, fd, &event))
        {
            err = errno;
            std::cout
                << "Failed to add socket: "
                << fd
                << " to polling with error: "
                << strerror(err)
                << "\n";
            return RTN_FAIL;
        }

        return RTN_OK;
    }


    RETCODE AddConnection(int fd, const CONNECTION& connection, uint32_t events)
    {
        RETCODE retcode = AddFDToPoll(fd, events);

        m_ConnectionMap[connection] = fd;
        m_FDMap[fd] = connection;

        m_OnClientConnect.Invoke(connection);

        return retcode;
    }

    RETCODE RemoveConnection(int fd, const CONNECTION& connection)
    {
        RETCODE retcode = RemoveFDFromPoll(fd);
        m_FDMap.erase(fd);
        m_ConnectionMap.erase(connection);
        m_OnDisconnect.Invoke(connection);

        return retcode;
    }

    RETCODE RemoveFDFromPoll(int fd)
    {
        PROFILE_FUNCTION();
        int err;

        if (0 > epoll_ctl(m_PollFD, EPOLL_CTL_DEL, fd, NULL))
        {
            err = errno;
            std::cout
                << "Failed to remove socket: "
                << fd
                << " from polling with error: "
                << strerror(err)
                << "\n";
            return RTN_FAIL;
        }
        else
        {
            if(close(fd))
            {
                err = errno;
                std::cout
                    << "Failed to close socket: "
                    << fd
                    << " from polling with error: "
                    << strerror(err)
                    << "\n";
            }

        }

        return RTN_OK;
    }

    RETCODE InitPoll()
    {
        PROFILE_FUNCTION();
        int err;

        /* The epoll_create argument is ignored on modern Linux */
        m_PollFD = epoll_create(255);
        if (m_PollFD < 0)
        {
            err = errno;
            std::cout << "Error creating epoll: " << strerror(err) << " \n";
            return RTN_FAIL;
        }

        return RTN_OK;
    }

    RETCODE StopPoll()
    {
        RETCODE retcode = RTN_OK;
        m_SendQueue.done();
        m_ReceiveQueue.done();
        Stop();

        for(std::unordered_map<CONNECTION,int>::iterator iter = m_ConnectionMap.begin(); iter != m_ConnectionMap.end(); ++iter)
        {
            retcode |= RemoveConnection(iter->second, iter->first);
        }

        retcode |= RemoveFDFromPoll(m_TCPSocket);
        m_ConnectionMap.clear();
        m_FDMap.clear();

        m_OnStop.Invoke();

        return retcode;

    }

    std::string GetTCPAddress()
    {
        return m_Address;
    }

    std::string GetTCPPort()
    {
        return m_Port;
    }

    int GetTCPSocket()
    {
        return m_TCPSocket;
    }

    bool m_Ready;
    int m_PollFD;
    int m_TCPSocket;
    std::string m_Port;
    std::string m_Address;
    TasQ<INET_PACKAGE*> m_SendQueue;
    TasQ<INET_PACKAGE*> m_ReceiveQueue;
    std::unordered_map<CONNECTION, int> m_ConnectionMap;
    std::unordered_map<int, CONNECTION> m_FDMap;
    Hook<ConnectDelegate> m_OnClientConnect;
    Hook<ConnectDelegate> m_OnServerConnect;
    Hook<DisconnectDelegate> m_OnDisconnect;
    Hook<MessageDelegate> m_OnReceive;
    Hook<StopDelegate> m_OnStop;
};


#endif