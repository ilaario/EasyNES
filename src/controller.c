//
// Created by Dario Bonfiglio on 10/9/25.
//

#include "headers/controller.h"
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>

void controllerset_init(cs c){
    if (!c) {
        fprintf(stderr, "controllerset_init: c is NULL\n");
        exit(EXIT_FAILURE);
    }

    // Allocate controllers for pad 1 and pad 2 inside the already-allocated controller_set
    c->pad_1 = (controller)malloc(sizeof(struct Controller));
    c->pad_2 = (controller)malloc(sizeof(struct Controller));
    if (!c->pad_1 || !c->pad_2) {
        fprintf(stderr, "controllerset_init: malloc failed\n");
        exit(EXIT_FAILURE);
    }

    controller_init(c->pad_1);
    controller_init(c->pad_2);
}

void controller_init(controller c){
    if (!c) {
        fprintf(stderr, "controller_init: c is NULL\n");
        return;
    }

    c->strobe         = false;
    c->current_button = 0;
    c->latched_button = 0;
    c->shift_index    = BTN_COUNT; // nothing latched yet
}

void controller_poll_host_input(cs c){
    if (!c || !c->pad_1 || !c->pad_2) {
        // Not initialized yet; avoid crashing.
        return;
    }
    c->pad_1->current_button = read_pad_1_as_bitmask(c);
    c->pad_2->current_button = read_pad_2_as_bitmask(c);
}

void controller_cpu_write(cs c, uint16_t addr, uint8_t value){
    if (!c || !c->pad_1 || !c->pad_2) return;

    if(addr == ADDR_STROBE_WRITE){
        bool new_strobe = (value & 1) != 0;

        if(new_strobe) {
            controller_poll_host_input(c);
            c->pad_1->latched_button = c->pad_1->current_button;
            c->pad_2->latched_button = c->pad_2->current_button;
            c->pad_1->shift_index = 0;
            c->pad_2->shift_index = 0;
        }

        c->pad_1->strobe = new_strobe;
        c->pad_2->strobe = new_strobe;
    }
}

uint8_t controller_cpu_read(cs c, uint16_t addr){
    if (!c) return 0xFF;
    if(addr == ADDR_PAD_1) return c->pad_1 ? controller_read_serial(c->pad_1) : 0x41; // bit0=1 when open
    if(addr == ADDR_PAD_2) return c->pad_2 ? controller_read_serial(c->pad_2) : 0x41;
    return 0xFF; // open bus
}

uint8_t controller_read_serial(controller c){
    if (!c) return 1; // open bus behaviour, bit0 high

    uint8_t bit0 = 0;

    if(c->strobe){
        bit0 = (c->current_button >> BTN_A) & 1;
    } else {
        if(c->shift_index < BTN_COUNT){
            bit0 = (c->latched_button >> c->shift_index) & 1;
        } else {
            bit0 = 1; // after 8 reads, controllers return 1
        }
        c->shift_index += 1;
    }

    return bit0;
}

uint8_t read_pad_1_as_bitmask(cs c){
    uint8_t mask = 0;

    bool gp0 = IsGamepadAvailable(0);

    // A / B
    if (IsKeyDown(KEY_X) || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)))  mask |= (1 << BTN_A);
    if (IsKeyDown(KEY_Z) || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))) mask |= (1 << BTN_B);

    // Select / Start
    if (IsKeyDown(KEY_RIGHT_SHIFT) || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_LEFT)))  mask |= (1 << BTN_SELECT);
    if (IsKeyDown(KEY_ENTER)       || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) mask |= (1 << BTN_START);

    // D-Pad
    if (IsKeyDown(KEY_UP)    || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP)))    mask |= (1 << BTN_UP);
    if (IsKeyDown(KEY_DOWN)  || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)))  mask |= (1 << BTN_DOWN);
    if (IsKeyDown(KEY_LEFT)  || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)))  mask |= (1 << BTN_LEFT);
    if (IsKeyDown(KEY_RIGHT) || (gp0 && IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))) mask |= (1 << BTN_RIGHT);

    return mask;
}

uint8_t read_pad_2_as_bitmask(cs c){
    uint8_t mask = 0;

    bool gp1 = IsGamepadAvailable(1);

    // A / B (player 2 default keys O/U)
    if (IsKeyDown(KEY_O) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)))  mask |= (1 << BTN_A);
    if (IsKeyDown(KEY_U) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))) mask |= (1 << BTN_B);

    // Select / Start (player 2 default keys Left Shift / Backspace)
    if (IsKeyDown(KEY_LEFT_SHIFT) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_MIDDLE_LEFT)))  mask |= (1 << BTN_SELECT);
    if (IsKeyDown(KEY_BACKSPACE)  || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_MIDDLE_RIGHT))) mask |= (1 << BTN_START);

    // D-Pad (player 2 default keys I J K L)
    if (IsKeyDown(KEY_I) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_LEFT_FACE_UP)))    mask |= (1 << BTN_UP);
    if (IsKeyDown(KEY_K) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_LEFT_FACE_DOWN)))  mask |= (1 << BTN_DOWN);
    if (IsKeyDown(KEY_J) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_LEFT_FACE_LEFT)))  mask |= (1 << BTN_LEFT);
    if (IsKeyDown(KEY_L) || (gp1 && IsGamepadButtonDown(1, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))) mask |= (1 << BTN_RIGHT);

    return mask;
}
