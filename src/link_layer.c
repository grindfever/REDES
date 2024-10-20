// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


int timeout = 0;
int retransmissions = 0;
int alarmTriggered = FALSE;
int alarmCount = 0;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){

    if (openSerialPort(connectionParameters.serialPort,connectionParameters.baudRate) < 0)return -1;
    // TODO
    LinkLayerState linkstate=START;
    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    unsigned char byte;

    int fd = connection(connectionParameters.serialPort);
    if (fd < 0) return -1;

    switch (connectionParameters.role) {

        case LlTx: {
            (void) signal(SIGALRM, alarmHandler); //when timer of alarm ends calls alarmHandler->alarmCount++
            while(connectionParameters.nRetransmissions!=0 && linkstate!=STOP_READ){
                sendSUFrame(fd,A_TR,SET);
                alarm(connectionParameters.timeout);
                alarmTriggered=FALSE;
                //UA acknowledgment if linkstate reaches STOP_READ if alarm triggers then nRetransmissions--
                while(alarmTriggered==FALSE && linkstate!=STOP_READ){
                    if (read(fd, &byte, 1)){ 
                        switch(linkstate){
                            case START:
                                if(byte==FLAG)linkstate=FLAG_OK;
                                break;
                            case FLAG_OK:
                                if(byte==A_RT)linkstate=A_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;
                            case A_OK:
                                if(byte==UA)linkstate=C_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;
                            case C_OK:
                                if(byte==A_RT^UA)linkstate=BCC1_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;   
                            case BCC1_OK:
                                if(byte==FLAG)linkstate=STOP_READ;
                                else linkstate=START;
                                break;        
                            default:
                            break;

                        }
                    }
                }
                 connectionParameters.nRetransmissions--;    
            }
            if(linkstate!=STOP_READ)return -1;
            printf("DEBUG:RECEIVED UA");
            break;
           
        }
        case LlRx: {
            //Receiving SET from transmiter,gets stuck otherwise
            while(linkstate!=STOP_READ){
                if(read(fd,&byte,1)){
                   switch(linkstate){
                        case START:
                            if(byte==FLAG)linkstate=FLAG_OK;
                                break;
                        case FLAG_OK:
                            if(byte==A_TR)linkstate=A_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;
                        case A_OK:
                            if(byte==SET)linkstate=C_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;
                        case C_OK:
                            if(byte==A_TR^SET)linkstate=BCC1_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;   
                        case BCC1_OK:
                            if(byte==FLAG)linkstate=STOP_READ;
                                else linkstate=START;
                                break;        
                        default:
                            break;

                   } 
                }
            }
            sendSUFrame(fd,A_RT,UA);
            printf("DEBUG:SENT UA");
            break;
        }
        default:{
            return -1;
            break;
        }
 
    }
   return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
int connection(const char *serialPort) {

    int fd = open(serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPort);
        return -1; 
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received
    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }

    return fd;
}
int sendSUFrame(int fd, unsigned char A, unsigned char C){
    unsigned char frame[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd,frame, 5);
}
void alarmHandler(int signal) {
    alarmTriggered = TRUE;
    alarmCount++;
}
