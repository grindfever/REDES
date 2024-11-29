#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <termios.h>

#define MAX_LENGTH  500
#define FTP_PORT    21

#define SV_READY4AUTH           220
#define SV_READY4PASS           331
#define SV_LOGINSUCCESS         230
#define SV_PASSIVE              227
#define SV_READY4TRANSFER       150
#define SV_TRANSFER_COMPLETE    226
#define SV_GOODBYE              221

#define AT            "@"
#define BAR           "/"
#define HOST_REG      "%*[^/]//%[^/]"
#define HOST_AT_REG   "%*[^/]//%*[^@]@%[^/]"
#define RESOURCE_REG  "%*[^/]//%*[^/]/%s"
#define USER_REG      "%*[^/]//%[^:/]"
#define PASS_REG      "%*[^/]//%*[^:]:%[^@\n$]"
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

/* Machine states that receives the response from the server */
typedef enum {
    START,
    SINGLE,
    MULTIPLE,
    END
} ResponseState;



/* 
* Parser that transforms user input in url parameters
* @param input, a string containing the user input
* @param url, a struct that will be filled with the url parameters
* @return 0 if there is no parse error or -1 otherwise
*/
int parse(char *input, struct URL *url);

/* 
* Opens a socket file descriptor based on given server ip and port
* @param ip, a string containing the server ip
* @param port, an integer value containing the server port
* @return socket file descriptor if there is no error or -1 otherwise
*/
int openSocket(char *ip, int port);


/* 
* Read server response
* @param socket, server connection file descriptor
* @param buffer, string that will be filled with server response
* @return server response code obtained by the operation
*/
int checkResponse(const int socket, char *buffer);

/* 
* Request resource
* @param socket, server connection file descriptor
* @param resource, string that contains the desired resource
* @return server response code obtained by the operation
*/
int requestResource(const int socket, char *resource);

/* 
* Get resource from server and download it in current directory
* @param socketA, server connection file descriptor
* @param socketB, server connection file descriptor
* @param filename, string that contains the desired file name
* @return server response code obtained by the operation
*/
int getResource(const int socketA, const int socketB, char *filename);

/* 
* Closes the server connection and the socket itself
* @param socketA, server connection file descriptor
* @param socketB, server connection file descriptor
* @return 0 if there is no close error or -1 otherwise
*/
int closeConnection(const int socketA, const int socketB);