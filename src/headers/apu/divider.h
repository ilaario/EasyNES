//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_DIVIDER_H
#define EASYNES_DIVIDER_H

#include <stdbool.h>
#include <stdlib.h>

struct Divider{
    int period;
    int counter;
};

typedef struct Divider* divider;

void divider_init(divider d, int period){
    if(!d) exit(EXIT_FAILURE);
    d -> period = period;
    d -> counter = 0;
};

bool div_clock(divider d){
    if(d -> counter == 0){
        d -> counter = d -> period;
        return true;
    }

    d -> counter -= 1;
    return false;
}

void set_period(divider d, int p) {
    d -> period = p;
}

void div_reset(divider d){
    d -> counter = d -> period;
}

int get_period(divider d){
    return d -> period;
}

#endif //EASYNES_DIVIDER_H
