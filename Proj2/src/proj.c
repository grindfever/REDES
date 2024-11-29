#include "../include/proj.h"

int parse(char *input, struct URL *url) {
    regex_t reg;
    regcomp(&reg, BAR, 0);
    if (regexec(&reg, input, 0, NULL, 0)) return -1;

    regcomp(&reg, AT, 0);
    if (regexec(&reg, input, 0, NULL, 0) != 0) {
        sscanf(input, HOST_REG, url->host);
        strcpy(url->user, DEFAULT_USER);
        strcpy(url->password, DEFAULT_PASSWORD);
    } else {  // ftp://[<user>:<password>@]<host>/<url-path>
        sscanf(input, HOST_AT_REG, url->host);
        sscanf(input, USER_REG, url->user);
        sscanf(input, PASS_REG, url->password);
    }

    sscanf(input, RESOURCE_REG, url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);

    struct hostent *h;
    if (strlen(url->host) == 0) return -1;
    if ((h = gethostbyname(url->host)) == NULL) {
        printf("Invalid hostname '%s'\n", url->host);
        exit(-1);
    }
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return !(strlen(url->host) && strlen(url->user) && strlen(url->password) && strlen(url->resource) && strlen(url->file));
}

int openSocket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);  
    server_addr.sin_port = htons(port); 
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    
    return sockfd;
}

int checkResponse(const int socket, char* buffer) {

    char byte;
    int index = 0, responseCode;
    ResponseState state = START;
    memset(buffer, 0, MAX_LENGTH);

    while (state != END) {
        
        read(socket, &byte, 1);
        switch (state) {
            case START:
                if (byte == ' ') state = SINGLE;
                else if (byte == '-') state = MULTIPLE;
                else if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case SINGLE:
                if (byte == '\n') state = END;
                else buffer[index++] = byte;
                break;
            case MULTIPLE:
                if (byte == '\n') {
                    memset(buffer, 0, MAX_LENGTH);
                    state = START;
                    index = 0;
                }
                else buffer[index++] = byte;
                break;
            case END:
                break;
            default:
                break;
        }
    }

    sscanf(buffer, RESPCODE_REG, &responseCode);
    return responseCode;
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./proj ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    } 

    struct URL url;
    memset(&url, 0, sizeof(url));
    if (parse(argv[1], &url) != 0) {
        printf("Parse error. Usage: ./proj ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }
    
    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", url.host, url.resource, url.file, url.user, url.password, url.ip);

    char answer[MAX_LENGTH];
    int socketA = createSocket(url.ip, FTP_PORT);
    if (socketA < 0 || checkResponse(socketA, answer) != SV_READY4AUTH) {
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }
    //authconn passivemode request resource get resource closeconnection
    char userCommand[100], passCommand[100];
    sprintf(userCommand, "USER %s\n", url.user);
    sprintf(passCommand, "PASS %s\n", url.password);

    write(socketA, userCommand, strlen(userCommand));
    if (checkResponse(socketA, answer) != SV_READY4PASS) {
        printf("Unknown user '%s'. Abort.\n", url.user);
        exit(-1);
    }

    write(socketA, passCommand, strlen(passCommand));
    if (checkResponse(socketA, answer) != SV_LOGINSUCCESS) {
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }
    
    write(socketA, "pasv\n", 5);
    if (checkResponse(socketA, answer) != SV_PASSIVE) {
        printf("Passive fail\n");
        exit(-1);
    }

    int ip1, ip2, ip3, ip4, port1, port2, port;
    char ip[MAX_LENGTH];
    sscanf(answer, PASSIVE_REG, &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    port = port1 * 256 + port2;
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    

    int socketB = createSocket(ip, port);
    if (socketB < 0) {
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    char fileCommand[107];
    sprintf(fileCommand, "RETR %s\n", url.resource);
    write(socketA, fileCommand, strlen(fileCommand));
    if (checkResponse(socketA, answer) != SV_READY4TRANSFER) {
        printf("Unknown resource '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    FILE *fd = fopen(url.file, "wb");
    if (fd == NULL) {
        printf("Error on open/create file '%s'\n", url.file);
        exit(-1);
    }
    char buffer[MAX_LENGTH];
    int bytes;
    //writing 1 byte at a time
    do {
        bytes = read(socketB, buffer, MAX_LENGTH);
        fwrite(buffer, 1, bytes, fd);
    } while (bytes > 0);
    fclose(fd);
    
    int status = checkResponse(socketA, buffer);
    if (status != SV_TRANSFER_COMPLETE) {
    printf("Transfer status error: %d\n", status);
    return -1;
    }

    write(socketA, "QUIT\n", 5);
    if(checkResponse(socketA, answer) != SV_GOODBYE){
        printf("SV_GOODBYE ERROR");
        return -1;
    }
    close(socketA);
    close(socketB);

    return 0;
}
