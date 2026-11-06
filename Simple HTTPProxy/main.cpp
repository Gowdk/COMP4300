/*
    Keenan Gowdar
    7861237
    COMP 4300 Assignment 1

    This file serves as the main entry point into the proxy server
*/

#include <iostream>
#include "HTTPProxyServer.h"

int last_four_digits(int studentID){
    return studentID % 10000;
}


int main(){
    int studentID = 7861237;
    HTTPProxyServer server(10000 + last_four_digits(studentID), studentID); 

    server.beginListening(5);
}



