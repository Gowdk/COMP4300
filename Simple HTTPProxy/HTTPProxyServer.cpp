/*
    Keenan Gowdar
    7861237
    COMP 4300 Assignment 1

    Provides the implementation of the Proxy Server.
     
*/
#include "HTTPProxyServer.h"

// Constructor sets up the socket and binds it to the proxy port
HTTPProxyServer::HTTPProxyServer(int port, int studentID){
    proxyPort = port;
    this->studentID = studentID;

    // AF_INET (IPv4), SOCK_STREAM (TCP), 0 == Default protocol
    if((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket() gave a bad fd\n");
        return;
    }
    
    bindSocket(proxyPort);
}

void HTTPProxyServer::bindSocket(int proxyPort){
    // Set up the sockaddr_in responsible for the listening port
    proxyAddress.sin_family = AF_INET;          // IPv4
    proxyAddress.sin_addr.s_addr = INADDR_ANY;  // Any interface
    proxyAddress.sin_port = htons(proxyPort);   

    // Bind the serverSocket to the proxyAddress
    if(bind(serverSocket, (sockaddr*)&proxyAddress, sizeof(proxyAddress)) == -1){
        perror("Failed to bind socket");
        close(serverSocket);
        return;
    }

    std::cout << "Listening for connections...\n";
}

// Begins listening, and blocks until a connection has been accepted.
/*
    int waitSize:           Max number of connections that can be placed into the wait queue
    ssize_t bufferSize:      size of data that can be recieved in buffer

*/
void HTTPProxyServer::beginListening(int waitSize){
    // Start listening for connections. We'll set a max of waitSize waiting connections.
    if(listen(serverSocket, waitSize) == -1){
        perror("Could not listen to the socket created, closed socket.");
        close(serverSocket);
    }

    // std::string vs char * ?? Interesting C++...
    std::string badHTTPMethod = "Unsupported HTTP method.\n";

    // Keep connection alive until client disconnects
    while(true){
        // accept()
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if(clientSocket == -1){
            perror("accept() failed");
            continue;
        } else {
            std::cout << "Connection Found!\n"; 
        }
       
        // Receive the data from the client
        char buffer[BUFFER_SIZE];

        ssize_t bytesRecieved = recv(clientSocket, buffer, sizeof(buffer), 0);
        // If client disconnects or error occurs, break.
        if(bytesRecieved <= 0){
            break;
        }

        buffer[bytesRecieved] = '\0'; // Null-terminate the bytes received.

        // Check the recieved request
        HTTPRequest newRequest(buffer);

        // Only deal with the newRequest if it is a valid GET request
        if(newRequest.isValid()){
            // For debugging: 
            // std::cout.write(buffer, bytesRecieved) << std::endl; // Print exactly the bytes received

            // Echo back the buffer
            // send(clientSocket, buffer, bytesRecieved, 0);

            // A new request has been created! Let's forward it out
            handleRequest(newRequest, clientSocket);
            close(clientSocket);
        } else {
            // Stop listening if a non-valid request was received.
            send(clientSocket, badHTTPMethod.c_str(), badHTTPMethod.size(), 0);
            close(clientSocket);
            break;
        }
    }

    //Only close the server socket on kill sig
    close(serverSocket);
}

void HTTPProxyServer::handleRequest(HTTPRequest& getRequest, int clientSocket){
    
    // Get the destination hostname
    char* host = getRequest.getHost();
    char* target = getRequest.getTarget();
    char* version = getRequest.getVersion();

    // Resolve hostname to network address information
    addrinfo hints{};
    addrinfo* serverInfo = nullptr;
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP

    // DNS Resolution function. Open on port 80
    if(getaddrinfo(host, "80", &hints, &serverInfo) != 0){
        perror("Could not resolve hostname.\n");
        return;
    }

    // Create destination socket
    int destinationSocket = socket(
        serverInfo->ai_family,
        serverInfo->ai_socktype,
        serverInfo->ai_protocol
    );

    // Error handling
    if(destinationSocket == -1){
        perror("Could not create destination socket.");
        freeaddrinfo(serverInfo);
        return;
    }

    // Establish TCP connection
    if(connect(destinationSocket,
               serverInfo->ai_addr,
               serverInfo->ai_addrlen) == -1){

        perror("Could not connect to destination server.");
        close(destinationSocket);
        freeaddrinfo(serverInfo);
        return;
    }

    // Free the memory associated with the addrinfo struc
    freeaddrinfo(serverInfo);
    std::cout << "Connected to destination server!\n";

    // Next: send the HTTP GET request
    // Get the path from the URL
    const char* path = target;

    if(strncmp(target, "http://", 7) == 0){
        path = strchr(target + 7, '/');

        // No path specified, request the root
        if(path == nullptr){
            path = "/";
        }
    }

    // Construct the GET request
    char requestBuffer[BUFFER_SIZE];

    // Format the request to be forwarded.
    int requestLength = snprintf(
        requestBuffer,
        sizeof(requestBuffer),
        "GET %s %s\r\n"
        "Host: %s\r\n"
        "User-Agent: curl/8.18.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        path,
        version,
        host
    );

    // Error handling if the formating function fails
    if(requestLength < 0 || requestLength >= sizeof(requestBuffer)){
        perror("Could not construct HTTP request.\n");
        close(destinationSocket);
        return;
    }

    // Send the request
    // Forward the request to the destination server
    int totalSent = 0;

    // While there's more of the request to send, continue to send.
    while(totalSent < requestLength){
        ssize_t bytesSent = send(
            destinationSocket,
            requestBuffer + totalSent,
            requestLength - totalSent,
            0
        );

        if(bytesSent <= 0){
            perror("Failed to forward HTTP request");
            close(destinationSocket);
            return;
        }

        totalSent += bytesSent;
    }

    std::cout << "Successfully forwarded HTTP GET request!\n";

    // Recieve the response then close the socket.
    ssize_t responseLength = 0;
    char * response = receiveResponse(destinationSocket, responseLength);
    close(destinationSocket);

    // Quick error check on an empty response.
    if(response == nullptr){
        perror("Failed to receive HTTP response.\n");
        return;
    }

    // Modify the response directly in memory
    ssize_t modifications = modifyResponse(response, responseLength);

    std::cout << "Made " << modifications
            << " RFC replacements.\n";

    // Next: forward the modified response to clientSocket
    sendResponse(clientSocket, response, responseLength);

    // After its all said and done, free the memory allocated to response.
    delete[] response;
}



/*
    Return the response using dynamic allocation.
    - Every time the response reaches its maximum size, defined
        by responseCapacity, we will double the capacity.
    - We will also modify the responseLength parameter to be the length of the response.
*/
char * HTTPProxyServer::receiveResponse(int destinationSocket, ssize_t& responseLength){
    char recievedBuffer[4096];

    ssize_t responseCapacity = 8192;

    // Allocate space for response data + null terminator
    char* response = new char[responseCapacity + 1];

    while(true){

        // Receive the bytes
        ssize_t bytesRecieved = recv(
            destinationSocket,
            recievedBuffer,
            sizeof(recievedBuffer),
            0
        );

        // EoT
        if(bytesRecieved == 0){
            break;
        }

        // Error 
        if(bytesRecieved < 0){
            perror("Error receiving response");
            delete[] response;
            return nullptr;
        }

        // Count the total size of bytes recieved at this iteration
        ssize_t bytesToCopy = bytesRecieved;

        // Expand the buffer if necessary
        if(responseLength + bytesToCopy > responseCapacity){

            // Double our buffer size
            ssize_t newCapacity = responseCapacity * 2;
            char* newResponse = new char[newCapacity + 1];

            // Copy previously received bytes
            memcpy(newResponse, response, responseLength);

            // Release old memory
            delete[] response;

            // Update pointer and capacity
            response = newResponse;
            responseCapacity = newCapacity;
        }

        // Append the newly received bytes
        memcpy(
            response + responseLength,
            recievedBuffer,
            bytesToCopy
        );

        // Count total length of our response.
        responseLength += bytesToCopy;
    }

    // null terminator 
    response[responseLength] = '\0';

    std::cout << "Successfully recieved initial response. Moving on to replacements (if needed).\n";

    return response;
}

/*
    Looks for the first occurrence of the string:
        "200 OK"
*/
bool HTTPProxyServer::statusCodeCheck(char* response, ssize_t responseLength){
    bool statusLineFound = false;
    std::string okStatus = "200 OK\r\n"; 

    // Find the first occurrence of the end of line pattern "\r\n"
    for(ssize_t i = 0; i + 2 <= responseLength; i++){
        if(memcpy(response + i, okStatus.c_str(), okStatus.size()) == 0){
            statusLineFound = true;
            break;
        }
    }

    // If we couldn't find a status line
    if(!statusLineFound){
        perror("Could not find the statusLine in our response.");
        return statusLineFound;
    }


    return statusLineFound;    
}

ssize_t HTTPProxyServer::modifyResponse(char* response, ssize_t responseLength){
    // Replacement String. Student ID is built when the constructor is called.
    char replacement[8];
    snprintf(replacement, sizeof(replacement), "%07d", studentID);
    const ssize_t patternLength = 7;
    ssize_t replacements = 0;

    ssize_t bodyStart = 0;
    bool foundBody = false;
    
    // Find the end of the HTTP headers by walking through each character until
    //  \r\n\r\n is found. We know the body starts exactly 4 characters after it is found.
    for(ssize_t i = 0; i + 4 <= responseLength; i++){
        if(memcmp(response + i, "\r\n\r\n", 4) == 0){
            bodyStart = i + 4;
            foundBody = true;
            break;
        }
    }

    // Could not find body.
    if(!foundBody){
        perror("Could not find HTTP response body.\n");
        return 0;
    }

    // Search the response body for "RFC ###"
    ssize_t currPosition = bodyStart;
    while(currPosition + patternLength <= responseLength){

        // First check for "RFC "
        if(memcmp(response + currPosition, "RFC ", 4) == 0){

            bool valid = true;

            // Verify the following three characters are digits
            for(int j = 4; j < 7; j++){
                if(response[currPosition+j] < '0' || response[currPosition+j] > '9'){
                    valid = false;
                    break;
                }
            }

            // Make sure there isn't a fourth digit
            if(currPosition + 7 < responseLength &&
               response[currPosition+7] >= '0' &&
               response[currPosition+7] <= '9'){
                valid = false;
            }

            // If we have a valid occurrence, Replace directly in the existing buffer
            if(valid){
                memcpy(response + currPosition, replacement, patternLength);

                replacements++; // Increment our replacement count
                currPosition += patternLength; // Skip past our pattern Length. Recall pattern length == to our replacement string
                continue;
            }
        }

        currPosition++; // Move on to next position
    }

    return replacements;
}

bool HTTPProxyServer::sendResponse(int clientSocket, char* response, ssize_t responseLength){
    bool success = false;

    ssize_t totalSent = 0;

    // While we still have bytes to send
    while(totalSent < responseLength){
        ssize_t bytesSent = send(clientSocket, response + totalSent,
                                responseLength - totalSent, 0);
        
        if(bytesSent < 0){
            perror("Error sending back the modified response!");
            return false;
        }

        totalSent += bytesSent;
    }

    std::cout << "Successfully forwarded "
              << totalSent
              << " bytes to the HTTP client.\n";

    return true;

}