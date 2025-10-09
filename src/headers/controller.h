//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_CONTROLLER_H
#define EASYNES_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

struct Controller {
    uint8_t latched_buttons;
    uint8_t shift_reg;
    bool strobe;
};

typedef struct Controller* controller;

void controller_init(controller c);
void controller_set_buttons(controller c, uint8_t mask);
void controller_write_strobe(controller c, uint8_t bit0);
uint8_t controller_read_shift(controller c);

#endif //EASYNES_CONTROLLER_H
