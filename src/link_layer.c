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
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received
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
    debugs("entering switch");
    switch (connectionParameters.role) {

        case LlTx: {
            debugs("LLTX-");
            (void) signal(SIGALRM, alarmHandler); //when timer of alarm ends calls alarmHandler->alarmCount++
            while(connectionParameters.nRetransmissions!=0 && linkstate!=STOP_READ){
                printf("%x|%x|%d",A_TR,SET,fd);
                fflush(stdout);
                if(sendSUFrame(A_TR,SET,fd)==-1)return -1;
                debugs("sendframe correct");
                frames_sent++; 
                alarm(connectionParameters.timeout);
                alarmTriggered=FALSE;
                //UA acknowledgment if linkstate reaches STOP_READ if alarm triggers then nRetransmissions--
                while(alarmTriggered==FALSE && linkstate!=STOP_READ){
                    debugs("while alarmtrigger");
                    if (read(fd, &byte, 1)){ 
                        debugs("read");

                        switch(linkstate){
                            case START:
                                debugs("START");
                                if(byte==FLAG)linkstate=FLAG_OK;
                                break;
                            case FLAG_OK:
                                debugs("FLAGOK");
                                if(byte==A_RT)linkstate=A_OK;
                                else if(byte != FLAG)linkstate=START; 
                                break;
                            case A_OK:
                                debugs("AOK");
                                if(byte==UA)linkstate=C_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;
                            case C_OK:
                                debugs("COK");
                                if(byte==(UA^A_RT))linkstate=BCC1_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;   
                            case BCC1_OK:
                                debugs("BCC1OK");
                                if(byte==FLAG)linkstate=STOP_READ;
                                else linkstate=START;
                                break;        
                            default:
                            debugs("ERROR-DEFAULT AT LLOPEN "); 
                            break;

                        }
                    }
                }
                 connectionParameters.nRetransmissions--;    
            }
            debugs("UA LOOP DONE");
            if(linkstate!=STOP_READ)return -1;
            debugs("received ua");
            break;
           
        }
        case LlRx: {
            debugs("LLRX");
            //Receiving SET from transmiter,gets stuck otherwise
            while(linkstate!=STOP_READ){
                int x=read(fd,&byte,1);
                if(x>0){
                   switch(linkstate){
                        case START:
                            if(byte==FLAG)linkstate=FLAG_OK;
                                break;
                        case FLAG_OK:
                            if(byte==A_TR)linkstate=A_OK;
                                else if(byte != FLAG)linkstate=START; 
                                break;
                        case A_OK:
                            if(byte==SET)linkstate=C_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;
                        case C_OK:
                            if(byte==(SET^A_RT))linkstate=BCC1_OK;
                                else if(byte==FLAG)linkstate=FLAG_OK;    
                                else linkstate=START;
                                break;   
                        case BCC1_OK:
                            if(byte==FLAG)linkstate=STOP_READ;
                                else linkstate=START;
                                break;        
                        default:
                            debugs("error: entered default on LLOPEN-LLRX");
                            return -1;
                            break;

                   } 
                }
                else if (x==0)break;
                else {
                    debugs("ERROR on LLRX LLOPEN");
                    return -1;
                }
            }
            if(sendSUFrame(A_RT,UA,fd)==-1)return -1;
            frames_sent++; 
            debugs("sent ua");
            break;
        }
        default:{
            debugs("default-wrong role");
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
                        else if(byte!=FLAG)linkstate=START;
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
                        if(byte==(byteRet^A_RT))linkstate=BCC1_OK;
                        else if(byte==FLAG)linkstate=FLAG_OK;
                        else linkstate=START;
                        break;
                    case BCC1_OK:
                        if(byte==FLAG)linkstate=STOP_READ;
                        else linkstate=START;
                        break;          
                    default:
                        
                        debugs("ERROR ON LLWRITE ENTERED DEFAULT");
                        break;
                }
            }
            if(byteRet==RR(0)||byteRet==RR(1)){
                if(transferT==0)transferT=1;
                else if(transferT==1)transferT=0;
                rr=TRUE;
                }
            if(byteRet==REJ(0)||byteRet==REJ(1)) rej=TRUE;
        }
        transmission++;
    }    
    free(frame);
    if (rr)return frameSize;
    else{
            llclose(fd,TRUE);
            return -1;
        } 
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
//packet: array of characters read
//return array length/number of characters read), -1 if error
int llread(int fd,unsigned char *packet)
{
    // TODO
    unsigned char byte=0,cbyte;
    unsigned char read_bcc2,bcc2;
    int i=0;
    LinkLayerState linkstate=START;
    
    while(linkstate!=STOP_READ){
        int x=read(fd,&byte,1);
         printf("byte-%x",byte);
                    fflush(stdout);
        if (x>0){
            switch(linkstate){
                case START:
                    if(byte==FLAG)linkstate=FLAG_OK;
                    break;
                case FLAG_OK:
                    if(byte==A_TR)linkstate=A_OK;    
                    else if (byte != FLAG) linkstate=START;
                    break;
                case A_OK:
                    if(byte==C_N(0)||byte==C_N(1)){
                        linkstate=C_OK;
                        cbyte=byte;
                        }
                    else if(byte==FLAG)linkstate=FLAG_OK;
                    else if(byte==DISC){
                        if(sendSUFrame(A_RT,DISC,fd)==-1)return -1;
                        frames_sent++; 
                        return 0;
                        }
                    else linkstate=START;
                    break;    
                case C_OK:
                    if (byte==(cbyte^A_TR))linkstate=READ_DATA;
                    else if(byte==FLAG)linkstate=FLAG_OK;
                    else linkstate=START;
                    break;   
                case READ_DATA:
                    if(byte==ESCAPE){
                        if(read(fd,&byte,1)){
                            if(byte==(ESCAPE^0X20)||byte==(FLAG^0x20)){
                                packet[i++]=byte^0x20;//removing the xor to get original byte
                            } 
                            else {
                                debugs("\n Retransmission:READ AFTER ESCAPE ERROR");
                                return -1;
                            }
                        }
                    }
                    else if(byte==FLAG){ //end of DATA field ->calculate bcc2->compare bcc2
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
                            if(sendSUFrame(A_RT,RR(transferR),fd)==-1)return -1;
                            frames_sent++; 
                            if(transferR==0)transferR=1;    
                            else if (transferR==1)transferR=0;
                            return i;
                        }
                        else{
                            debugs("\n RETRANSMISSION:(REJ)BCC2 NOT MATCHED");
                            if(sendSUFrame(A_RT,REJ(transferR),fd)==-1)return -1;
                            frames_sent++; 
                        }  
                    }
                    else{//DATA BYTE
                        packet[i++]=byte;
                    }
                    break;
                default:
                    debugs("\n ERROR ON LLREAD ENTERED DEFAULT");
                    return -1;
                    break;    
            }
        }
        else if(x==0)break;
        else{
            debugs("error llread x");
            return -1;
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
//T---------R//
//DISC->     //
//     <-DISC//
//UA->       // 
int llclose(int fd,int showStatistics)
{
    // TODO
    LinkLayerState linkstate=START;
    unsigned char byte;
    int transmission=0;
   
    (void) signal(SIGALRM, alarmHandler);
    while(transmission<retransmissions && linkstate!=STOP_READ){
        if(sendSUFrame(A_TR,DISC,fd)==-1)return -1; //Transmiter sends receiver a DISC frame 
        frames_sent++; 
        alarm(timeout);
        alarmTriggered=FALSE;
        //Check if frame is a DISC from the Receiver to the Transmiter
        //(in llread(),Receiver sends Transmiter DISC)
        while(alarmTriggered==FALSE && linkstate!=STOP_READ){
            if(read(fd,&byte,1)){
                switch(linkstate){
                    case START:
                        if(byte==FLAG)linkstate=FLAG_OK;
                        break;
                    case FLAG_OK:
                        if(byte==A_RT)linkstate=A_OK;
                        else if(byte!=FLAG)linkstate=START;
                        break;
                    case A_OK:
                        if(byte==DISC)linkstate=C_OK;      
                        else if(byte==FLAG)linkstate=FLAG_OK;
                        else linkstate=START;
                        break;
                    case C_OK:
                        if(byte==(DISC^A_RT))linkstate=BCC1_OK;
                        else if(byte==FLAG)linkstate=FLAG_OK;
                        else linkstate=START;
                        break;
                    case BCC1_OK:
                        if(byte==FLAG)linkstate=STOP_READ;
                        else linkstate=START;
                        break;
                    default:
                        debugs("\n ERROR ON LLCLOSE ENTERED DEFAULT");
                        return -1;
                        break;                    
                }
            }
        }
        transmission++;
    }
    if(sendSUFrame(A_TR, UA,fd)==-1)return -1; // if it was a DISC from Receiver then Transmiter sends UA to Receiver
    frames_sent++; 
    if (linkstate != STOP_READ){
        debugs("EXHAUSTED ALL RETRANSMISSIONS");
        return -1;
    }else if(showStatistics){
        printf("\n Number of Retransmissions : %d",transmission);
        fflush(stdout);
        printf("\n Number of Timeouts : %d",alarmCount);
        fflush(stdout);
        printf("\n Number of Frames Sent : %d",frames_sent);
        fflush(stdout);
    }
    if(closeSerialPort()>-1) return 1;
    else {
        debugs("ERROR:closeSerialPort returned -1");
        return -1;
        }
}

int sendSUFrame( unsigned char A, unsigned char C,int fd){
    unsigned char bcc1=A^C;
    unsigned char frame[5] = {FLAG, A, C, bcc1, FLAG};
    return write(fd,frame, 5);
}
void alarmHandler(int signal) {
    alarmTriggered = TRUE;
    alarmCount++;
}
void debugs(char* string){
    printf("\n %s",string);
    fflush(stdout);
}