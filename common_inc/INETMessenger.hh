#ifndef INETMESSENGER__HH
#define INETMESSENGER__HH
#include <retcode.hh>
#include <DOFRI.hh>
#include <StoppableTask.hh>

#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <queue>

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// AcceptArgs is a struct because you can only sent a single argument through
// the thread. This is a way to send all kinds of values.
struct AcceptArgs
{
    int listening_socket;
    std::queue<int>& accepted_sockets;
};

// A stopable daemon
class AcceptThread: public DaemonThread<AcceptArgs>
{

public:
    // Function to be executed by thread function
    void run(AcceptArgs args)
    {
        struct sockaddr incoming_accepted_address;
        socklen_t incoming_address_size = sizeof(incoming_accepted_address);
        int accept_socket = -1;
        char accepted_address[INET6_ADDRSTRLEN];

        std::cout << "Entered acceptance thread with socket: "
                  << args.listening_socket
                  << " \n";

        fcntl(args.listening_socket, F_SETFL, O_NONBLOCK);

        while (stopRequested() == false)
        {
            accept_socket = accept(args.listening_socket,
                            (struct sockaddr *)&incoming_accepted_address,
                            &incoming_address_size);

            if(0 < accept_socket)
            {
                args.accepted_sockets.push(accept_socket);
                inet_ntop(incoming_accepted_address.sa_family,
                get_in_addr((struct sockaddr *)&incoming_accepted_address),
                    accepted_address, sizeof(accepted_address));
                std::cout << "Got connection with IP: " << accepted_address << " on socket: " << accept_socket <<"\n";
            }
            else
            {
                // Nothing here so we slumber
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        std::cout << "Stopped acceptance thread!\n";

    }
};

class INETMessenger
{


public:
    INETMessenger(const std::string&& portNumber = "")

        : m_Ready(false), m_CanAccept(false), m_IsListening(false), m_IsAccepting(false),
          m_AcceptingPort(portNumber), m_AcceptingAddress(),
          m_ListeningSocket(-1), m_ConnectionAddress(), m_ConnectionSocket(-1),
          m_AcceptThread{}, m_AcceptTask(), m_AcceptedConnections(),
          m_AcceptSocketsQueue()
    {
        if(m_AcceptingPort.empty())
        {
            m_CanAccept = false;
            return;
        }

        struct addrinfo hints, *returnedAddrInfo, *currentAddrInfo;
        int getInfoStatus = 0;
        int yes=1;
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
            return;
        }
        else
        {
            char accepted_address[INET6_ADDRSTRLEN];

            inet_ntop(returnedAddrInfo->ai_addr->sa_family,
                      get_in_addr((struct sockaddr *)&returnedAddrInfo),
                      accepted_address,
                      sizeof(accepted_address));

            m_AcceptingAddress = accepted_address;
        }

        for(currentAddrInfo = returnedAddrInfo; currentAddrInfo != NULL; currentAddrInfo = currentAddrInfo->ai_next)
        {

            if ((m_ListeningSocket = socket(currentAddrInfo->ai_family, currentAddrInfo->ai_socktype,
                    currentAddrInfo->ai_protocol)) == -1) {
                perror("server: socket");
                continue;
            }

            if (setsockopt(m_ListeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1)
            {
                m_CanAccept = false;
                return;
            }

            if (bind(m_ListeningSocket, currentAddrInfo->ai_addr, currentAddrInfo->ai_addrlen) == -1) {
                close(m_ListeningSocket);
                perror("server: bind");
                continue;
            }

            break;
        }

        // Get a socket for listening for connections
        freeaddrinfo(returnedAddrInfo);
        if(nullptr == currentAddrInfo)
        {
            m_CanAccept = false;
            return;
        }

        m_CanAccept = true;
    }

    ~INETMessenger()
    {
        if(m_IsAccepting)
        {
            m_AcceptTask.stop();
            m_AcceptThread.join();
            close(m_ListeningSocket);
            while(!m_AcceptSocketsQueue.empty())
            {
                int& accept_socket = m_AcceptSocketsQueue.front();
                close(accept_socket);
                std::cout << "Closed socket: " << accept_socket << "\n";
                m_AcceptSocketsQueue.pop();
            }
        }

        if(m_IsListening)
        {
            close(m_ListeningSocket);
        }
    }

    RETCODE Send(int socket, char* message)
    {
        int message_length = strlen(message);
        while(message_length > 0)
        {
            message_length -= send(socket, message, message_length, 0);
        }

        return RTN_OK;
    }

    RETCODE Listen(int listenQueueSize = 10)
    {
        if(m_CanAccept)
        {
            // Start listening for connections
            listen(m_ListeningSocket, listenQueueSize);

            AcceptArgs args{m_ListeningSocket, m_AcceptSocketsQueue};

            m_AcceptThread = std::thread([this, args]()
            {
                this->m_AcceptTask.run(args);
            });

            m_IsAccepting = true;
            return RTN_OK;
        }

        return RTN_FAIL;
    }

    RETCODE Connect(const std::string& address, const std::string& port)
    {
        struct addrinfo hints, *returnedAddrInfo, *currentAddrInfo;
        int rv;
        char s[INET6_ADDRSTRLEN];

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((rv = getaddrinfo(address.c_str(), port.c_str(), &hints, &returnedAddrInfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return RTN_NOT_FOUND;
        }

        // loop through all the results and connect to the first we can
        for(currentAddrInfo = returnedAddrInfo; currentAddrInfo != NULL; currentAddrInfo = currentAddrInfo->ai_next) {
            if ((m_ConnectionSocket = socket(currentAddrInfo->ai_family, currentAddrInfo->ai_socktype,
                    currentAddrInfo->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }

            if (connect(m_ConnectionSocket, currentAddrInfo->ai_addr, currentAddrInfo->ai_addrlen) == -1) {
                close(m_ConnectionSocket);
                perror("client: connect");
                continue;
            }

            break;
        }

        if (currentAddrInfo == NULL) {
            return RTN_CONNECTION_FAIL;
        }

        char accepted_address[INET6_ADDRSTRLEN];

        inet_ntop(returnedAddrInfo->ai_addr->sa_family,
                    get_in_addr((struct sockaddr *)&returnedAddrInfo),
                    accepted_address,
                    sizeof(accepted_address));
        m_ConnectionAddress = accepted_address;

        freeaddrinfo(returnedAddrInfo); // all done with this structure

        m_IsListening = true;

        return RTN_OK;
    }

    RETCODE Recieve(int socket, void** buffer, int buffer_length)
    {
        int bytes_received = 1;
        while( bytes_received > 0 )
        {
            bytes_received = recv(socket, *buffer, buffer_length, 0);

        }

        return RTN_OK;

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

    inline std::string GetConnectedAddress(void)
    {
        if(m_IsListening)
        {
            return m_ConnectionAddress;
        }

        return "NO CONNECT ADDRESS";
    }

private:
    bool m_Ready;
    bool m_CanAccept;
    bool m_IsListening;
    bool m_IsAccepting;
    std::string m_AcceptingPort;
    std::string m_AcceptingAddress;
    std::thread m_AcceptThread;
    AcceptThread m_AcceptTask;
    int m_ListeningSocket;
    std::string m_ConnectionAddress;
    int m_ConnectionSocket;
    std::vector<struct sockaddr> m_AcceptedConnections;
    std::queue<int> m_AcceptSocketsQueue;
    std::vector<int> m_AcceptSockets;
};

#endif