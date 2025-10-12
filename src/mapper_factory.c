//
// Created by Dario Bonfiglio on 10/13/25.
//

#include "headers/mapper.h"

mapper create_mapper_for_cart(cartridge cart) {
    switch (cart->header.mapper_id) {
        case 0:  return mapper_nrom_create(cart);
        case 1:  return mapper_mmc1_create(cart);   // NEW
        default:
            fprintf(stderr, "Mapper %d non supportato.\n", cart->header.mapper_id);
            return NULL;
    }
}
