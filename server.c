#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFERSIZE 8096
#define SERVER_DIR "./html"
#define PORT 8081

struct
{
    char *ext;
    char *filetype;
} extensions[] = {
    {"gif", "image/gif"},
    {"jpg", "image/jpg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"ico", "image/ico"},
    {"zip", "image/zip"},
    {"gz", "image/gz"},
    {"tar", "image/tar"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {0, 0}};

void web(int fd, int req_count)
{
    int j, file_fd, buf_len;
    long i, ret, len;
    char *fstr;
    static char buffer[BUFFERSIZE + 1]; /* To ensure zero initi*/

    ret = read(fd, buffer, BUFFERSIZE); /* Read HTTP request in one go */
    if (ret == 0 || ret == -1)
    { /* Stop if failed to read  */
        printf("Failed to read browser request\n");
    }
    if (ret > 0 && ret < BUFFERSIZE) /* Check if the return code is within valid range */
        buffer[ret] = 0;          /* Terminate the buffer */
    else
        buffer[0] = 0;
    for (i = 0; i < ret; i++) /* Replace carriage return and line feed characters */
        if (buffer[i] == '\r' || buffer[i] == '\n')
            buffer[i] = '*';
    printf("Request recieved %s : %d \n", buffer, req_count);
    if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4))
    {
        printf("Only simple GET operation supported : %s \n", buffer);
        (void)write(fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forb1dden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type, or operation is not allowed on this simple static file web server.\n</body></html>\n", 271);
    }
    for (i = 4; i < BUFFERSIZE; i++)
    { /*Null terminatation after the second space */
        if (buffer[i] == ' ')
        { 
            buffer[i] = 0;
            break;
        }
    }
    for (j = 0; j < i - 1; j++) /* check for illegal parent directory use .. */
        if (buffer[j] == '.' && buffer[j + 1] == '.')
        {
            printf("Directory . or .. is unsupoorted : %s \n", buffer);
            (void)write(fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forb2dden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
        }
    if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6))
        (void)strcpy(buffer, "GET /index.html");
    buf_len = strlen(buffer);
    fstr = (char *)0;
    for (i = 0; extensions[i].ext != 0; i++)
    {
        len = strlen(extensions[i].ext);
        if (!strncmp(&buffer[buf_len - len], extensions[i].ext, len))
        {
            fstr = extensions[i].filetype;
            break;
        }
    }
    if (fstr == 0)
    {
        printf("Only simple GET operation supported : %s \n", buffer);
        (void)write(fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forb3dden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n", 271);
    }

    if ((file_fd = open(&buffer[5], O_RDONLY)) == -1)
    { /* open the file for reading */
        printf("Failed to open file :");
        (void)write(fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n", 224);
    }

    len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* Seek to the end of the file to find its length */
    (void)lseek(file_fd, (off_t)0, SEEK_SET);       /* Seek back to the beginning of the file */

    (void)sprintf(buffer, "HTTP/1.1 200 OK\nServer: webserver/1.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", len, fstr); /* Header + a blank line */

    (void)write(fd, buffer, strlen(buffer));

    /* Send file in 8KB blocks */
    while ((ret = read(file_fd, buffer, BUFFERSIZE)) > 0)
    {
        (void)write(fd, buffer, ret);
    }
    sleep(1); /* Allow the socket to drain before signaling */
    close(fd);
    exit(1);
}

int main()
{
    int port, listenfd, socketfd, req_count;
    // int i, pid;
    socklen_t length;
    static struct sockaddr_in client_addr;  
    static struct sockaddr_in server_addr; 

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        printf("error init socket");
    port = PORT;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        printf("error bind socket\n");
    if (listen(listenfd, 64) < 0)
        printf("error listen socket\n");

    if (chdir(SERVER_DIR) == -1)
    {
        (void)printf("ERROR: Can't Change to directory %s\n", SERVER_DIR);
        exit(EXIT_FAILURE);
    }
    for (req_count = 1;; req_count++)
    {
        length = sizeof(client_addr);
        if ((socketfd = accept(listenfd, (struct sockaddr *)&client_addr, &length)) < 0)
            printf("error accept socket\n");

        // Fork to handle the incoming connection in a separate process
        pid_t pid = fork();

        if (pid < 0)
        {
            printf("Error forking process\n");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // Child process
            close(listenfd);    // Close listening socket 
            web(socketfd, req_count); // Handle the request 
            close(socketfd);    // Close the socket
            exit(EXIT_SUCCESS);
        }
        else
        {
            // Parent process
            close(socketfd); // Close the socket 
    }
}
}