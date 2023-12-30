#include "TCPRequestChannel.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>

using namespace std;

// I had to change port_no to be an int as I haad major issues with stoi() converison inside the constructor
// Now, stoi() conversion of the port argument is handled in main. I think this is a much more elegant solution :)

TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const int _port_no) {
    struct sockaddr_in socket_in;
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // create an IPv4 TCP socket

    //cout << _port_no << endl;             // debug, used to resolve port stoi issue.

    socket_in.sin_family = AF_INET;
    socket_in.sin_port = htons(_port_no);

    int statusBindToPort = 0, statusListenToPort = 0;

    if (_ip_address == "") {
        socket_in.sin_addr.s_addr = INADDR_ANY;

        statusBindToPort = ::bind(sockfd, (struct sockaddr*) &socket_in, sizeof(socket_in));
        
        // Error handling for bind to port, useful as this shows why tests fail
        // I had issues with all tests passing in 1 run, then all tests failing another run.
        // This reveals why testing twice in a row fails - everything fails to bind to port
        // I think this is just Ubuntu being slow to free up the ports after the first make test runs
        // Concurent tests after a 30 sec - 1 min wait work fine.

        if (statusBindToPort < 0) {
            perror("Error binding to port!");
            exit(1);
        }

        statusListenToPort = listen(sockfd, 20);

        // Similar error handling for listening to port to see why my tests are failing

        if (statusListenToPort < 0) {
            perror("Error listening to port!");
            exit(1);
        }
    }

    else {
        struct in_addr ip_addr_in;
        inet_pton(AF_INET, _ip_address.c_str(), &ip_addr_in);
        socket_in.sin_addr = ip_addr_in;
        connect(sockfd, (struct sockaddr*) &socket_in, sizeof(socket_in));
    }
}

// Constructor 2: When given socket FD, associate with internal socket FD var
TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    this->sockfd = _sockfd;
}


// Destructor: Close socket
TCPRequestChannel::~TCPRequestChannel () {
    close(this->sockfd);
}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_storage client_addr;
    socklen_t sockin_size = sizeof(client_addr);

    int client_socket = accept(sockfd, (struct sockaddr*) &client_addr, &sockin_size);
    return client_socket;
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return recv(sockfd, msgbuf, msgsize, 0);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return send(sockfd, msgbuf, msgsize, 0);
}
