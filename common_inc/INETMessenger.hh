#ifndef INETMESSENGER__HH
#define INETMESSENGER__HH

constexpr int _SERVER_VERSION = 1;

#include <retcode.hh>
#include <DOFRI.hh>
#include <DaemonThread.hh>
#include <TasQ.hh>

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
#include <Hook.hh>

struct CONNECTION
{
    int port;
    char address[INET6_ADDRSTRLEN];
    int socket;

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
    CONNECTION connection_token;
    size_t message_size;
};

struct INET_PACKAGE
{
    INET_HEADER header; // pointer to header can proxy as pointer to whole INET_PACKAGE
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
    // Timeout at 1 second
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
        //std::cout << "Failed to receive ack from socket: " << socket << "\n";
        return RTN_CONNECTION_FAIL;
    }

    //std::cout << "Received ack from socket: " << socket << "\n";

    return RTN_OK;
}

static RETCODE SendAck(int socket, ACKNOWLEDGE& acknowledge)
{
    if(-1 == send(socket, static_cast<void*>(&acknowledge), sizeof(acknowledge), 0))
    {
        //std::cout << "Failed to send ack to socket: " << socket << "\n";
        return RTN_CONNECTION_FAIL;
    }

    //std::cout << "Sent ack to socket: " << socket << "\n";

    return RTN_OK;
}

   typedef void (*ConnectDelegate)(const CONNECTION&);
   typedef void (*DisconnectDelegate)(const CONNECTION&);
   typedef void (*MessageDelegate)(const CONNECTION&, const char*);

// @TODO: Figure out how to pass queue reference rather than pointer
class PollThread: public DaemonThread<int>
{

public:

    // Use your own memory
    RETCODE Send(INET_PACKAGE* package)
    {
        // Wait until send goes through
        m_SendQueue.Push(package);
        return RTN_OK;
    }

    // Send packed data -- structs without other references
    template<class DATA>
    RETCODE Send(const DATA& data, size_t connection_token)
    {
        INET_PACKAGE* package = new char[sizeof(DATA) + sizeof(INET_PACKAGE)];
        package->header.message_size = sizeof(DATA);
        package->header.connection_token = connection_token;
        memcpy(&(package->payload[0]), *data, sizeof(DATA));
        m_SendQueue.Push(package);
        return RTN_OK;
    }

    RETCODE Receive(INET_PACKAGE* message)
    {
        // Try to get a value
        return m_ReceiveQueue.TryPop(message) ? RTN_OK : RTN_NOT_FOUND;
    }

    void execute(int dummy = 0)
    {
        RETCODE retcode = RTN_OK;
        int num_poll_events = 0;
        int err = 0;
        int ret = 0;
        int timeout = 3000; /* in milliseconds */
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
        RETCODE retcode = GetConnectionForSelf();
        retcode |= InitPoll();
        retcode |= AddFDToPoll(m_TCPSocket, EPOLLIN | EPOLLPRI);
        if(RTN_OK == retcode)
        {
            // Ignore broken pipe signal to prevent send/read from causing errors
            sigignore(SIGPIPE);

            Start(0);

            m_Ready = true;
        }

    }

