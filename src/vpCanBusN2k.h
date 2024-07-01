// vpCanBusN2k.h
#ifndef VPCANBUSN2K_H
#define VPCANBUSN2K_H

#include "config.h"  // Include the configuration header

void InitializeCAN();
void InitializeNMEA2000();
void ReadCANMessages();
void ProcessCANMessage(long unsigned int PGN, unsigned char len, unsigned char* Data);
void CheckSourceAddressChange();
void SayHello();

#endif  // VPCANBUSN2K_H
