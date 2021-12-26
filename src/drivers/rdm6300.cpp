//
// Copyright (C) 2020-2021: Luis Ramón López López
// Basado en:
// A simple library to interface with rdm6300 rfid reader.
// Arad Eizen (https://github.com/arduino12).
//

#include "rdm6300.h"

void Rdm6300::begin(int rx_pin, uint8_t uart_nr) {
    /* init serial port to rdm6300 baud, without TX, and 20ms read timeout */
    end();
    _stream = _hardware_serial = new HardwareSerial(uart_nr);
    _hardware_serial->begin(RDM6300_BAUDRATE, SERIAL_8N1, rx_pin, -1);
    _stream->setTimeout(RDM6300_READ_TIMEOUT);
}

void Rdm6300::end() {
    _stream = nullptr;
    if (_hardware_serial)
        _hardware_serial->end();
}

bool Rdm6300::update() {
    char buff[10];
    uint32_t tag_id;

    if (!_stream)
        return false;

    if (_stream->available() < 9)
        return false;

    if (9 != _stream->readBytes(buff, 9))
        return false;

    /* if a packet doesn't end with the right byte, drop it */
    if (buff[8] != ' ')
        return false;

    /* add null */
    buff[8] = 0;
    tag_id = strtol(buff, NULL, 16);

    /* if a new tag appears- return it */
    if (_last_tag_id != tag_id) {
        _last_tag_id = tag_id;
        _last_read_ms = 0;
    }
    /* if the old tag is still here set tag_id to zero */
    if (is_tag_near())
        tag_id = 0;
    _last_read_ms = millis();

    _tag_id = tag_id;
    return tag_id;
}

bool Rdm6300::is_tag_near() {
    return millis() - _last_read_ms < RDM6300_NEXT_READ_MS;
}

uint32_t Rdm6300::get_tag_id() {
    uint32_t tag_id = _tag_id;
    _tag_id = 0;
    return tag_id;
}
