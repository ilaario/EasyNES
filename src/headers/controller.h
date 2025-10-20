//
// Created by Dario Bonfiglio on 10/9/25.
//

#ifndef EASYNES_CONTROLLER_H
#define EASYNES_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define ADDR_STROBE_WRITE  0x4016
#define ADDR_PAD_1         0x4016
#define ADDR_PAD_2         0x4017

#define BTN_A              0
#define BTN_B              1
#define BTN_SELECT         2
#define BTN_START          3
#define BTN_UP             4
#define BTN_DOWN           5
#define BTN_LEFT           6
#define BTN_RIGHT          7
#define BTN_COUNT          8


struct Controller {
    bool    strobe;
    uint8_t current_button;
    uint8_t latched_button;
    int     shift_index;
};

typedef struct Controller* controller;

struct controller_set{
    controller pad_1;
    controller pad_2;
};

typedef struct controller_set* cs;

void    controllerset_init(cs c);
void    controller_init(controller c);
void    controller_poll_host_input(cs c);
void    controller_cpu_write(cs c, uint16_t addr, uint8_t value);
uint8_t controller_cpu_read(cs c, uint16_t addr);
uint8_t controller_read_serial(controller c);
uint8_t read_pad_1_as_bitmask(cs c);
uint8_t read_pad_2_as_bitmask(cs c);

#endif //EASYNES_CONTROLLER_H
