//
// Created by Dario Bonfiglio on 10/19/25.
//

#include "../headers/apu/pulse.h"
#include <math.h>

static void quarter_frame_noop(struct FrameClockable *self) { (void)self; }
static void half_frame_sweep_bridge(struct FrameClockable *self) {
    s_half_frame_clock((sweep)self);
}

static inline int calc_note_freq(int period, int seq_length, double clock_period_ns)
{
    return (int)(1e9 / (clock_period_ns * period * seq_length));
}

static inline void freq_to_note(double freq, char *out, size_t out_size)
{
    static const char *notes[] = {
            "A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"
    };
    const int note_count = 12;

    double note_number_double = 12.0 * log2(freq / 440.0) + 49.0;
    int note_number = (int)lround(note_number_double);

    int note_index = (note_number - 1) % note_count;
    if (note_index < 0)
        note_index += note_count;

    int octave = (note_number + 8) / note_count;

    snprintf(out, out_size, "%s%d", notes[note_index], octave);
}

void pulse_init(pulse p, enum Pulse_Type t){
    if(!p) exit(EXIT_FAILURE);
    p -> type = t;
    p -> volume = (volume)calloc(1, sizeof(struct Volume));
    volume_init(p -> volume);
    p -> length_counter = (length_counter)calloc(1, sizeof(struct LengthCounter));
    length_init(p -> length_counter);
    p -> sequencer = (divider)calloc(1, sizeof(struct Divider));
    divider_init(p -> sequencer, 0);
    p -> seq_idx = 0;
    p -> seq_type = SEQ_12_5;
    p -> period = 0;

    p -> sweep = (sweep)calloc(1, sizeof(struct Sweep));
    sweep_init(p -> sweep, p, t == Pulse1);
}

void sweep_init(sweep s, struct Pulse* p, bool ones_complement){
    if(!s) exit(EXIT_FAILURE);
    s -> base.quarter_frame_clock = quarter_frame_noop;
    s -> base.half_frame_clock = half_frame_sweep_bridge;
    s -> pulse = p;
    s -> ones_complement = ones_complement;

    s -> period          = 0;
    s -> enabled         = false;
    s -> reload          = false;
    s -> negate          = false;
    s -> shift           = 0;

    s -> div = (divider)calloc(1, sizeof(struct Divider));
    divider_init(s -> div, 0);
}

void p_set_period(pulse p, int pi){
    p -> period = pi;
    set_period(p -> sequencer, p -> period);
}

void p_clock(pulse p){
    if(div_clock(p -> sequencer)) p -> seq_idx = (p -> seq_idx + 7) % 8;
}

uint8_t p_sample(pulse p){
    if(muted(p -> length_counter)) return 0;
    // Mute rule must use pulse timer period, not sweep unit period (0..7).
    if(is_muted(p -> period, calculate_target(p -> sweep, p -> period))) return 0;
    if(!active(p -> seq_type, p -> seq_idx)) return 0;

    return get(p -> volume);
}

void s_half_frame_clock(sweep s){
    if(s -> reload){
        set_period(s -> div, s -> period);
        s -> reload = false;
        return;
    }

    if(!s -> enabled) return;
    if(!div_clock(s -> div)) return;

    if(s -> shift > 0){
        int current = s -> pulse -> period;
        int target  = calculate_target(s, current);
        if (!is_muted(current, target)) {
            p_set_period(s -> pulse, target);
        }
    }
}

int calculate_target(sweep s, int current){
    const int amt = current >> s -> shift;
    if(!s -> negate) return current + amt;
    if(s -> ones_complement) return (0 > current - amt - 1 ? 0 : current - amt - 1);
    return (0 > current - amt ? 0 : current - amt);
}
