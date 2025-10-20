//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/units.h"


void length_init(length_counter l){
    if(!l) exit(EXIT_FAILURE);
    l -> halt    = false;
    l -> enabled = false;
    l -> counter = 0;
}

void set_enable(length_counter f, bool new_value){
    f -> enabled = new_value;
    if(!f -> enabled) f -> counter = 0;
}

bool is_enabled(length_counter f) { return f -> enabled; }

void set_from_table(length_counter f, size_t index){
    const static int length_table[] = {
            10, 254, 20, 2,  40, 4,  80, 6,
            160, 8,  60, 10, 14, 12, 26, 14,
            12, 16,  24, 18, 48, 20, 96, 22,
            192, 24, 72, 26, 16, 28, 32, 30,
    };

    if(!f -> enabled) return;
    f -> counter = length_table[index];
}

void half_frame_clock(length_counter f){
    if (f -> halt) return;
    if (f -> counter == 0) return;
    --f -> counter;
}
bool muted(length_counter f){
    return !f -> enabled || f -> counter == 0;
}

void linear_init(linear_counter l){
    if(!l) exit(EXIT_FAILURE);
    l -> reload       = false;
    l -> reload_value = 0;
    l -> control      = true;
    l -> counter      = 0;
}

void set_linear(linear_counter f, int new_value){
    f -> reload_value = new_value;
}

void lc_quarter_frame_clock(linear_counter f){
    if (f -> reload) {
        f -> counter = f -> reload_value;
        if (!f -> control) f -> reload = false;
    }
    if (f -> counter == 0) return;

    --f -> counter;
}


void volume_init(volume v){
    if(!v) exit(EXIT_FAILURE);
    v -> divider = (divider)calloc(1, sizeof(struct Divider));
    divider_init(v -> divider, 0);
    v -> fixedVolumeOrPeriod = max_volume;
    v -> decayVolume         = max_volume;
    v -> constant_volume     = true;
    v -> is_looping          = false;
    v -> should_start        = false;
}

void v_quarter_frame_clock(volume f){
    if (f -> should_start)
    {
        f -> should_start = false;
        f -> decayVolume = max_volume;
        set_period(f -> divider, f -> fixedVolumeOrPeriod);
        return;
    }

    if (!div_clock(f -> divider)) return;

    if (f -> decayVolume > 0) --f -> decayVolume;
    else if (f -> is_looping) f -> decayVolume = max_volume;
}

int get(volume f){
    if (f -> constant_volume) return f -> fixedVolumeOrPeriod;
    return f -> decayVolume;
}