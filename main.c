#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

#include "pio_utils.h"
#include "sm.pio.h"

#define CLOCK_MHZ 250

#define ADD_BITS 16
#define BUFFER_SIZE (1u << ADD_BITS)

static_assert(sizeof(void *) == 4);
static_assert(sizeof(int) == 4);

// Buffer spans 6502 address space
_Alignas(BUFFER_SIZE) unsigned char buffer[BUFFER_SIZE];

int main()
{
    bool clock_ok = set_sys_clock_khz(CLOCK_MHZ * 1000, true);
    stdio_init_all();

    int freq = measure_freq(PIN_1MHZ);
    printf("6502 clock = %.2f MHz\n", freq / 1000000.0);

    // Initialise the multiplexer state machine
    PIO mux_pio;
    uint mux_sm;
    int mux_offset;
    pio_claim_free_sm_and_add_program(&mux_program, &mux_pio, &mux_sm, &mux_offset);
    mux_init(mux_pio, mux_sm, mux_offset);
    pio_sm_set_enabled(mux_pio, mux_sm, true);
    pio_sm_put(mux_pio, mux_sm, (int)(&buffer) >> ADD_BITS);

    // Initialise the write data state machine
    PIO write_pio;
    uint write_sm;
    int write_offset;
    pio_claim_free_sm_and_add_program(&write_program, &write_pio, &write_sm, &write_offset);
    write_init(write_pio, write_sm, write_offset);
    pio_sm_set_enabled(write_pio, write_sm, true);

    printf("%x %x\n", mux_pio, mux_sm);
    printf("%x %x\n", write_pio, write_sm);

    uint mux_chan = dma_claim_unused_channel(true);
    uint write_chan = dma_claim_unused_channel(true);

    dma_channel_config mux_cfg = dma_channel_get_default_config(mux_chan);
    channel_config_set_high_priority(&mux_cfg, true);
    channel_config_set_dreq(&mux_cfg, pio_get_dreq(mux_pio, mux_sm, false));
    channel_config_set_transfer_data_size(&mux_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&mux_cfg, 0);
    channel_config_set_write_increment(&mux_cfg, 0);
    channel_config_set_chain_to(&mux_cfg, write_chan);
    dma_channel_configure(
        mux_chan,
        &mux_cfg,
        &dma_channel_hw_addr(write_chan)->write_addr,
        &mux_pio->rxf[mux_sm],
        1,
        true);

    dma_channel_config w_cfg = dma_channel_get_default_config(write_chan);
    channel_config_set_high_priority(&w_cfg, true);
    channel_config_set_dreq(&w_cfg, pio_get_dreq(write_pio, write_sm, false));
    channel_config_set_transfer_data_size(&w_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&w_cfg, 0);
    channel_config_set_write_increment(&w_cfg, 0);
    channel_config_set_chain_to(&w_cfg, mux_chan);
    dma_channel_configure(
        write_chan,
        &w_cfg,
        NULL,
        &write_pio->rxf[write_sm],
        1,
        true);

    // Set the DMA going
    pio_sm_put(mux_pio, mux_sm, 11);


    for (;;)
    {
        // Pretty print the VDU Screen RAM
        screen_dump(buffer + 0x8000);
        sleep_ms(40);
    }

}


