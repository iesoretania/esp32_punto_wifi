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
#include <SmartLeds.h>

MelodyPlayer player(BUZZER_PIN, 0, LOW);

const int LED_COUNT = 8;
const int CHANNEL = 1;

SmartLed *leds = nullptr;

Rgb targetLeds[LED_COUNT];

typedef enum { LED_OFF, LED_RAINBOW, LED_FIXED, LED_ANIMATION } LEDStatus;

typedef struct LedAnimationItemStruct {
    Rgb color;
    uint16_t increment;
    int32_t delay;
} LedAnimationItem;

LedAnimationItem led_animation[10];
LedAnimationItem new_led_animation[10];

int16_t animation_state = -2;

LEDStatus led_status = LED_OFF;
Rgb target_color;
Rgb source_color;
Rgb current_color;
uint8_t hue = 0;
uint16_t transition = 0;
const Rgb black_color = Rgb();

void set_led_animation(LedAnimationItem items[], int count, Rgb initial_color);

void set_led_off();

void set_led_fixed(Rgb color);

void set_led_rainbow();

void notify_tick() {
    // SmartLed -> RMT driver (WS2812/WS2812B/SK6812/WS2813)
    if (nullptr == leds) {
        leds = new SmartLed(LED_WS2812B, LED_COUNT, RGB_PIN, CHANNEL, DoubleBuffer);
    }

    switch (led_status) {
        case LED_OFF:
            for (int i = 0; i != LED_COUNT; i++) {
                (*leds)[i] = black_color;
            }
            current_color = black_color;
            break;
        case LED_FIXED:
            for (int i = 0; i != LED_COUNT; i++) {
                (*leds)[i] = target_color;
            }
            current_color = target_color;
            break;
        case LED_ANIMATION:
            if (animation_state == -1) {
                for (int i = 0; i < 10; i++) {
                    led_animation[i] = new_led_animation[i];
                }
                animation_state = 0;
                transition = 0;
                target_color = led_animation[0].color;
            }
            if (animation_state >= 0) {
                if (transition < 10000) {
                    transition += led_animation[animation_state].increment;
                    if (transition > 10000) transition = 10000;

                    float r = ((float) target_color.r * (float) transition +
                               (float) source_color.r * (10000 - (float) transition)) / 10000.0f;
                    float g = ((float) target_color.g * (float) transition +
                               (float) source_color.g * (10000 - (float) transition)) / 10000.0f;
                    float b = ((float) target_color.b * (float) transition +
                               (float) source_color.b * (10000 - (float) transition)) / 10000.0f;
                    current_color = Rgb((int) r & 0xFF, (int) g & 0xFF, (int) b & 0xFF);
                } else {
                    if (led_animation[animation_state].delay > 0) {
                        led_animation[animation_state].delay--;
                    }
                    if (led_animation[animation_state].delay == 0) {
                        source_color = led_animation[animation_state].color;
                        animation_state++;
                        target_color = led_animation[animation_state].color;
                        transition = 0;
                    }
                }
                for (int i = 0; i != LED_COUNT; i++) {
                    (*leds)[i] = current_color;
                }
            }
            break;
        case LED_RAINBOW:
            hue++;
            for (int i = 0; i < LED_COUNT; i++) {
                (*leds)[i] = Hsv{static_cast<uint8_t>( hue + 8 * i ), 255, 255};
            }
            current_color = (*leds)[0];
    }
    leds->show();
}

void notify_start() {
    const char* init_melody = "init:d=4,o=6,b=200: 8c, 4g";
    Melody melody = MelodyFactory.loadRtttlString(init_melody);
    player.playAsync(melody);

    set_led_rainbow();
}

void notify_ready() {
    const char* init_melody = "start:d=4,o=6,b=200: 8c, 8e, 4g";
    Melody melody = MelodyFactory.loadRtttlString(init_melody);
    player.playAsync(melody);

    set_led_off();
}

void notify_rfid_read() {
    const char* card_melody = "card:d=16,o=5,b=200: g";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);
}

void notify_check_start() {
    LedAnimationItem anim[] = {
            {
                    .color = Rgb(240, 240, 255),
                    .increment = 1000,
                    .delay = -1,
            }
    };

    set_led_animation(anim, 1, black_color);
}

void notify_check_success_in() {
    const char* card_melody = "entrada:d=16,o=6,b=200: 8b";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);

    LedAnimationItem anim[] = {
            {
                    .color = Rgb(0, 255, 0),
                    .increment = 500,
                    .delay = 50,
            },
            {
                    .color = Rgb(0, 0, 0),
                    .increment = 500,
                    .delay = -1,
            }
    };

    set_led_animation(anim, 2, current_color);
}

void notify_check_success_out() {
    const char* card_melody = "salida:d=16,o=6,b=200: 8b, p, 8b";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);

    LedAnimationItem anim[2] = {
            {
                    .color = Rgb(255, 165, 0),
                    .increment = 500,
                    .delay = 50,
            },
            {
                    .color = Rgb(0, 0, 0),
                    .increment = 500,
                    .delay = -1,
            }
    };

    set_led_animation(anim, 2, current_color);
}

void notify_check_error() {
    const char* card_melody = "error:d=16,o=6,b=180: 1c";
    Melody melody = MelodyFactory.loadRtttlString(card_melody);
    player.playAsync(melody);

    LedAnimationItem anim[] = {
            {
                    .color = Rgb(255, 0, 0),
                    .increment = 500,
                    .delay = 5,
            },
            {
                    .color = Rgb(0, 0, 0),
                    .increment = 500,
                    .delay = 0,
            },

            {
                    .color = Rgb(255, 0, 0),
                    .increment = 500,
                    .delay = 5,
            },
            {
                    .color = Rgb(0, 0, 0),
                    .increment = 500,
                    .delay = -1,
            },
    };

    set_led_animation(anim, 4, current_color);
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

void notify_stop() {
    set_led_off();
}

void notify_firmware_start() {
    set_led_fixed(Rgb(16, 16, 255));
}

void set_led_animation(LedAnimationItem items[], int count, Rgb initial_color) {
    if (count > 10) count = 10;
    for (int i = 0; i < count; i++) {
        new_led_animation[i] = items[i];
    }
    animation_state = -1;

    source_color = initial_color;

    led_status = LED_ANIMATION;
}

void set_led_off() {
    led_status = LED_OFF;
}

void set_led_fixed(Rgb color) {
    target_color = Rgb(color);
    led_status = LED_FIXED;
}

void set_led_rainbow() {
    led_status = LED_RAINBOW;
    hue = 0;
}
