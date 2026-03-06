#include "rfid.h"

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

void Rfid::init(){
    SPI.begin();
    rfid.PCD_Init();
};

// same as MRFC522 uidByte length is 10
unsigned char Rfid::loop(byte *uuid){
    // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
    for (byte i = 0; i < 6; i++) this->key.keyByte[i] = 0xFF;

    rfid.uid.size = 0;

    if (rfid.PICC_IsNewCardPresent()){
        if(rfid.PICC_ReadCardSerial()){
            MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        
            this->status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &this->key, &(rfid.uid)); //line 834 of MFRC522.cpp file
            if (this->status == MFRC522::STATUS_OK) {
                memcpy(uuid, rfid.uid.uidByte, rfid.uid.size);
            }
            else{
                Serial.print("Auth failed: "); Serial.println(rfid.GetStatusCodeName(this->status));

                rfid.uid.size = 0;
            }
        }

        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
    }

    return rfid.uid.size;
};