#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <regex.h> 
#define MAX_LENGTH  666
#define FTP    21 

#define SV_AUTH                 220
#define SV_READYPASS            331 
#define SV_LOGINSUCCESS         230 
#define SV_PASSIVE              227 
#define SV_READYTOTRANSFER      150 
#define SV_TRANSFER_COMPLETE    226 
#define SV_GOODBYE              221 

#define RESPCODE_REG  "%d"
#define PASSIVE_REG   "%*[^(](%d,%d,%d,%d,%d,%d)%*[^\n$)]"

#define DEFAULT_USER        "anonymous"
#define DEFAULT_PASSWORD    "password"

/* Parser output */
struct URL {
    char host[MAX_LENGTH];      // 'ftp.up.pt'
    char resource[MAX_LENGTH];  // 'parrot/misc/canary/warrant-canary-0.txt'
    char file[MAX_LENGTH];      // 'warrant-canary-0.txt'
    char user[MAX_LENGTH];      // 'username'
    char password[MAX_LENGTH];  // 'password'
    char ip[MAX_LENGTH];        // 193.137.29.15
};

/* Machine states for receiving the response from the server */
typedef enum {
    START,
    SINGLE,
    MULTIPLE,
    END
} ResponseState;

/* 
* Parser that transforms user input into URL parameters
* @param input, a string containing the user input
* @param url, a struct that will contain the URL parameters
* @return 0 if no parse error or -1 otherwise
*/
int parse(char *input, struct URL *url);

/* 
* Opens a socket file descriptor based on given server IP and port
* @param ip, string containing server IP
* @param port, integer containing the server port
* @return socket file descriptor if no error or -1 otherwise
*/
int openSocket(char *ip, int port);

/* 
* Reads server response
* @param socket, server connection file descriptor
* @param buffer, string that will be filled with server response
* @return server response code obtained by the operation
*/
int checkResponse(const int socket, char *buffer);
