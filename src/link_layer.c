// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int frames_sent=0;
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
                frames_sent++; 
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
            printf("\n DEBUG:RECEIVED UA");
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
            frames_sent++; 
            printf("\n DEBUG:SENT UA");
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
    unsigned char *frame = (unsigned char *) malloc(frameSize);//dynamic because of variable bufSize and Byte stuffing
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
    //DATA field with byte stuffing
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
    //each transmission writes frame to receiver and reads the acknowledgment(rr or rej) from the receiver
    //we are looking for an rr, if not we need to exhaust all retransmissions and return -1
    while(transmission<retransmissions && !rr){
        rr= 0;
        rej = 0;
        alarmTriggered = FALSE;
        alarm(timeout);
        while (alarmTriggered == FALSE && !rr && !rej) {
            write(fd,frame,stuffedi);
            frames_sent++; 
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
                        printf("ERROR ON LLWRITE ENTERED DEFAULT");
                        break;
                }
            }
            if(byteRet==RR(0)||byteRet==RR(1)){
                transferT=(transferT+1)%2;
                rr=TRUE;
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
int llread(int fd,unsigned char *packet)
{
    // TODO
    unsigned char byte,cbyte;
    unsigned char read_bcc2,bcc2;
    int i=0;
    LinkLayerState linkstate=START;
    
    while(linkstate!=STOP_READ){
        if (read(fd,&byte,1)){
            switch(linkstate){
                case START:
                    if(byte==FLAG)linkstate=FLAG_OK;
                    break;
                case FLAG_OK:
                    if(byte==A_TR)linkstate=A_OK;    
                    else linkstate=START;
                    break;
                case A_OK:
                    if(byte==C_N(0)||byte==C_N(1)){
                        linkstate=C_OK;
                        cbyte=byte;
                        }
                    else if(byte==FLAG)linkstate=FLAG_OK;
                    else if(byte==DISC){
                        sendSUFrame(fd,A_RT,DISC);
                        frames_sent++; 
                        return 0;
                        }
                    else linkstate=START;
                    break;    
                case C_OK:
                    if (byte==A_TR^cbyte)linkstate=READ_DATA;
                    else if(byte==FLAG)linkstate=FLAG_OK;
                    else linkstate=START;
                    break;   
                case READ_DATA:
                    if(byte==ESCAPE){
                        if(read(fd,&byte,1)){
                            if(byte==ESCAPE^0X20||byte==FLAG^0x20){
                                packet[i++]=byte^0x20;//removing the xor to get original byte
                            } 
                            else {
                                printf("\n Retransmission:READ AFTER ESCAPE ERROR");
                                return -1;
                            }
                        }
                    }
                    else if(byte==FLAG){ //end of DATA field ->calculate bcc2->compare bcc2
                        //removing bcc2 from packet
                        read_bcc2=packet[i-1];
                        i--;
                        packet[i]='\0';
                        //calculate bcc2 with data in packet to compare with read_bcc2
                        bcc2=packet[0];
                        for(int j=1;j<i;j++){
                            bcc2^=packet[j];
                        }
                        //bcc2 comparison
                        if(bcc2==read_bcc2){
                            linkstate=STOP_READ;
                            sendSUFrame(fd,A_RT,RR(transferR));
                            frames_sent++; 
                            transferR=(transferR+1)%2;
                            return i;
                        }
                        else{
                            printf("\n RETRANSMISSION:(REJ)BCC2 NOT MATCHED");
                            sendSUFrame(fd,A_RT,REJ(transferR));
                            frames_sent++; 
                        }  
                    }
                    else{//DATA BYTE
                        packet[i++]=byte;
                    }
                    break;
                default:
                    printf("\n ERROR ON LLREAD ENTERED DEFAULT");
                    return -1;
                    break;    
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
//if showStatistics=true print statistics {NUMBER OF TIMEOUTS,FRAMES,RETRANSMISSIONS}
//Positive value in case of success
//Negative value in case of error
int llclose(int fd,int showStatistics)
{
    // TODO
    LinkLayerState linkstate=START;
    unsigned char byte;
    int transmission=0;
   
    (void) signal(SIGALRM, alarmHandler);
    while(transmission<retransmissions && linkstate!=STOP_READ){
        sendSUFrame(fd,A_TR,DISC); //Transmiter sends receiver a DISC frame 
        frames_sent++; 
        alarm(timeout);
        alarmTriggered=FALSE;
        //Check if frame is a DISC from the Receiver to the Transmiter(done in llread()to simplify llclose)
        while(alarmTriggered==FALSE && linkstate!=STOP_READ){
            if(read(fd,&byte,1)){
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
                        if(byte==DISC)linkstate=C_OK;      
                        else if(byte==FLAG)linkstate=FLAG_OK;
                        else linkstate=START;
                        break;
                    case C_OK:
                        if(byte==A_RT^DISC)linkstate=BCC1_OK;
                        else if(byte==FLAG)linkstate=FLAG_OK;
                        else linkstate=START;
                        break;
                    case BCC1_OK:
                        if(byte==FLAG)linkstate=STOP_READ;
                        else linkstate=START;
                        break;
                    default:
                        printf("\n ERROR ON LLCLOSE ENTERED DEFAULT");
                        return -1;
                        break;                    
                }
            }
        }
        transmission++;
    }
    sendSUFrame(fd, A_TR, UA); // if it was a DISC from Receiver then Transmiter sends UA to Receiver
    frames_sent++; 
    if (linkstate != STOP_READ){
        printf("EXHAUSTED ALL RETRANSMISSIONS");
        return -1;
    }else if(showStatistics){
        printf("\n Number of Retransmissions : %d",transmission);
        printf("\n Number of Timeouts : %d",alarmCount);
        printf("\n Number of Frames Sent : %d",frames_sent);
    }
    if(closeSerialPort()>-1) return 1;
    else {
        printf("ERROR:closeSerialPort returned -1");
        return -1;
        }
}

int sendSUFrame(int fd, unsigned char A, unsigned char C){
    unsigned char frame[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd,frame, 5);
}
void alarmHandler(int signal) {
    alarmTriggered = TRUE;
    alarmCount++;
}
