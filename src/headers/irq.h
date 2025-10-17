//
// Created by Dario Bonfiglio on 10/17/25.
//

#ifndef EASYNES_IRQ_H
#define EASYNES_IRQ_H

struct irq_h{
    void (*pull)(void);
    void (*release)(void);
};

typedef struct irq_h* irq_handle;

#endif //EASYNES_IRQ_H
