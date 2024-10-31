// Application layer protocol implementation


#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <math.h>

void applicationLayer(const char *serialPort, const char *role,const int baudRate,int nTries, int timeout, const char *filename){   
    LinkLayer linklayer;
    if (strcmp(role, "tx")) linklayer.role = LlRx;
    else {linklayer.role = LlTx;}
    linklayer.nRetransmissions = nTries;
    linklayer.baudRate = baudRate;
    linklayer.timeout = timeout;
    strcpy(linklayer.serialPort,serialPort); 
    debugs("LLOPEN-");
    int fd=llopen(linklayer);
    if (fd < 0) {
        printf("Connection error\n");
        fflush(stdout); 
        exit(-1);
    }
    debugs("END LLOPEN");
    debugs("ENTER SWITCH ROLE");
    switch (linklayer.role) {
        /*  Transmiter:
            Open the file to be transmitted.
            Split the file into packets, send each packet using llwrite.
            Send a control packet to indicate the start and end of the file transmission.
            Close the file and the link (llclose). */
        case LlTx: {
            debugs("LLTX");
            FILE *file =fopen(filename,"rb");
            if(file==NULL){
                printf("ERROR:File not found ");
                exit(-1);
            } 
            debugs("FILE OPENED");
            //get file size
            int fstart=ftell(file);
            fseek(file,0,SEEK_END);
            int file_size=ftell(file)-fstart;
            fseek(file,fstart,SEEK_SET);
            //start controlpacket
            unsigned int control_packet_size;               //c=1->start controlpacket
            unsigned char *controlPacketStart = get_controlPacket(1, filename, file_size, &control_packet_size);
            if(llwrite(fd, controlPacketStart, control_packet_size) < 0){ 
                debugs("\n Exit:writing start packet\n");
                exit(-1);
            }
            debugs("start packet SENT");

            unsigned char* data=(unsigned char*)malloc(sizeof(unsigned char)* file_size);
            fread(data,sizeof(unsigned char),file_size,file);//reads from file to data file_size bytes/characters

            unsigned char s=0;    
            int bytes_left=file_size;//bytes left to read
            int data_size;//data size per packet 
          

            //data distribution by packets 
            debugs("SENDING DATAPACKETS");
            while(bytes_left>=0){ 
                data_size=bytes_left;
                if(data_size>MAX_PAYLOAD_SIZE)data_size=MAX_PAYLOAD_SIZE;
                unsigned char* packet_data = (unsigned char*) malloc(data_size);
                for (int i = 0; i < data_size; i++) {
                    packet_data[i] = data[i];
                }
                int data_packet_size=4+data_size; //C|S|L1|L2|DATA ->1|1|1|1|DATA_SIZE
                unsigned char* datapacket = (unsigned char*)malloc(data_packet_size);
                datapacket[0] = 2;  //c=2->datapacket
                datapacket[1] = s;
                datapacket[2] = data_size >> 8 & 0xFF; //L2=MSB of data_size
                datapacket[3] = data_size & 0xFF;      //L1=LSB of data_size
                for (int i = 0; i < data_size; i++) {
                    datapacket[i + 4] = packet_data[i]; 
                }   

                if(llwrite(fd, datapacket, data_packet_size) < 0) {
                    debugs("Exit:applayer-writing data packets\n");
                    exit(-1);
                }
                bytes_left -= MAX_PAYLOAD_SIZE; 
                data+=data_size; //
                s=(s+1)%99;
            }          
            debugs("get end packet");                                //c=3->end control packet
            unsigned char *controlPacketEnd = get_controlPacket(3, filename, file_size, &control_packet_size);
            if(llwrite(fd, controlPacketEnd, control_packet_size) < 0) {  
                debugs("Exit: error in end packet");
                exit(-1);
            }
            debugs("SENT END PACKET"); 
            debugs("LLCLOSE"); 
            llclose(fd,TRUE);
            break;


        }
        /*  Receiver:
            Receive data packets using llread.
            Reconstruct the file by writing received packets to a file.
            Close the file and the link (llclose).*/
        case LlRx: {  
            debugs("llRX");
            unsigned char *packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
            int packet_size = -1;
            debugs("read packets:  ");
            while (packet_size < 0) {
                packet_size = llread(fd, packet);
            }
            debugs("done packets");
            //control start packet
            //file size & filename extraction
            unsigned int rfile_size=0;
            unsigned char* name = readCPacket(packet, packet_size, &rfile_size); 
            debugs("startpacketread");
            FILE* new_file = fopen((char *) name, "wb+");//file where we will copy the packets            
            packet_size = -1; 
            //data packet loop
            debugs("READING DATAPACKETS:");
          while (TRUE) {
    // Read the packet
    while ((packet_size = llread(fd, packet)) < 0);

    if (packet[0] == 3) { // Control End packet
        break; // Exit loop on end packet
    } else if (packet[0] == 2) { // Data packet
        // Allocate buffer for the data
        unsigned char *buffer = (unsigned char *)malloc(packet_size - 4);
        if (buffer == NULL) {
            perror("malloc failed");
            break; // Exit loop if memory allocation fails
        }
        
        // Copy data into buffer
        for (unsigned int i = 0; i < packet_size - 4; i++) {
            buffer[i] = packet[i + 4];
        }

        // Write data to the file
        fwrite(buffer, sizeof(unsigned char), packet_size - 4, new_file);

        // Free the allocated buffer
        free(buffer);
    }
}

            debugs("ALL DATA READ");

            free(packet);
            free(name);
            fclose(new_file);
            break;
        }
        default: {
            debugs("ERROR:entered default on applayer as receiver");
            exit(-1);
            break;
        }
}
}

unsigned char * get_controlPacket(const unsigned int c, const char* file_name, long int file_size, unsigned int* size){

    const int L1 = (int) ceil(log2f((float)file_size)/8.0);
    const int L2 = strlen(file_name);
    *size = 3+L1+2+L2;
    unsigned char *packet = (unsigned char*)malloc(*size);
    
    unsigned int pos = 0;
    packet[pos++]=c;
    packet[pos++]=0; //t=0->file size
    packet[pos++]=L1;

    for (unsigned char i = 0 ; i < L1 ; i++) {
        packet[2+L1-i] = file_size & 0xFF;//geting 1 byte from length
        file_size >>= 8; //getting the next byte
    }
    pos+=L1;
    packet[pos++]=1; //t=1->file name
    packet[pos++]=L2;

    for (unsigned int i = 0; i < L2; i++) {
    packet[pos + i] = file_name[i];
    }
    return packet;
}

// C|T1|L1|V1|T2|L2|V2
unsigned char* readCPacket(unsigned char* packet, int size, unsigned int *file_size) {
    // File Size
    unsigned char l1 = packet[2];
    unsigned char l2l1extract[l1]; 
    *file_size = 0; 
    for (unsigned int i = 0; i < l1; i++) {
        l2l1extract[i] = packet[3 + i];
    }
    for (unsigned int i = 0; i < l1; i++) {
        *file_size |= (l2l1extract[l1 - i - 1] << (8 * i));
    }
    // File Name
    unsigned char file_name_size = packet[3 + l1 + 1]; 
    unsigned char *name = (unsigned char*)malloc(file_name_size); 

    for (unsigned int i = 0; i < file_name_size; i++) {
        name[i] = packet[3 + l1 + 2 + i];
    }

    return name; 
}
