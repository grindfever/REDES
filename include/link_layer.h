// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <fcntl.h>     
#include <unistd.h>    
#include <stdio.h>     
#include <stdlib.h>    
#include <string.h>    
#include <termios.h> 
#include <sys/types.h>
#include <sys/stat.h>  
#include <signal.h>
#include <time.h>


#define FLAG 0x7E
#define ESCAPE 0x7D
#define SET  0x03   
#define UA   0x07   
#define A_TR 0x03 //TRANSMITER TO RECEIVER 
#define A_RT 0x01 //RECEIVER TO TRANSMITER
#define RR(n) (0xAA + (n))  // Receiver Ready for I-frame n ,n=[0,1]
#define REJ(n) (0x54 + (n)) // Receiver didn't receive I-frame n,n=[0,1] 
#define DISC 0x0B
//Information frames
//BCC2=D1^D2^D3^D4^....^DN
#define C_N(n) ((n & 0x01) << 7) //C_N(1)=0X80 C_N(0)=0x00

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source
#define MAX_PAYLOAD_SIZE 1000
// MISC
#define FALSE 0
#define TRUE 1

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct  {
    char serialPort[50]; /*Device /dev/ttySx, x = 0, 1*/
    LinkLayerRole role; /*TRANSMITTER | RECEIVER*/
    int baudRate; /*Speed of the transmission*/
    int nRetransmissions; /*Number of retries in case of failure*/
    int timeout; /*Timer value: 1 s*/
}LinkLayer;

typedef enum{
    START,
    FLAG_OK,
    A_OK,
    C_OK,
    BCC1_OK,
    STOP_READ
}LinkLayerState;


// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);
//Opens connection with serialport
//Returns 1=success -1=error
int connection(const char *serialPort);
//Sends supervision/unumbered frame to serial port fd 
//Frame format: |FLAG|A|C|BCC1|FLAG|
int sendSUFrame(int fd, unsigned char A, unsigned char C);
// Counts timeouts
void alarmHandler(int signal);

#endif // _LINK_LAYER_H_
