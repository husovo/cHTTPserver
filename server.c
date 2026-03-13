#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <crtdefs.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8999"
#define BACK_LOG 10
#define BUFF_SIZE 1024

//int image_size(FILE *file, size_t *img_size);
void sendHeader(SOCKET *client);
int image_size(FILE *file, size_t *img_size);
void sendImg(SOCKET *client,char *content, size_t *imgSize);

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
        int bytes_recv = recv(client_fd,buffer,BUFF_SIZE - 1,0);
        if(bytes_recv > 0)
        {
            buffer[bytes_recv] = '\0';
            printf("%s",buffer);

            
            FILE *image = fopen("books.jpg","rb");
            if(!image)
            {
                return 1;
            }
            size_t img_len = 0;
            int imgLen = image_size(image,&img_len);
            char *req = buffer + 4;
            char *tok = strtok(req, " ");

            if (tok && tok[0] == '/')
                tok++;

            if (strcmp(tok, "") == 0) {
                sendHeader(&client_fd);
                FILE *html = fopen("index.html","r");
            if(!html)
            {
                printf("fialed to open file\n");
                return 1;
            }
            char respose[BUFF_SIZE];
            size_t read = 0;
            // keep reading till all bytes are read and sent
            while((read = fread(respose,sizeof respose[0],BUFF_SIZE,html)) > 0)
            {
                send(client_fd,respose,read,0);
            }
            // close file
            fclose(html);
            }
            else if (strcmp(tok, "books.jpg") == 0) {

                FILE *image = fopen("books.jpg", "rb");

                size_t img_len;
                image_size(image, &img_len);
                fclose(image);

                char header[256];
                snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: image/jpeg\r\n"
                    "Content-Length: %zu\r\n\r\n",
                    img_len);

                send(client_fd, header, strlen(header), 0);
                sendImg(&client_fd, "books.jpg", &img_len);
                
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

void sendHeader(SOCKET *client)
{
    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    if(send(*client,header,strlen(header),0) == SOCKET_ERROR)
    {
        printf("Send() Failed: %ld",WSAGetLastError());
        closesocket(*client);
        WSACleanup();
        return;
    }
}

void sendImg(SOCKET *client,char *content, size_t *imgSize)
{
    char buffer[BUFF_SIZE];
    size_t sum = 0;
    size_t read = 0;
    FILE *img = fopen(content,"rb");
    if(!img)
    {
        printf("Image not found\n");
        return;
    }else{
        while((read = fread(buffer,sizeof buffer[0],BUFF_SIZE,img)) > 0)
        {
            send(*client,buffer,read,0);
            sum += read;
        }
    }
}
int image_size(FILE *file, size_t *img_size)
{
    size_t sum = 0;
    size_t read = 0;
    char buffer[BUFF_SIZE];

    while ((read = fread(buffer, 1, BUFF_SIZE, file)) > 0) {
        sum += read;
    }

    if (ferror(file))
        return -1;        

    *img_size = sum;      
    return 1; 
}
