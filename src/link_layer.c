// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


int timeout = 0;
int retransmissions = 0;
int alarmTriggered = FALSE;
int alarmCount = 0;
int transferT=0;
int transferR=1;
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){

    if (openSerialPort(connectionParameters.serialPort,connectionParameters.baudRate) < 0)return -1;
    // TODO
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(connectionParameters.serialPort);
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
    //opened serialport for connection 
    //now we will send SET as the Transmiter and wait for UA from receiver and we will check this SET as the Receiver and send to the transmitter the UA

    LinkLayerState linkstate=START;
    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    unsigned char byte;
    
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
////////////////////////////////////////////////~
//buf: array of characters to transmit
//bufSize: length of the characters array
//return number of writen characters,-1 if error
int llwrite(int fd, const unsigned char *buf, int bufSize)
{   
    // TODO
    int frameSize = 6 + bufSize; //+6 for the FLAG|A|C|BCC1|(D1-DN)|BCC2|FLAG BYTES 
    unsigned char *frame = (unsigned char *) malloc(frameSize);//dynamic because of variable DATA and Byte stuffing
    frame[0]=FLAG;
    frame[1]=A_TR;
    frame[2]=C_N(transferT);
    frame[3]=A_TR^C_N(transferT);
    for (int i = 0; i < bufSize; i++) {
        frame[4 + i] = buf[i];   //D1 to DN
    }

    unsigned char bcc2 = buf[0];
    for (int i = 1; i < bufSize; i++) {
        bcc2 ^= buf[i];  // BCC2=D1^D2^D3.....^DN
    }
    //byte stuffing 
    //if FLAG is found on buf replace with ESCAPE then FLAG^0X20
    //if ESCAPE is found on buf replace with ESCAPE then ESCAPE^0X20
    int stuffedi = 4;

    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE) {
            frame = realloc(frame, ++frameSize); 
            frame[stuffedi++] = ESCAPE;         
            frame[stuffedi++] = buf[i] ^ 0x20; 
        } else {
        frame[stuffedi++] = buf[i];
        }
    }
    frame[stuffedi++] = bcc2;
    frame[stuffedi++] = FLAG;

    int rr;
    int rej;
    int transmission=0;
    LinkLayerState linkstate = START;
    unsigned char byte,byteRet;
    //each transmission writes frame to receiver and reads the acknowledgment from the receiver
    while(transmission<retransmissions){
        rr= 0;
        rej = 0;
        alarmTriggered = FALSE;
        alarm(timeout);
        while (alarmTriggered == FALSE && !rr && !rej) {
            write(fd,frame,stuffedi);
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
                        if(byte==SET||byte==UA||byte==RR(0)||byte==RR(1)||byte==REJ(0)||byte==REJ(1)||byte==DISC){
                            linkstate=C_OK;
                            byteRet=byte;
                        }
                        else if(byte==FLAG)linkstate=FLAG_OK;
                        else linkstate=START;
                        break;
                    case C_OK:
                        if(byte==A_RT^byteRet)linkstate=BCC1_OK;
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
            if(byteRet==RR(0)||byteRet==RR(1)){
                rr=TRUE;
                transferT=(transferT+1)%2;
                }
            if(byteRet==REJ(0)||byteRet==REJ(1)) rej=TRUE;
        }
        transmission++;
    }    
    free(frame);
    if (rr)return frameSize;
    else return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
//packet: array of characters read
//return array length/number of characters read), -1 if error
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
