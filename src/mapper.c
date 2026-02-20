//
// Created by Dario Bonfiglio on 10/17/25.
//

#include "headers/mapper.h"

void mapper_destroy(mapper m) {
    if (!m) return;
    if (m -> destroy) {
        m -> destroy(m);
        return;
    }
    free(m);
}

void create_mapper(mapper* out, cartridge cart,
                   irq_handle irq
                   /*void (*mirroring_cb)(void)*/) {
    if (!out || !cart) return;
    *out = NULL;

    switch (cart -> header.mapper_id) {
        case NROM:
            *out = mapper_nrom_create(cart);
            break;
        case SxROM:
            *out = mapper_mmc1_create(cart);
            break;
        case UxROM:
            *out = mapper_uxrom_create(cart);
            break;
        case CNROM:
            *out = mapper_cnrom_create(cart);
            break;
        case MMC3:
            *out = mapper_mmc3_create(cart, irq);
            break;
        case AxROM:
            *out = mapper_axrom_create(cart);
            break;
        case ColorDreams:
            *out = mapper_colordreams_create(cart);
            break;
        case GxROM:
            *out = mapper_gxrom_create(cart);
            break;
        default:
            perror("Unsupported mapper id: %u", cart -> header.mapper_id);
            break;
    }
}
