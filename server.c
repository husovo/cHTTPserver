#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <crtdefs.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8999"
#define BACK_LOG 10
#define BUFF_SIZE 1024

void sendImg(SOCKET client_fd, FILE *img);
void sendHead(SOCKET client_fd, char *content_type, long len);
int sendall(SOCKET client_fd, char *buff, size_t *len);
char *get_content_type(char *type);
void handle_get(SOCKET client_fd, char *path, char *realpath);

int main(void)
{
    WSADATA wsaData;
    int iResult, client_len;
    struct sockaddr_storage client;

    // initialise winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
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
    if ((iResult = getaddrinfo(NULL, PORT, &hints, &res)) != 0)
    {
        printf("gai error: %d", iResult);
        WSACleanup();
        return 1;
    }

    SOCKET sockfd = INVALID_SOCKET;

    // A SOCKET object for the server
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == INVALID_SOCKET)
    {
        printf("socket() error: %d", WSAGetLastError());
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    // bind the socket to a network address within the system
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR)
    {
        printf("Bind() error: %d", WSAGetLastError());
        freeaddrinfo(res);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(res);

    // listen for incoming connections
    if (listen(sockfd, BACK_LOG) == SOCKET_ERROR)
    {
        printf("Listen() error: %d", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    printf("listening on port: %s\n", PORT);

    SOCKET client_fd = INVALID_SOCKET;

    while (1)
    {
        client_len = sizeof client;
        // accept cleint connection
        if ((client_fd = accept(sockfd, (struct sockaddr *)&client, &client_len)) == INVALID_SOCKET)
        {
            printf("Accept() error: %d", WSAGetLastError());
            closesocket(sockfd);
            WSACleanup();
            continue;
        }

        char buffer[BUFF_SIZE];

        // http header + response
        int bytes_recv = recv(client_fd, buffer, BUFF_SIZE - 1, 0);
        if (bytes_recv <= 0)
        {
            closesocket(client_fd);
            continue;
        }
        buffer[bytes_recv] = '\0';

        char *line = strtok(buffer, "\r\n");

        char method[20], path[256], protocol[16];
        ;

        if (sscanf(line, "%19s %255s %15s", method, path, protocol) != 3)
        {
            closesocket(client_fd);
            continue;
        }
        char *realpath = path;

        if (path[0] == '/' && path[1] != '\0')
        {
            realpath = path + 1;
        }
        if (strstr(realpath, ".."))
        {
            char header[] = "HTTP/1.1 404 Not Found\r\n"
                            "Content-Type: text/plain\r\n"
                            "\r\n"
                            "NOT FOUND";
            send(client_fd, header, strlen(header), 0);
            closesocket(client_fd);
            continue;
        }
        printf("%s %s\n", method, path);

        if (strcmp(method, "GET") == 0)
        {
            handle_get(client_fd, path, realpath);
        }
    }
    closesocket(sockfd);

    return 0;
}

void sendHead(SOCKET client_fd, char *content_type, long len)
{
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "connection: close\r\n"
             "Content-Length: %ld\r\n\r\n",
             content_type, len);
    size_t headerlen = strlen(header);
    if (sendall(client_fd, header, &headerlen) == SOCKET_ERROR)
    {
        printf("Send head Failed: %ld", WSAGetLastError());
        closesocket(client_fd);
        WSACleanup();
        return;
    }
}

void sendImg(SOCKET client, FILE *img)
{
    char buffer[BUFF_SIZE];
    size_t read = 0;

    while ((read = fread(buffer, sizeof buffer[0], BUFF_SIZE, img)) > 0)
    {
        if (sendall(client, buffer, &read) == SOCKET_ERROR)
        {
            printf("Send img Failed: %ld", WSAGetLastError());
            closesocket(client);
            WSACleanup();
            return;
        }
    }
}

int sendall(SOCKET client_fd, char *buff, size_t *len) // ref: beej guide
{
    int total = 0;
    int bytesleft = *len;
    size_t original_len = *len;
    int n;

    while (total < original_len)
    {
        n = send(client_fd, buff + total, bytesleft, 0);
        if (n == SOCKET_ERROR)
            break;
        total += n;
        bytesleft -= n;
    }
    *len = total;

    return (total == original_len) ? 0 : SOCKET_ERROR;
}

char *get_content_type(char *type)
{
    if (strcmp(type, ".jpg") == 0 || strcmp(type, ".jpeg") == 0)
    {
        return "image/jpeg";
    }
    else if (strcmp(type, ".html") == 0)
    {
        return "text/html";
    }
    else if (strcmp(type, ".css"))
    {
        return "text/css";
    }
    else if (strcmp(type, ".js"))
    {
        return "application/javascript";
    }
    return "application/octet-stream";
}
void handle_get(SOCKET client_fd, char *path, char *realpath)
{
    char filepath[256];
    if(strcmp(path, "/") == 0)
    {
        snprintf(filepath,sizeof(filepath),"index.html");
    }else{
        snprintf(filepath,sizeof(filepath),realpath);
    }

    FILE *file = fopen(filepath, "rb");
    if(!file)
    {
        char header[] = "HTTP/1.1 404 Not Found\r\n"
                            "Content-Type: text/plain\r\n"
                            "\r\n"
                            "NOT FOUND";
            send(client_fd, header, strlen(header), 0);
            closesocket(client_fd);
            return;
    }
    // file size
    fseek(file,0L,SEEK_END);
    long file_len = ftell(file);
    rewind(file);

    // extention
    char *type = strrchr(filepath,'.');
    if(!type)
        type = "";
    char *content_type = get_content_type(type);

    // send the the response header
    sendHead(client_fd,content_type,file_len);

    //send the file
    char buffer[BUFF_SIZE];
    size_t read;
    while((read = fread(buffer, 1,BUFF_SIZE,file)) > 0)
    {
        if(sendall(client_fd,buffer,&read) == SOCKET_ERROR)
            break;
    }

    fclose(file);
    closesocket(client_fd);
}
