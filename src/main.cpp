#include "config.h"

void setup() {
    Serial.begin(115200);
    setupStorage();
    configureWiFiSettings();
    scanAvailableWiFiNetworks();
    initiateWiFiConnection();
    initializeRtc();
    initializeServer();
    serverHandle();
    SayHello();  // Print some useful information to USB-serial

    InitializeCAN();
    InitializeNMEA2000();

}

void loop() {

    // Run routine every x ms
    // nonBlockingDelay(43200000, the_function, lastCalibrationTime);
    NMEA2000.ParseMessages();  // Parse NMEA2000 messages

    CheckSourceAddressChange();
    ReadCANMessages();

}