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
    void* return_value;
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

        while (stopRequested() == false)
        {
            accept_socket = accept(args.listening_socket,
                            (struct sockaddr *)&incoming_accepted_address,
                            &incoming_address_size);

            if(0 < accept_socket)
            {
                inet_ntop(incoming_accepted_address.sa_family,
                get_in_addr((struct sockaddr *)&incoming_accepted_address),
                    accepted_address, sizeof(accepted_address));
                std::cout << "Got connection with IP: " << accepted_address;
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
    INETMessenger(const std::string&& portNumber,
                  unsigned int listenQueuSize = 10)

        : m_Ready(false), m_PortNumber(portNumber), m_ListenQueueSize(listenQueuSize),
          m_ListeningSocket(-1), m_AcceptThread{}, m_AcceptTask(),
          m_AcceptedConnections(), m_AcceptSockets()
    {
        struct addrinfo hints;

        // Set how we want the results to come as
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC; // IPV4 or IPV6
        hints.ai_socktype = SOCK_STREAM; // slower, yet reliable
        hints.ai_flags = AI_PASSIVE; // fill in IP for me

        // Get address for self
        if(0 < getaddrinfo(nullptr, m_PortNumber.c_str(), &hints, &m_Result))
        {
            m_Ready = false;
            // bad mojo error here
        }
        // Get a socket for listening for connections
        m_ListeningSocket = socket(m_Result->ai_family, m_Result->ai_socktype, m_Result->ai_protocol);
        m_Ready = true;
    }

    ~INETMessenger()
    {
        m_AcceptTask.stop();
        m_AcceptThread.join();
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

    RETCODE Listen()
    {
        if(m_Ready)
        {
            // Bind the socket to this executable
            bind(m_ListeningSocket, m_Result->ai_addr, m_Result->ai_addrlen);
            // Start listening for connections
            listen(m_ListeningSocket, m_ListenQueueSize);

            AcceptArgs accept_args = {m_ListeningSocket, nullptr};

            m_AcceptThread = std::thread([&]()
            {
                m_AcceptTask.run(accept_args);
            });

            return RTN_OK;
        }

        return RTN_FAIL;
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

private:
    bool m_Ready;
    std::string m_PortNumber;
    unsigned int m_ListenQueueSize;
    struct addrinfo* m_Result; // Probably should copy this value
                               //instead of pointer
    std::thread m_AcceptThread;
    AcceptThread m_AcceptTask;
    int m_ListeningSocket;
    std::vector<struct sockaddr> m_AcceptedConnections;
    std::vector<int> m_AcceptSockets;
};

#endif