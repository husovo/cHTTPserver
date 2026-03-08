#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <crtdefs.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8999"
#define BACK_LOG 10
#define BUFF_SIZE 1024

int main(void)
{
    WSADATA wsaData;
    int iResult, client_len;
    struct sockaddr_storage client;

    // initialise winsock
    iResult = WSAStartup(MAKEWORD(2,2),&wsaData);
    if(iResult != 0)
    {
        printf("WSAStartup failed: %d", iResult);
        return 1;
    }

    // instantiate a SOCKET object
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE; 

    // get the addinfo and translate it to a usable ip addresses
    if((iResult = getaddrinfo(NULL,PORT,&hints,&res)) != 0)
    {
        printf("gai error: %d",iResult);
        WSACleanup();
        return 1;
    }
    
    SOCKET sockfd = INVALID_SOCKET;
    
    // A SOCKET object for the server
    if((sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol)) == INVALID_SOCKET)
    {
        printf("socket() error: %ld",WSAGetLastError());
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    // bind the socket to a network address within the system
    if(bind(sockfd, res->ai_addr,res->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Bind() error: %d",WSAGetLastError());
        freeaddrinfo(res);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(res);

    // listen for incoming connections
    if(listen(sockfd,BACK_LOG) == SOCKET_ERROR)
    {
        printf("Listen() error: %ld",WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    SOCKET client_fd = INVALID_SOCKET;

    while(1)
    {
        client_len = sizeof client;
        // accept cleint connection
        if((client_fd = accept(sockfd,(struct sockaddr *)&client,&client_len)) == INVALID_SOCKET)
        {
            printf("Accept() error: %d",WSAGetLastError());
            closesocket(sockfd);
            WSACleanup();
            return 1;
        }

        char buffer[BUFF_SIZE];

        // http header + response
        char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" "Hello from the server";
        int bytes_recv = recv(client_fd,buffer,BUFF_SIZE - 1,0);
        if(bytes_recv > 0)
        {
            buffer[bytes_recv] = '\0';
            printf("%s",buffer);
            
            // send a simple header + response
            if(send(client_fd,header,strlen(header),0) == SOCKET_ERROR)
            {
                printf("Send() Failed: %ld",WSAGetLastError());
                closesocket(client_fd);
                WSACleanup();
                return 1;
            }
        }
        if(bytes_recv == 0)
        {
            printf("connection closing");
        }
        closesocket(client_fd);
    }
    closesocket(sockfd);

    return 0;
}
