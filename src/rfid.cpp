//
// Copyright (C) 2020-2021: Luis Ramón López López
//
// This file is part of "Punto de Control de Presencia Wi-Fi".
//
// "Punto de Control de Presencia Wi-Fi" is free software: you can redistribute
// it and/or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// "Punto de Control de Presencia Wi-Fi" is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
//

#include <Arduino.h>
#include <SPI.h>

#include <MFRC522.h>
#include "drivers/rdm6300.h"

#include "rfid.h"
#include "notify.h"

MFRC522 mfrc522(RFID_SDA_PIN, RFID_RST_PIN);
Rdm6300 rdm6300;
uint32_t lastTag;

void initialize_rfid() {
    SPI.begin();
    mfrc522.PCD_Init();
    rdm6300.begin(RFID_RDM6300_TX, 1);
}

bool rfid_new_card_detected() {
    if (rdm6300.update()) {
        lastTag = rdm6300.get_tag_id();
        return true;
    } else {
        return mfrc522.PICC_IsNewCardPresent();
    }
}

String rfid_read_id() {
    String uidS;

    if (mfrc522.PICC_IsNewCardPresent()) {
        notify_rfid_read();

        if (mfrc522.PICC_ReadCardSerial()) {
            uint32_t uid = (uint32_t) (
                    mfrc522.uid.uidByte[0] +
                    (mfrc522.uid.uidByte[1] << 8) +
                    (mfrc522.uid.uidByte[2] << 16) +
                    (mfrc522.uid.uidByte[3] << 24));

            uidS = (String) uid;

            while (uidS.length() < 10) {
                uidS = "0" + uidS;
            }
        }
        mfrc522.PICC_HaltA();
    } else if (rdm6300.update()) {
        notify_rfid_read();
        uidS = (String) lastTag;

        while (uidS.length() < 10) {
            uidS = "0" + uidS;
        }
    }
    return uidS;
}
