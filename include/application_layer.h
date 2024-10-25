// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_
#include "link_layer.h"
#include <math.h>
// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

//L1 - bytes to store file size
//L2 - bytes to store file name
//    packet = C|T1|L1|V1|T2|L2|V2
// packetsize= 1|1 |1 |L1|1 |1 |L2                      
unsigned char * get_controlPacket(const unsigned int c, const char* file_name, long int file_size, unsigned int* size);

//returns file name and modifies file_size to number of bytes read
unsigned char* readCPacket(unsigned char* packet, int size, unsigned int *file_size);
#endif // _APPLICATION_LAYER_H_
