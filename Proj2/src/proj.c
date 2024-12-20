#include "../include/proj.h"

int parse(char *input, struct URL *url) {
    // Initialize the URL structure
    memset(url, 0, sizeof(struct URL));
    if (strncmp(input, "ftp://", 6) == 0) {
        input += 6;  // Skip the "ftp://"
    }
    char *atSymbol = strchr(input, '@');//check for @ meaning there is user&password
    if (atSymbol != NULL) {
        *atSymbol = '\0';  // Null-terminate before @
        // Extract user and password (if present)
        if (sscanf(input, "%507[^:]:%507s", url->user, url->password) != 2) { //regex-read all untill : then read again
            printf("Error: Invalid user:password format.\n");
            return -1;
        }
        
        // Now extract host and resource after @
        if (sscanf(atSymbol + 1, "%507[^/]/%s", url->host, url->resource) != 2) {
            printf("Error: Invalid host/resource format.\n");
            return -1;
        }
    } else {
        // No @ -> no user/password
        if (sscanf(input, "%507[^/]/%s", url->host, url->resource) != 2) {
            printf("Error: Invalid host/resource format.\n");
            return -1;
        }
        strcpy(url->user, "anonymous");
        strcpy(url->password, "password");
    }

    // Extract filename from resource path
    char *fileName = strrchr(url->resource, '/');
    if (fileName != NULL) {
        strcpy(url->file, fileName + 1);  // +1 to skip the / 
    } else {
        strcpy(url->file, url->resource);  // If no / the resource is the file
    }

    // Get Ip address
    struct hostent *h;
    if (strlen(url->host) == 0) {
        printf("Error: No host specified.\n");
        return -1;
    }
    if ((h = gethostbyname(url->host)) == NULL) {
        printf("Error: Invalid hostname '%s'.\n", url->host);
        return -1;
    }
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}



int openSocket(char *ip, int port) {
    struct sockaddr_in server_addr;
    int sockfd;
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);  //server ip
    server_addr.sin_port = htons(port); //serverport

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
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
            case MULTIPLE://multiple lines have "-" before the response
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
    //url fields extracted on parse() 
    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", 
        url.host, url.resource, url.file, url.user, url.password, url.ip);

    char answer[MAX_LENGTH];
    int socketA = openSocket(url.ip, FTP);
    if (socketA < 0 || checkResponse(socketA, answer) != SV_AUTH) { //SV_AUTH 220
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP);
        exit(-1);
    }

    char userCommand[MAX_LENGTH+8], passCommand[MAX_LENGTH+8];
    sprintf(userCommand, "USER %s\n", url.user);
    sprintf(passCommand, "PASS %s\n", url.password);

    write(socketA, userCommand, strlen(userCommand)); //Write user to server
    if (checkResponse(socketA, answer) != SV_READYPASS) {//check if ready for password SV_READYPASS 331
        printf("Unknown user '%s'. Abort.\n", url.user);
        exit(-1);
    }

    write(socketA, passCommand, strlen(passCommand));//Write password to server
    if (checkResponse(socketA, answer) != SV_LOGINSUCCESS) { //check if login successfull SV_LOGINSUCCESS 230
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.password);
        exit(-1);
    }

    write(socketA, "pasv\n", 5); //Write pasv command to server
    if (checkResponse(socketA, answer) != SV_PASSIVE) {//Check if server is entering passive mode SV_PASSIVE 227
        printf("Passive fail\n");
        exit(-1);
    }

    int ip1, ip2, ip3, ip4, port1, port2;
    char ip[MAX_LENGTH];
    if (sscanf(answer, PASSIVE_REG, &ip1, &ip2, &ip3, &ip4, &port1, &port2) == 6) {
        int port = port1 * 256 + port2;
        sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
        // Connect to the passive mode IP and port
        int socketB = openSocket(ip, port);
        if (socketB < 0) {
            printf("Error: Could not establish connection to passive mode IP %s, Port: %d\n", ip, port);
            return -1;
        }

        // Request the file
        char fileCommand[MAX_LENGTH+8];
        sprintf(fileCommand, "RETR %s\n", url.resource);
        write(socketA, fileCommand, strlen(fileCommand));//write which resource we want with RETR 
        if (checkResponse(socketA, answer) != SV_READYTOTRANSFER) {//check if ready to transfer 150
            printf("Unknown resource '%s' in '%s:%d'\n", url.resource, ip, port);
            exit(-1);
        }

        FILE *fd = fopen(url.file, "wb");
        if (fd == NULL) {
            printf("Error on open/create file '%s'\n", url.file);
            exit(-1);
        }

        // Write data from socketB to file
        char buffer[MAX_LENGTH];
        int bytes;
        do {
            bytes = read(socketB, buffer, MAX_LENGTH); //Read from server socket to buffer
            fwrite(buffer, 1, bytes, fd); //write buffer to file
        } while (bytes > 0);
        fclose(fd);

        // Check transfer completion status
        int status = checkResponse(socketA, buffer);
        if (status != SV_TRANSFER_COMPLETE) {//check if transfer complete on server SV....226
            printf("Transfer status error: %d\n", status);
            return -1;
        }

        // Close the connection
        write(socketA, "QUIT\n", 5);//Write QUIT to server
        if (checkResponse(socketA, answer) != SV_GOODBYE) { //check for server goodbye 221
            printf("SV_GOODBYE ERROR");
            return -1;
        }
        close(socketA);
        close(socketB);
    } else {
        // Error occurred if parsing passive mode information fails
        printf("Error: Passive mode format invalid.\n");
        exit(-1);
    }

    return 0;
}
