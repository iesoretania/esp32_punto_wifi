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

#include "notify.h"

#include <melody_player.h>
#include <melody_factory.h>

MelodyPlayer player(BUZZER_PIN, 0, LOW);

void notify_start() {
    const char* init_melody = "init:d=4,o=6,b=200: 8c, 4g";
    Melody melody = MelodyFactory.loadRtttlString(init_melody);
    player.playAsync(melody);
}
void notify_ready() {
    const char* init_melody = "start:d=4,o=6,b=200: 8c, 8e, 4g";
    Melody melody = MelodyFactory.loadRtttlString(init_melody);
    player.playAsync(melody);
}

void notify_check_start() {
    const char* card_melody = "card:d=16,o=5,b=200: g";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}

void notify_check_success_in() {
    const char* card_melody = "entrada:d=16,o=6,b=200: 8b";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}

void notify_check_success_out() {
    const char* card_melody = "salida:d=16,o=6,b=200: 8b, p, 8b";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}

void notify_check_error() {
    const char* card_melody = "error:d=16,o=6,b=180: 1c";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}

void notify_key_press() {
    const char* card_melody = "card:d=16,o=5,b=200: 32c";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}

void notify_button_press() {
    const char* card_melody = "card:d=16,o=5,b=200: 16e";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}