//
// Created by Dario Bonfiglio on 10/11/25.
//

#include "headers/emu.h"

// alternanza NTSC (aprox) dei cicli CPU per frame
static inline int cycles_for_frame(int frame_index) {
    // 29780, 29781, 29780, 29781, ...
    return (frame_index & 1) ? 29781 : 29780;
}

void emu_step_one_tick(cpu c, bus b) {
    // 1) CPU fa uno step: ritorna i cicli consumati (gestisce al suo interno eventuale DMA)
    int used = cpu_step(c);

    // 2) PPU avanza di 3x i cicli CPU
    ppu_tick(b->ppu, (uint64_t)used);

    // 3) Bridge NMI: se la PPU ha pendente un NMI, latcheggia la linea CPU e pulisci il pending
    if (ppu_nmi_pending(b->ppu)) {
        cpu_nmi(c);
        ppu_clear_nmi(b->ppu);
    }
}

void emu_run_for_instructions(cpu c, bus b, uint64_t instr_count) {
    for (uint64_t i = 0; i < instr_count; ++i) {
        emu_step_one_tick(c, b);
    }
}

void emu_run_for_frames(cpu c, bus b, int frames) {
    for (int f = 0; f < frames; ++f) {
        int budget = cycles_for_frame(f);
        while (budget > 0) {
            // Facciamo step CPU singoli per avere un bridge NMI reattivo
            int before_cycles = c->cycles; // se li tieni, ok; altrimenti ignora
            emu_step_one_tick(c, b);

            // cpu_step ha già consumato dei cicli (ritorna i “used”, ma qui
            // li recuperiamo ripetendo la chiamata se vuoi essere esplicito):
            // alternativa: cambia emu_step_one_tick per ritornare i cicli usati.
            // Per semplicità ricalcoliamo:
            // NOTA: se non tieni c->cycles cumulativi, modifica emu_step_one_tick per tornare i cicli.
            (void)before_cycles; // togli se usi un contatore reale
            // Metodo semplice e robusto: richiama direttamente cpu_step e usa il valore.
            // Qui lo facciamo esplicito:
            int used = cpu_step(c);
            ppu_tick(b->ppu, (uint64_t)used);
            if (ppu_nmi_pending(b->ppu)) {
                cpu_nmi(c);
                ppu_clear_nmi(b->ppu);
            }
            budget -= used;
        }
    }
}
