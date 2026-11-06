

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

// Size of headers
#define MAX_REQUEST_SIZE 4096
#define MAX_METHOD_SIZE 16
#define MAX_TARGET_SIZE 2048
#define MAX_VERSION_SIZE 16
#define MAX_HOST_SIZE 256

#include <cstring>
#include <iostream>

class HTTPRequest{
    public:
        HTTPRequest(char* request);

        bool parse();
        
        // Getters for the Header values
        char* getRawRequest();
        char* getMethod();
        char* getHost();
        char* getTarget();
        char* getVersion();

        int isValid();

    private:
        // Valid bit. Set to 0 if parse fails.
        int valid;

        // Houses the entire original message
        char rawRequest[MAX_REQUEST_SIZE];   

        // Headers
        char method[MAX_METHOD_SIZE];
        char host[MAX_HOST_SIZE];
        char target[MAX_TARGET_SIZE];
        char version[MAX_VERSION_SIZE];
        
};


#endif