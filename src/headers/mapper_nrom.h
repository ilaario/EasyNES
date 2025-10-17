//
// Created by Dario Bonfiglio on 10/17/25.
//

#ifndef EASYNES_MAPPER_NROM_H
#define EASYNES_MAPPER_NROM_H

#include "mapper.h"

struct mapper_nrom{
    struct Mapper base;
    bool one_bank;
    bool uses_character_ram;

    uint8_t* character_ram;
};

typedef struct mapper_nrom* nrom;

// factory per NROM (mapper 0)
mapper mapper_nrom_create(cartridge cart);
void mapper_destroy(mapper m);


#endif //EASYNES_MAPPER_NROM_H
