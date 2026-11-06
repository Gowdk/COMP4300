// HTTPProxyServer.h

#ifndef HTTP_PROXY_SERVER_H
#define HTTP_PROXY_SERVER_H
#define BUFFER_SIZE 4096

#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h> //inet_pton(), inet_ntop()
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      //addrinfo struc and assiocated DNS resolution functions

#include "HTTPRequest.h"

class HTTPProxyServer {
public:
    HTTPProxyServer(int proxyPort, int studentID);
    void beginListening(int waitSize);
    

private:
    int proxyPort;
    int serverSocket;
    int studentID;
    sockaddr_in proxyAddress{};

    void bindSocket(int proxyPort);

    // For manipulating the request received.
    void handleRequest(HTTPRequest& getRequest, int clientSocket);

    /*
        Calls recv() on the destinationSocket.

        Dynamically allocated char * response is returned.
        responseLength is modified to be the length of response.
    */
    char * receiveResponse(int destinationSocket, ssize_t& responseLength);
    
    // Check if the response code is 200!
    bool statusCodeCheck(char* response, ssize_t responseLength);

    /*
        Modifies the response recieved from the server by replacing:
        "RFC ###" --> "7 digits of the student number"

        NOTE: These are of same length, so in-place modification is safe.

        Returns the number of replacements
    */
    ssize_t modifyResponse(char* response, ssize_t responseLength);

    /*
        Sends the client (whom made the intial request) the modified response.
    */
    bool sendResponse(int clientSocket, char* response, ssize_t responseLength);
};

#endif