#ifndef __HALINE_RFID_H
#define __HALINE_RFID_H

#include <Arduino.h>
#include <SPI.h> 
#include <MFRC522.h>              	//use MFRC522 by GitHubCommunity Version 1.4.9

#define RST_PIN		14
#define SS_PIN		13

class Rfid{
    private:
        MFRC522::StatusCode status;
        MFRC522::MIFARE_Key key;

    public:
        void init();
        unsigned char loop(byte *uuid);
};

#endif