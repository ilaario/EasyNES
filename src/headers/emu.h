//
// Created by Dario Bonfiglio on 10/11/25.
//

#ifndef EASYNES_EMU_H
#define EASYNES_EMU_H

#include "cpu.h"
#include "bus.h"
#include "ppu.h"

void emu_step_one_tick(cpu c, bus b);

#endif //EASYNES_EMU_H
