//
// Created by Dario Bonfiglio on 10/11/25.
//

#ifndef EASYNES_EMU_H
#define EASYNES_EMU_H


#include <stdint.h>
#include "bus.h"
#include "cpu.h"
#include "ppu.h"

// Un tick “logico”: esegue UN passo CPU (quanti cicli consuma lo decide cpu_step)
// e avanza la PPU di 3x quei cicli. Gestisce anche il bridge dell’NMI.
void emu_step_one_tick(cpu c, bus b);

// Esegui N istruzioni CPU (comode per test deterministici)
void emu_run_for_instructions(cpu c, bus b, uint64_t instr_count);

// Esegui N frame NTSC (alternanza 29780 / 29781 cicli CPU per frame)
void emu_run_for_frames(cpu c, bus b, int frames);

#endif //EASYNES_EMU_H
