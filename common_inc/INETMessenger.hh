#ifndef INETMESSENGER__HH
#define INETMESSENGER__HH
#include <retcode.hh>
#include <DOFRI.hh>
#include <DaemonThread.hh>

#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <queue>
#include <signal.h>

constexpr int _SERVER_VERSION = 31;

struct INET_HEADER
{
    int message_type;
    size_t message_size;
};

struct INET_PACKAGE
{
    INET_HEADER header; // pointer to header can proxy as pointer to whole INET_PACKAGE
    void* payload;
};

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

enum ACKNOWLEDGE_TYPE
{
    SERVER_TO_CLIENT = 1,
    CLIENT_TO_SERVER = 2,
};

enum CONNECTION_REASON
{
    RECEIVE = 1, // Request receiving data
    SEND = 2 // Request sending data
};

struct ACKNOWLEDGE
{
    size_t server_version;
    ACKNOWLEDGE_TYPE ack_type;
    CONNECTION_REASON con_reason;
};

struct CONNECTION
{
    int socket;
    char address[INET6_ADDRSTRLEN];
};

static RETCODE ReceiveAck(int socket, ACKNOWLEDGE& acknowledge)
{
    int bytes_received =
        recv(socket, static_cast<void*>(&acknowledge), sizeof(acknowledge), 0);

    if( sizeof(acknowledge) != bytes_received ||
        _SERVER_VERSION != acknowledge.server_version)
    {
        std::cout << "Failed to receive ack from socket: " << socket << "\n";
        return RTN_CONNECTION_FAIL;
    }

    std::cout << "Received ack from socket: " << socket << "\n";

    return RTN_OK;
}

static RETCODE SendAck(int socket, ACKNOWLEDGE& acknowledge)
{
    if(-1 == send(socket, static_cast<void*>(&acknowledge), sizeof(acknowledge), 0))
    {
        std::cout << "Failed to send ack to socket: " << socket << "\n";
        return RTN_CONNECTION_FAIL;
    }

    std::cout << "Sent ack to socket: " << socket << "\n";

    return RTN_OK;
}

// A stopable daemon for accepting client connections
// @TODO: Figure out how to pass queue reference rather than pointer
class AcceptThread: public DaemonThread<int, std::queue<CONNECTION>*>
{

public:
    // Function to be executed by thread function
    void execute(int listening_socket, std::queue<CONNECTION>* client_connections)
    {
        RETCODE retcode = RTN_OK;
        struct sockaddr incoming_accepted_address;
        socklen_t incoming_address_size = sizeof(incoming_accepted_address);
        int accept_socket = -1;
        char accepted_address[INET6_ADDRSTRLEN];

        std::cout << "Entered acceptance thread with socket: "
                  << listening_socket
                  << " \n";

        // Need to non-block this so we can check for daemon stop
        fcntl(listening_socket, F_SETFL, O_NONBLOCK);

        CONNECTION connection = {0};
        ACKNOWLEDGE acknowledge = {0};

        while (StopRequested() == false)
        {
            accept_socket = accept(listening_socket,
                            (struct sockaddr *)&incoming_accepted_address,
                            &incoming_address_size);

            if(0 < accept_socket)
            {
                inet_ntop(incoming_accepted_address.sa_family,
                get_in_addr((struct sockaddr *)&incoming_accepted_address),
                    accepted_address, sizeof(accepted_address));


                std::cout << "Got connection with IP: "
                          << accepted_address
                          << " on socket: "
                          << accept_socket
                          <<"\n";

                // Wait for client to send ack
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                retcode = ReceiveAck(accept_socket, acknowledge);

                if(RTN_OK == retcode && _SERVER_VERSION == acknowledge.server_version)
                {
                    std::cout << "Client: "
                              << accepted_address
                              << " on socket: "
                              << accept_socket
                              << " accepted acknowledge!\n";

                    connection.socket = accept_socket;
                    memcpy(connection.address, accepted_address, sizeof(connection.address));
                    client_connections->push(connection);
                }
                else
                {
                    close(accept_socket);
                    std::cout << "Client: "
                              << accepted_address
                              << "on socket: "
                              << accept_socket
                              << "failed acknowledge!\nClosing...\n";
                }
            }
            else
            {
                // Nothing here so we slumber for a second (literally)
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        std::cout << "Stopped acceptance thread!\n";

    }

};

class INETMessenger
{

public:
    INETMessenger(const std::string& portNumber = "")

        : m_Ready(false), m_CanAccept(false), m_IsAccepting(false),
          m_AcceptingPort(portNumber), m_AcceptingAddress(),  m_AcceptTask(),
          m_ListeningSocket(-1),
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
            std::cout << "\nSending to: " << connection.address << " on port: " << connection.socket << "\n";
            retcode |= Send(connection.socket, message.c_str(), message.length());
        }

        return retcode;
    }

    RETCODE SendToAll(char* buffer, size_t buffer_size)
    {
        RETCODE retcode = RTN_OK;
        for(CONNECTION& connection : m_Connections)
        {
            retcode |= Send(connection.socket, buffer, buffer_size);
        }

        return retcode;
    }

    RETCODE Receive(char buffer[], int buffer_length)
    {
        return Receive(m_Connections[0].socket, buffer, buffer_length);
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

            m_AcceptTask.Start(m_ListeningSocket, &m_ClientQueue);

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
            fcntl(accept_connection.socket, F_SETFL, O_NONBLOCK);
            m_Connections.push_back(accept_connection);
            std::cout << "New socket ready: " << accept_connection.socket << "\n";
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
            close(connection.socket);
            std::cout << "Closed address: "<< connection.address << " socket: " << connection.socket << "\n";
        }

        m_Connections.clear();

        return retcode;
    }

    RETCODE StopListeningForAccepts()
    {
        if(m_IsAccepting)
        {
            m_IsAccepting = false;
            m_AcceptTask.Stop();
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
        ACKNOWLEDGE ack = {_SERVER_VERSION};

        // Send our server version to server to match
        RETCODE retcode = SendAck(connectedSocket, ack);
        if(RTN_OK == retcode)
        {
            connection.socket = connectedSocket;
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
        return m_Connections[index].socket;
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
    AcceptThread m_AcceptTask;
    int m_ListeningSocket;
    std::queue<CONNECTION> m_ClientQueue;
public:
    std::vector<CONNECTION> m_Connections;
};

#endif