//
// Copyright (C) 2020-2021: Luis Ramón López López
// Basado en:
// A simple library to interface with rdm6300 rfid reader.
// Arad Eizen (https://github.com/arduino12).
//

#ifndef _CRDM6300_h_
#define _CRDM6300_h_

#include <Arduino.h>
#include <HardwareSerial.h>
#include <Stream.h>

#define RDM6300_BAUDRATE        9600
#define RDM6300_READ_TIMEOUT    20

class Rdm6300 {
public:
    void begin(int rx_pin, uint8_t uart_nr = 1);

    void end();

    bool update();

    uint32_t get_tag_id();

    void clear();

    HardwareSerial *_hardware_serial = nullptr;
    Stream *_stream = nullptr;
    uint32_t _tag_id = 0;
    uint32_t _last_tag_id = 0;
};

#endif
