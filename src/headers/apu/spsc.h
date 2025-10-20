//
// Created by Dario Bonfiglio on 10/19/25.
//

#ifndef EASYNES_SPSC_H
#define EASYNES_SPSC_H

#include <stddef.h>     // size_t
#include <stdint.h>
#include <stdatomic.h>  // C11 atomics
#include <string.h>     // memcpy
#include <stdlib.h>     // malloc/free

typedef struct {
    size_t              max_size;     // capacità (num. elementi)
    _Atomic size_t      write_index;  // indice di scrittura (solo writer)
    _Atomic size_t      read_index;   // indice di lettura  (solo reader)
    size_t              elem_size;    // dimensione di ciascun elemento in byte
    unsigned char*      storage;      // buffer contiguo: max_size * elem_size
} spsc_ring;

/* Inizializza: alloca storage per 'capacity' elementi di grandezza 'elem_size' */
static inline int spsc_ring_init(spsc_ring* q, size_t capacity, size_t elem_size) {
    if (!q || capacity == 0 || elem_size == 0) return 0;
    q->max_size = capacity;
    q->elem_size = elem_size;
    q->storage = (unsigned char*)malloc(capacity * elem_size);
    if (!q->storage) return 0;
    atomic_store_explicit(&q->write_index, 0, memory_order_relaxed);
    atomic_store_explicit(&q->read_index,  0, memory_order_relaxed);
    return 1;
}

/* Libera le risorse */
static inline void spsc_ring_free(spsc_ring* q) {
    if (!q) return;
    free(q->storage);
    q->storage = NULL;
    q->max_size = q->elem_size = 0;
    atomic_store_explicit(&q->write_index, 0, memory_order_relaxed);
    atomic_store_explicit(&q->read_index,  0, memory_order_relaxed);
}

/* Util: next_index */
static inline size_t spsc_next_index(size_t idx, size_t max_size) {
    ++idx;
    if (idx >= max_size) idx -= max_size;
    return idx;
}

/* Push di UN elemento (ritorna 1 se ok, 0 se pieno)
   'elem' è un puntatore ai dati dell'elemento da copiare */
static inline int spsc_ring_push(spsc_ring* q, const void* elem) {
    const size_t write_index = atomic_load_explicit(&q->write_index, memory_order_relaxed); // solo writer
    const size_t next        = spsc_next_index(write_index, q->max_size);

    // Se next == read_index => pieno
    if (next == atomic_load_explicit(&q->read_index, memory_order_acquire))
        return 0;

    // Scrivi l'elemento nello slot corrente
    memcpy(q->storage + (write_index * q->elem_size), elem, q->elem_size);

    // Pubblica l'avanzamento dell'indice (release)
    atomic_store_explicit(&q->write_index, next, memory_order_release);
    return 1;
}

/* Pop di FINO A 'output_count' elementi in 'out' (buffer pre-allocato)
   Ritorna il numero di elementi effettivamente letti */
static inline size_t spsc_ring_pop(spsc_ring* q, void* out, size_t output_count) {
    const size_t write_index = atomic_load_explicit(&q->write_index, memory_order_acquire);
    const size_t read_index  = atomic_load_explicit(&q->read_index,  memory_order_relaxed); // solo reader

    size_t avail;
    if (write_index >= read_index) {
        avail = write_index - read_index;
    } else {
        avail = write_index + q->max_size - read_index;
    }
    if (avail == 0) return 0;

    if (output_count > avail) output_count = avail;

    size_t new_read_index = read_index + output_count;

    // Copia potenzialmente in due tranche (wrap-around)
    size_t elem_size = q->elem_size;
    unsigned char* dst = (unsigned char*)out;

    if (read_index + output_count >= q->max_size) {
        // Prima parte: dalla posizione corrente fino alla fine del buffer
        const size_t count0 = q->max_size - read_index;
        const size_t bytes0 = count0 * elem_size;
        memcpy(dst, q->storage + (read_index * elem_size), bytes0);

        // Seconda parte: dall'inizio del buffer
        const size_t count1 = output_count - count0;
        const size_t bytes1 = count1 * elem_size;
        memcpy(dst + bytes0, q->storage, bytes1);

        new_read_index -= q->max_size; // wrap
    } else {
        // Un blocco contiguo
        const size_t bytes = output_count * elem_size;
        memcpy(dst, q->storage + (read_index * elem_size), bytes);

        if (new_read_index == q->max_size) new_read_index = 0;
    }

    // Pubblica l'avanzamento del read_index (release)
    atomic_store_explicit(&q->read_index, new_read_index, memory_order_release);
    return output_count;
}

/* Reset (non thread-safe) */
static inline void spsc_ring_reset(spsc_ring* q) {
    atomic_store_explicit(&q->write_index, 0, memory_order_relaxed);
    atomic_store_explicit(&q->read_index,  0, memory_order_release);
}

/* Size (best-effort) */
static inline size_t spsc_ring_size(spsc_ring* q) {
    const size_t w = atomic_load_explicit(&q->write_index, memory_order_relaxed);
    const size_t r = atomic_load_explicit(&q->read_index,  memory_order_relaxed);
    return (w >= r) ? (w - r) : (w + q->max_size - r);
}

/* Empty (best-effort; risultato può cambiare immediatamente in scenari concorrenti) */
static inline int spsc_ring_empty(spsc_ring* q) {
    return (spsc_ring_size(q) == 0);
}

/* Capacity */
static inline size_t spsc_ring_capacity(const spsc_ring* q) {
    return q->max_size;
}

#endif //EASYNES_SPSC_H
