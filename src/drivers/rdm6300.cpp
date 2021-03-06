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

void Rdm6300::clear() {
    while (_stream->available() > 0) _stream->read();
}

bool Rdm6300::update() {
    char buff[20];
    uint32_t tag_id;

    static int status = 0;

    if (!_stream)
        return false;

    size_t count;

    if (_stream->available() == 0) {
        _last_tag_id = 0;
    }

    switch (status) {
        case 0:
            while (_stream->available() > 0) {
                if (_stream->read() == '\x20') {
                    status = 1;
                    break;
                }
            }
            if (status == 0) return false;
        case 1:

            if (_stream->available() < 10) return false;
            count = _stream->readBytes(buff, 10);
            if (count != 10) return false;

            status = 0;

            break;
        default:
            return false;
    }

    /* add null */
    buff[10] = 0;
    tag_id = strtol(&buff[2], nullptr, 16);

    /* if a new tag appears- return it */
    if (_last_tag_id != tag_id) {
        _last_tag_id = tag_id;
        _tag_id = tag_id;
        return true;
    }
    return false;
}

uint32_t Rdm6300::get_tag_id() {
    uint32_t tag_id = _tag_id;
    return tag_id;
}
