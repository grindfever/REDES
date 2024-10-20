// Application layer protocol implementation


#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,int nTries, int timeout, const char *filename){   
    LinkLayer linklayer;
    if (role=="tx") linklayer.role = LlRx;
    else linklayer.role = LlTx;
    linklayer.nRetransmissions = nTries;
    linklayer.baudRate = baudRate;
    linklayer.timeout = timeout;
    stcpy(linklayer.serialPort,serialPort);

    int fd=llopen(linklayer);
    if (fd < 0) {
        perror("Connection error\n");
        exit(-1);
    }
    switch (linklayer.role) {

        case LlTx: {//Transmiter

        }
        case LlRx: {//Receiver
            
        }
        default: {
            exit(-1);
            break;
        }
}
}
