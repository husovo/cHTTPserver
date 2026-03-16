#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <crtdefs.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8999"
#define BACK_LOG 10
#define BUFF_SIZE 1024


void sendImg(SOCKET client,FILE *img);
void sendHead(SOCKET client_fd, char*content_type,long len);
int sendall(SOCKET client_fd, char *buff, size_t *len);
char *get_content_type(char *type);

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
        printf("socket() error: %ld", WSAGetLastError());
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
        printf("Listen() error: %ld", WSAGetLastError());
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
        if (bytes_recv > 0)
        {
            buffer[bytes_recv] = '\0';
            printf("%s", buffer);
            char *line = strtok(buffer, "\r\n");
            char method[20], path[256], protocol[16];
            sscanf(line,"%s%s%s",method,path,protocol);
            
            if (!method || !path || !protocol)
            {
                closesocket(client_fd);
                continue;
            }
            if(strcmp(method,"GET") != 0)
            {
                closesocket(client_fd);
                continue;
            }
            char *type = strrchr(path,'.');
            if(type == NULL)
            type = "";
            char *realpath = path;
            if(path[0] == '/' && path[1] != '\0'){
                realpath = path +1;
            }
            if(strstr(realpath,".."))
            {

                char header[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n"
                                "\r\n"
                                "NOT FOUND";
                send(client_fd, header, strlen(header),0);
                    closesocket(client_fd);
                continue;
            }

            if ( (path[0] == '/' && path[1] == '\0') )
            {
            
                FILE *html = fopen("index.html", "r");
                if (!html)
                {
                    printf("fialed to open file\n");
                    return 1;
                }
                // find the size of the html file
                fseek(html, 0L, SEEK_END);
                long html_len = ftell(html);
                rewind(html);
                char *content_type = get_content_type(type);
                // send the image header
                sendHead(client_fd,content_type,html_len);
                char respose[BUFF_SIZE];
                size_t read = 0;
                // keep reading till all bytes are read and sent
                while ((read = fread(respose, sizeof respose[0], BUFF_SIZE, html)) > 0)
                {
                    send(client_fd, respose, read, 0);
                }
                // close file
                fclose(html);
            }
            else if(strcmp(type,".jpg") ==0 ||strcmp(type,".jepg") ==0)
            {
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "%s", realpath);
                FILE *image = fopen(filepath, "rb");
                if (!image)
                {
                    closesocket(client_fd);
                    char header[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n"
                                    "NOT FOUND";
                    send(client_fd, header, strlen(header), 0);
                    continue;
                }
                // find the size of the image
                fseek(image, 0L, SEEK_END);
                long img_len = ftell(image);
                rewind(image);
                char *content_type = get_content_type(type);
                // send the image header
                sendHead(client_fd,content_type,img_len);

                // send the image data
                sendImg(client_fd, image);
                fclose(image);
            }else
            {
                closesocket(client_fd);
                continue;
            }
        }
        if (bytes_recv == 0)
        {
            printf("connection closing");
        }
        closesocket(client_fd);
    }
    closesocket(sockfd);

    return 0;
}


void sendHead(SOCKET client_fd, char*content_type,long len)
{
    char header[256];
                snprintf(header, sizeof(header),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %ld\r\n\r\n",
                         content_type, len);
    size_t headerlen = strlen(header);
    if(sendall(client_fd, header, &headerlen)== SOCKET_ERROR)
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
        if(sendall(client, buffer, &read) == SOCKET_ERROR)
        {
            printf("Send img Failed: %ld", WSAGetLastError());
            closesocket(client);
            WSACleanup();
            return;
        }
    }
}

int sendall(SOCKET client_fd, char *buff, size_t *len)
{
    int total = 0;
    int bytesleft = *len;
    int n; 

    while(total < *len)
    {
        n = send(client_fd, buff+total,bytesleft,0);
        if(n == SOCKET_ERROR)
            break;
        total += n;
        bytesleft -= n;
    }
    *len = total;

    return n == -1? SOCKET_ERROR : 0;
}

char *get_content_type(char *type)
{
    if(strcmp(type,".jpg") ==0 || strcmp(type,".jpeg") == 0)
    {
        return "image/jpeg";
    }else if(strcmp(type,".html") ==0)
    {
        return "text/html";
    }else
    {
        return "text/html";
    }
}