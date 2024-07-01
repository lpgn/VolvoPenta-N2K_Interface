/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// This Sketch is based on code by  Timo Lappalainen https://github.com/ttlappalainen
//                                  Andreas Koritnik https://github.com/AK-Homberger
//                                  Cory J. Fowler   https://github.com/coryjfowler
//                                    
// 20211214 first version to read Volvo Penta MDI J1939 messages (RPM, engine hours, coolant temperature, alternator voltage)
// from VP CANBus, convert it and write it to a N2K bus.
//

#include "vpCanBusN2k.h"

#define ESP32_CAN_TX_PIN GPIO_NUM_26  // Set CAN TX port to 26
#define ESP32_CAN_RX_PIN GPIO_NUM_25  // Set CAN RX port to 25

#include <Preferences.h>
#include <NMEA2000_CAN.h> 
#include <N2kMessages.h>
#include <mcp_can.h>

tNMEA2000 NMEA2000;  // Define the NMEA2000 object here

Preferences preferences;    // Nonvolatile storage on ESP32 - To store LastDeviceAddress

const unsigned long TransmitMessages[] PROGMEM = {127488L, 127489L,0}; // Supported messages for N2K bus

#define CAN0_INT 17                            
MCP_CAN CAN0(5);   

const char Description[] = "Description: Volvo Penta->N2K interface. Read J1939 data from VP canbus, \nconvert it and send it to a N2K bus.\n\n";

int NodeAddress;            // To store last Node Address

extern const char Description[];

// Function to initialize CAN
void InitializeCAN() {
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully");
  } else {
    Serial.println("Error Initializing MCP2515...");
    while (1);
  }
  
  CAN0.setMode(MCP_LISTENONLY);   // Set operation mode to listen only. We don't want to write anything to the VP CAN-Bus
  pinMode(CAN0_INT, INPUT);       // Configuring pin for INT input
}

// Function to initialize NMEA2000
void InitializeNMEA2000() {
  uint8_t chipid[6];
  uint32_t id = 0;

  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  NMEA2000.SetN2kCANSendFrameBufSize(150);

  // Generate unique number from chip id
  esp_efuse_mac_get_default(chipid);
  for (int i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "VolvoPenta-N2K Interface",  // Manufacturer's Model ID
                                 "SW-Vers:  0.9 (2021-12-14)",  // Manufacturer's Software version code
                                 "Mod-Vers: 0.9 (2021-12-14)" // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id, // Unique number. Use e.g. Serial number.
                                160, // Device function=Device that brings information from an engine used for propulsion onto the NMEA 2000 network
                                50,  // Device class=Propulsion
                                2002 // Just chosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  preferences.begin("nvs", false);                          // Open nonvolatile storage (nvs)
  NodeAddress = preferences.getInt("LastNodeAddress", 37);  // Read stored last NodeAddress, default 34
  preferences.end();
  Serial.printf("\nN2K-NodeAddress=%d\n", NodeAddress);

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
  NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, NodeAddress);
  NMEA2000.ExtendTransmitMessages(TransmitMessages);

  NMEA2000.Open();
  delay(200);
}

// Function to read CAN messages
void ReadCANMessages() {
  long unsigned int PGN;
  unsigned char len = 0;
  unsigned char Data[16];

  if (Serial.available()) { // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
    Serial.read();
  }

  if(!digitalRead(CAN0_INT)) { // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&PGN, &len, Data); // Read data: len = data length, Data = data byte(s)
    PGN = (PGN>>8) & 0xFFFF; // Get the PGN
    ProcessCANMessage(PGN, len, Data);
  }
}

// Function to process a CAN message
void ProcessCANMessage(long unsigned int PGN, unsigned char len, unsigned char* Data) {
  tN2kMsg N2kMsg;
  static int SendSTAT = 0;
  double EngineHours, CoolantTemperature, AlternatorVoltage, RPM;

  switch(PGN) {
    case 61444: 
      RPM = (Data[4] * 256.0 + Data[3]) / 8.0; // Get revolutions
      SetN2kPGN127488(N2kMsg, 0, RPM, N2kDoubleNA, N2kInt8NA); // Prepare the datagram
      NMEA2000.SendMsg(N2kMsg); // Send it out
      Serial.printf("RPM: %.1f\n", RPM);
      break;
    case 65253: 
      EngineHours = (Data[0] + Data[1] * 256) / 20; // Get engine hours
      SendSTAT |= 1; // Set status 'engine hours value is available'
      break;
    case 65262: 
      CoolantTemperature = Data[0] - 40; // Get coolant temperature
      SendSTAT |= 2; // Set status 'coolant temperature value is available'
      break;
    case 65271: 
      AlternatorVoltage = (Data[7] * 256.0 + Data[6]) / 20.0; // Get alternator voltage
      SendSTAT |= 4; // Set status 'alternator voltage value is available'
      break;
  }

  if (SendSTAT == 0x07) { // Are the three values available
    SetN2kPGN127489(N2kMsg, 0, N2kDoubleNA, N2kDoubleNA, CToKelvin(CoolantTemperature), AlternatorVoltage, N2kDoubleNA, EngineHours*3600.0, N2kDoubleNA, N2kDoubleNA, N2kInt8NA, N2kInt8NA, 0x00,0x00);
    NMEA2000.SendMsg(N2kMsg);
    Serial.printf("Battery:%.2f Hours: %.2f Temperature: %.2f\n", AlternatorVoltage, EngineHours, CoolantTemperature);
    CoolantTemperature = AlternatorVoltage = EngineHours = 0.0;
    SendSTAT = 0;
  }
}

// Function to check if SourceAddress has changed (due to address conflict on bus)
void CheckSourceAddressChange() {
  int SourceAddress = NMEA2000.GetN2kSource();

  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress; // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }
}

// Function to print some useful information to USB-serial
void SayHello() {
  char Sketch[80], buf[256];
  int i;

  // Get source code filename
  strcpy(buf, __FILE__);
  i = strlen(buf);

  // remove path and suffix
  while (buf[i] != '.' && i >= 0)
    i--;
  buf[i] = '\0';
  while ( buf[i] != '\\' && i >= 0)
    i--;
  i++;
  strcpy(Sketch, buf + i);

  // Sketch date/time of compilation
  sprintf(buf, "\nSketch: \"%s\", compiled %s, %s\n", Sketch, __DATE__, __TIME__);
  Serial.println(buf);
  Serial.println(Description);
}
