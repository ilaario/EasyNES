//
// Created by Dario Bonfiglio on 10/17/25.
//

#include "headers/mapper.h"

void create_mapper(mapper m, cartridge cart,
                   irq_handle irq
                   /*void (*mirroring_cb)(void)*/) {
    switch (cart -> header.mapper_id) {
        case NROM:
            m = mapper_nrom_create(cart);
            break;
        /* case SxROM:
            ret -> reset(mapper_mmc1_create(cart, mirroring_cb));
            break;
         case UxROM:
            ret -> reset(new MapperUxROM(cart));
            break;
        case CNROM:
            ret -> reset(new MapperCNROM(cart));
            break;
        case MMC3:
            ret -> reset(new MapperMMC3(cart, irq, mirroring_cb));
            break;
        case AxROM:
            ret -> reset(new MapperAxROM(cart, mirroring_cb));
            break;
        case ColorDreams:
            ret -> reset(new MapperColorDreams(cart, mirroring_cb));
            break;
        case GxROM:
            ret -> reset(new MapperGxROM(cart, mirroring_cb));
            break; */
        default:
            break;
    }
}
