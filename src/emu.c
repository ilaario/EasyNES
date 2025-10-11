//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/emu.h"

void emu_step_one_tick(cpu c, bus b){
    int used = cpu_step(c);
    ppu_tick(b -> ppu, (uint64_t)used);

    if(ppu_nmi_pending(b -> ppu)){
        cpu_nmi(c);
        ppu_clear_nmi(b -> ppu);
    }
}
