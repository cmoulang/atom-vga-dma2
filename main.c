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
    puts("atom-vga-dma 2");
    puts(__DATE__ " " __TIME__);

    int freq = measure_freq(PIN_1MHZ);
    printf("6502 clock = %.2f MHz\n", freq / 1000000.0);
    sleep_ms(1000);

    // Initialise the multiplexer state machine
    PIO mux_pio;
    uint mux_sm;
    int mux_offset;
    pio_claim_free_sm_and_add_program(&mux_program, &mux_pio, &mux_sm, &mux_offset);
    mux_init(mux_pio, mux_sm, mux_offset);
    pio_sm_set_enabled(mux_pio, mux_sm, true);
    pio_sm_put(mux_pio, mux_sm, (int)(&buffer) >> ADD_BITS);

    uint mux_addr_chan = dma_claim_unused_channel(true);
    uint mux_data_chan = dma_claim_unused_channel(true);

    dma_channel_config c;
    c  = dma_channel_get_default_config(mux_addr_chan);
    channel_config_set_high_priority(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(mux_pio, mux_sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, 0);
    channel_config_set_write_increment(&c, 0);
    channel_config_set_chain_to(&c, mux_data_chan);
    dma_channel_configure(
        mux_addr_chan,
        &c,
        &dma_channel_hw_addr(mux_data_chan)->write_addr,
        &mux_pio->rxf[mux_sm],
        1,
        true);

    c = dma_channel_get_default_config(mux_data_chan);
    channel_config_set_high_priority(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(mux_pio, mux_sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, 0);
    channel_config_set_write_increment(&c, 0);
    channel_config_set_chain_to(&c, mux_addr_chan);
    dma_channel_configure(
        mux_data_chan,
        &c,
        NULL,
        &mux_pio->rxf[mux_sm],
        1,
        false);

    // Set the DMA going
    const int delay_count = 8;
    pio_sm_put(mux_pio, mux_sm, delay_count);

    for (;;)
    {
        // Pretty print the VDU Screen RAM
        screen_dump(buffer + 0x8000);
        sleep_ms(40);
    }

}


