/*
    Keenan Gowdar
    7861237
    COMP 4300 Assignment 1

    This class will hold the state of the HTTP request made by the 
        HTTP Client. It will then forward it out to the HTTP Server requested
        after being accepted by the HTTP proxy

*/

#include "HTTPRequest.h"

HTTPRequest::HTTPRequest(char* request){
    // Copy all characters from the original request into rawRequest
    std::strncpy(rawRequest, request, MAX_REQUEST_SIZE - 1);

    // Set the null terminator for all current variables.
    rawRequest[MAX_REQUEST_SIZE - 1] = '\0';
    method[0] = '\0';
    host[0] = '\0';
    target[0] = '\0';
    version[0] = '\0';

    // Initially set this to invalid.
    valid = 0;

    if(HTTPRequest::parse()){
        std::cout << "Successfully obtained a GET request\n";
        valid = 1;  // Valid GET request!
    } else {
        std::cout << "Did not recieve a proper GET request\n";
    }
}

/* 
    Parse the raw Request (line by line) and
    return true if all header fields have values

    What if we just strstr to the header names?
*/
bool HTTPRequest::parse(){
    char workingCpy[MAX_REQUEST_SIZE];
    strcpy(workingCpy, rawRequest);

    // Locate the end of the request line and headers
    char* firstLineEnd = strstr(workingCpy, "\r\n");
    char* headersEnd = strstr(rawRequest, "\r\n\r\n");

    if(firstLineEnd == nullptr || headersEnd == nullptr){
        std::cout << "Invalid HTTP request.\n";
        return false;
    }

    // Separate the first line from the remaining headers
    *firstLineEnd = '\0';

    // (1) Method - MUST BE GET
    char* currPosition = strtok(workingCpy, " ");

    if(currPosition == nullptr || strcmp(currPosition, "GET") != 0){
        std::cout << "Unsupported HTTP method.\n";
        return false;
    }

    if(strlen(currPosition) >= sizeof(method)) return false;
    strcpy(method, currPosition);

    // (2) URL
    currPosition = strtok(nullptr, " ");

    if(currPosition == nullptr || strlen(currPosition) >= sizeof(target)){
        std::cout << "Invalid URL.\n";
        return false;
    }

    strcpy(target, currPosition);

    // (3) HTTP Version
    currPosition = strtok(nullptr, " ");

    if(currPosition == nullptr || strlen(currPosition) >= sizeof(version)){
        std::cout << "Missing HTTP version.\n";
        return false;
    }

    if(strcmp(currPosition, "HTTP/1.1") != 0 &&
       strcmp(currPosition, "HTTP/1.0") != 0){
        return false;
    }

    strcpy(version, currPosition);

    // Reject unexpected additional request-line fields
    if(strtok(nullptr, " ") != nullptr) return false;

    // (4) Extract the Host header
    char* hostStart = strstr(rawRequest, "\r\nHost:");

    // Also accept lowercase "host:"
    if(hostStart == nullptr){
        hostStart = strstr(rawRequest, "\r\nhost:");
    }

    if(hostStart == nullptr || hostStart >= headersEnd){
        std::cout << "Missing Host header.\n";
        return false;
    }

    // Skip past "\r\nHost:"
    hostStart += 7;

    // Skip leading spaces
    while(*hostStart == ' ' || *hostStart == '\t'){
        hostStart++;
    }

    // Find the end of the Host header
    char* hostEnd = strstr(hostStart, "\r\n");

    if(hostEnd == nullptr || hostEnd > headersEnd){
        return false;
    }

    // Remove trailing whitespace
    while(hostEnd > hostStart &&
          (hostEnd[-1] == ' ' || hostEnd[-1] == '\t')){
        hostEnd--;
    }

    size_t hostLength = hostEnd - hostStart;

    if(hostLength == 0 || hostLength >= sizeof(host)){
        return false;
    }

    strncpy(host, hostStart, hostLength);
    host[hostLength] = '\0';

    return true;
}

// Getters
char* HTTPRequest::getRawRequest(){return rawRequest;}
char* HTTPRequest::getMethod(){return method;}
char* HTTPRequest::getHost(){return host;}
char* HTTPRequest::getTarget(){return target;}
char* HTTPRequest::getVersion(){return version;}
int HTTPRequest::isValid(){return valid;}
