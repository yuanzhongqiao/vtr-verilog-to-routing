/*
 * Copyright 2023 CAS—Atlantic (University of New Brunswick, CASA)
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SIMULATION_BUFFER_H
#define SIMULATION_BUFFER_H

#include <atomic>
#include <thread>
/**
 * Odin use -1 internally, so we need to go back on forth
 * TODO: change the default odin value to match both the Blif value and this buffer
 */
typedef signed char data_t;

#define BUFFER_SIZE 12                     //use something divisible by 4 since the compact buffer can contain 4 value
#define CONCURENCY_LIMIT (BUFFER_SIZE - 1) // access to cycle -1 with an extra pdding cell

struct BitFields {
    uint8_t i0 : 2;
    uint8_t i1 : 2;
    uint8_t i2 : 2;
    uint8_t i3 : 2;
};

class atomic_buffer {
  private:
    BitFields bits[BUFFER_SIZE / 4];
    std::atomic<bool> lock;
    int32_t cycle;

    void lock_it() {
        std::atomic_thread_fence(std::memory_order_acquire);
        while (lock.exchange(true, std::memory_order_relaxed)) {
            std::this_thread::yield();
        }
    }

    void unlock_it() {
        lock.exchange(false, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_relaxed);
    }

    uint8_t get_bits(uint8_t index) {
        uint8_t modindex = index % (BUFFER_SIZE);
        uint8_t address = modindex / 4;
        uint8_t bit_index = modindex % 4;
        switch (bit_index) {
            case 0:
                return (this->bits[address].i0);
            case 1:
                return (this->bits[address].i1);
            case 2:
                return (this->bits[address].i2);
            case 3:
                return (this->bits[address].i3);
            default:
                return 0x3;
        }
    }

    void set_bits(uint8_t index, uint8_t value) {
        uint8_t modindex = index % (BUFFER_SIZE);
        uint8_t address = modindex / 4;
        uint8_t bit_index = modindex % 4;

        value = value & 0x3;

        switch (bit_index) {
            case 0:
                this->bits[address].i0 = value;
                break;
            case 1:
                this->bits[address].i1 = value;
                break;
            case 2:
                this->bits[address].i2 = value;
                break;
            case 3:
                this->bits[address].i3 = value;
                break;
            default:
                break;
        }
    }

  public:
    atomic_buffer(data_t value_in) {
        this->lock = false;
        this->cycle = -1;
        this->init_all_values(value_in);
    }

    void print() {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            uint8_t value = get_bits(i);
            printf("%s", (value == 0) ? "0" : (value == 1) ? "1"
                                                           : "x");
        }
        printf("\n");
    }

    void init_all_values(data_t value) {
        this->lock = false;
        for (int i = 0; i < BUFFER_SIZE; i++)
            set_bits(i, value);
    }

    int32_t lock_free_get_cycle() {
        return this->cycle;
    }

    void lock_free_update_cycle(int64_t cycle_in) {
        //if (cycle_in > this->cycle)
        this->cycle = cycle_in;
    }

    data_t lock_free_get_value(int64_t cycle_in) {
        return get_bits(cycle_in);
    }

    void lock_free_update_value(data_t value_in, int64_t cycle_in) {
        if (cycle_in > this->cycle) {
            set_bits(cycle_in, value_in);
            lock_free_update_cycle(cycle_in);
        }
    }

    int32_t get_cycle() {
        lock_it();
        int32_t value = lock_free_get_cycle();
        unlock_it();
        return value;
    }

    data_t get_value(int64_t cycle_in) {
        lock_it();
        data_t value = lock_free_get_value(cycle_in);
        unlock_it();
        return value;
    }

    void update_value(data_t value_in, int64_t cycle_in) {
        lock_it();
        lock_free_update_value(value_in, cycle_in);
        unlock_it();
    }
};

#endif