    RETCODE GetConnectionForSelf(void)
    {
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
            memcpy(conn.address, accepted_address, sizeof(conn.address));
            AddFDToPoll(connectedSocket, EPOLLIN | EPOLLPRI);
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
        RETCODE retcode = RTN_OK;
        int err;
        ssize_t recv_ret;
        INET_HEADER inet_header = {};
        char* payload = nullptr;
        const uint32_t err_mask = EPOLLERR | EPOLLHUP;

        // Need to get connection that matches fd to call disconnect delegate
        if (revents & err_mask)
        {
            retcode = RemoveFDFromPoll(fd);
            m_FDMap.erase(fd);
            m_ConnectionMap.erase(connection);
            m_OnDisconnect.Invoke(connection);
            return RTN_CONNECTION_FAIL;
        }

        recv_ret = recv(fd, &(inet_header), sizeof(INET_HEADER), 0);
        if (recv_ret == 0)
        {
            RemoveFDFromPoll(fd);
            m_FDMap.erase(fd);
            m_ConnectionMap.erase(connection);
            m_OnDisconnect.Invoke(connection);
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
            RemoveFDFromPoll(fd);
            m_FDMap.erase(fd);
            m_ConnectionMap.erase(connection);
            m_OnDisconnect.Invoke(connection);
            return RTN_CONNECTION_FAIL;
        }

        int remaining_message = inet_header.message_size;
        payload = new char[remaining_message];

        while(0 < (recv_ret = recv(fd, (payload + inet_header.message_size - remaining_message), remaining_message, 0)))
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
                RemoveFDFromPoll(fd);
                m_FDMap.erase(fd);
                m_ConnectionMap.erase(connection);
                m_OnDisconnect.Invoke(connection);
                return RTN_CONNECTION_FAIL;
            }

        }


        m_OnReceive.Invoke(connection, payload);
        delete[] payload;
        return RTN_OK;
    }

    RETCODE HandleSends(void)
    {
        int message_length = 0;
        int socket;
        INET_PACKAGE* packet;

        while(m_SendQueue.TryPop(packet))
        {
            std::unordered_map<CONNECTION,int>::iterator connection = m_ConnectionMap.find(packet->header.connection_token);
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
                std::cout << "Could not find: " << packet->header.connection_token.address << "sending failed!\n";
            }

            delete packet;
        }

        return RTN_OK;
    }

    RETCODE AcceptNewClient()
    {
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

                m_ConnectionMap[connection] = accept_socket;
                m_FDMap[accept_socket] = connection;
                AddFDToPoll(accept_socket, EPOLLIN | EPOLLPRI);
                m_OnClientConnect.Invoke(connection);
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

    RETCODE RemoveFDFromPoll(int fd)
    {
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
        int err;

        /* The epoll_create argument is ignored on modern Linux */
        m_PollFD = epoll_create(255);
        if (m_PollFD < 0) {
            err = errno;
            std::cout << "Error creating epoll: " << strerror(err) << " \n";
            return RTN_FAIL;
        }

        return RTN_OK;
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
};

class INETMessenger
{

public:

    INETMessenger(const std::string& portNumber = "")

        : m_Ready(false), m_CanAccept(false), m_IsAccepting(false),
          m_AcceptingPort(portNumber), m_AcceptingAddress(),
          /*m_PollThread(),*/ m_ListeningSocket(-1),
          m_ClientQueue(), m_Connections()
    {
        SetSignalHandlers(); // maybe..

        if(m_AcceptingPort.empty())
        {
            // We only want to receive and that's ok
            m_CanAccept = false;
            return;
        }

        GetConnectionForSelf();
    }

    ~INETMessenger()
    {

        StopListeningForAccepts();
    }

    RETCODE Send(int socket, const char* message, size_t message_length)
    {
        while(message_length > 0)
        {
            message_length -= send(socket, message, message_length, 0);
        }

        return RTN_OK;
    }

    RETCODE SendToAll(const std::string& message)
    {
        RETCODE retcode = RTN_OK;
        for(CONNECTION& connection : m_Connections)
        {
            //std::cout << "\nSending to: " << connection.address << " on port: " << connection.socket << "\n";
            //retcode |= Send(connection.socket, message.c_str(), message.length());
        }

        return retcode;
    }

    RETCODE SendToAll(char* buffer, size_t buffer_size)
    {
        RETCODE retcode = RTN_OK;
        for(CONNECTION& connection : m_Connections)
        {
            //retcode |= Send(connection.socket, buffer, buffer_size);
        }

        return retcode;
    }

    RETCODE Receive(char buffer[], int buffer_length)
    {
        //return Receive(m_Connections[0].socket, buffer, buffer_length);
        return RTN_OK;
    }

    RETCODE Receive(int socket, char buffer[], int buffer_length)
    {
        int bytes_received = recv(socket, buffer, buffer_length, 0);

        if(0 == bytes_received)
        {
            return RTN_CONNECTION_FAIL;
        }
        else if(-1 == bytes_received)
        {
            return RTN_FAIL;
        }

        return RTN_OK;
    }

    RETCODE Listen(int listenQueueSize = 10)
    {
        if(m_CanAccept)
        {
            // Start listening for connections
            if(-1 == listen(m_ListeningSocket, listenQueueSize))
            {
                return RTN_CONNECTION_FAIL;
            }

            //m_PollThread.Start();

            m_IsAccepting = true;

            return RTN_OK;
        }

        return RTN_FAIL;
    }



    RETCODE GetAcceptedConnections()
    {
        RETCODE retcode = RTN_OK;
        while(!m_ClientQueue.empty())
        {
            CONNECTION& accept_connection = m_ClientQueue.front();
            //fcntl(accept_connection.socket, F_SETFL, O_NONBLOCK);
            m_Connections.push_back(accept_connection);
            //std::cout << "New socket ready: " << accept_connection.socket << "\n";
            m_ClientQueue.pop();

        }

        return retcode;
    }

    RETCODE CloseAllConnections()
    {
        // Get every connection waiting to be accepted
        RETCODE retcode = GetAcceptedConnections();

        // Close them all
        for(CONNECTION& connection : m_Connections)
        {
            //close(connection.socket);
            //std::cout << "Closed address: "<< connection.address << " socket: " << connection.socket << "\n";
        }

        m_Connections.clear();

        return retcode;
    }

    RETCODE StopListeningForAccepts()
    {
        if(m_IsAccepting)
        {
            m_IsAccepting = false;
            //m_PollThread.Stop();
            close(m_ListeningSocket);
            // Maybe make closing everything optional..
            return CloseAllConnections();
        }

        return RTN_OK;
    }

    RETCODE GetConnectionForSelf(void)
    {
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
        if((getInfoStatus = getaddrinfo(nullptr, m_AcceptingPort.c_str(), &hints, &returnedAddrInfo)) != 0)
        {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(getInfoStatus));
            m_CanAccept = false;
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

            m_AcceptingAddress = accepted_address;
        }

        // Find connection address for us
        for(currentAddrInfo = returnedAddrInfo; currentAddrInfo != NULL; currentAddrInfo = currentAddrInfo->ai_next)
        {

            if ((m_ListeningSocket = socket(currentAddrInfo->ai_family, currentAddrInfo->ai_socktype,
                    currentAddrInfo->ai_protocol)) == -1)
            {
                perror("server: socket");
                continue;
            }

            if (setsockopt(m_ListeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1)
            {
                m_CanAccept = false;
                return RTN_CONNECTION_FAIL;
            }


            // We got one, so bind!
            if (bind(m_ListeningSocket, currentAddrInfo->ai_addr, currentAddrInfo->ai_addrlen) == -1)
            {
                close(m_ListeningSocket);
                perror("server: bind");
                continue;
            }

            break;
        }

        // Cleanup
        freeaddrinfo(returnedAddrInfo);
        if(nullptr == currentAddrInfo)
        {
            m_CanAccept = false;
            return RTN_MALLOC_FAIL;
        }

        // Ignore broken pipe signal to prevent send/read from causing errors
        sigignore(SIGPIPE);

        m_CanAccept = true;

        return RTN_OK;
    }

    RETCODE Connect(const std::string& address, const std::string& port)
    {
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

        CONNECTION connection{0, '\0'};
        // We must send CLIENT_TO_SERVER, but we make requests on what type
        //ACKNOWLEDGE ack = {_SERVER_VERSION, CLIENT_TO_SERVER, SEND | RECEIVE};

        // Send our server version to server to match
        RETCODE retcode = RTN_OK;//SendAck(connectedSocket, ack);
        if(RTN_OK == retcode)
        {
            //connection.socket = connectedSocket;
            memcpy(connection.address, accepted_address, sizeof(connection.address));
            m_Connections.push_back(connection);
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

    inline std::string GetAddress(void)
    {
        if(m_CanAccept)
        {
            return m_AcceptingAddress;
        }

        return "NO ACCEPT ADDRESS";
    }

    inline std::string GetPort(void)
    {
        if(m_CanAccept)
        {
            return m_AcceptingPort;
        }

        return "NO ACCEPT PORT";
    }

    inline std::string GetConnectedAddress(size_t index = 0)
    {
        if(index < m_Connections.size())
        {
            return m_Connections[index].address;
        }

        return "NO CONNECT ADDRESS";
    }

    inline int GetConnectionSocket(size_t index = 0)
    {
        return 0;
    }

    inline bool IsListening(void)
    {
        return m_IsAccepting;
    }

private:

    static void HandleSignal(int sig)
    {
        std::cout << "\nHandled signal: " << sig << "\n";
        exit(1);
    }

    void SetSignalHandlers(void)
    {
        signal(SIGINT, HandleSignal);
        signal(SIGQUIT, HandleSignal);
    }

    bool m_Ready;
    bool m_CanAccept;
    bool m_IsAccepting;
    std::string m_AcceptingPort;
    std::string m_AcceptingAddress;
    //PollThread m_PollThread;
    int m_ListeningSocket;
    int m_PollFD;
    std::queue<CONNECTION> m_ClientQueue;
public:
    std::vector<CONNECTION> m_Connections;
};



#endif