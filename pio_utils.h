#pragma once

#include "hardware/pio.h"

/*! \brief Lookup the GPIO_FUNC_PIOn constant for the given PIO 
 *
 * \param pio the PIO
 * \return -ve if error or GPIO_FUNC_PIO0, GPIO_FUNC_PIO1 or GPIO_FUNC_PIO2
 */
static const int piou_gpio_func(PIO pio)
{
    int gpio_func;
    if (pio == pio0)
    {
        gpio_func = GPIO_FUNC_PIO0;
    }
    else if (pio == pio1)
    {
        gpio_func = GPIO_FUNC_PIO1;
    }
#ifdef PICO_RP2350
    else if (pio == pio2)
    {
        gpio_func = GPIO_FUNC_PIO2;
    }
#endif
    else
    {
        gpio_func=-1;
    }
    return gpio_func;
}

/*! \brief Lookup the rx fifo not empty pio interrupt source enum for the given state machine
 *
 * \param sm state machine index 0..3
 * \return the appropriate enum: pis_sm0_rx_fifo_not_empty..pis_sm3_rx_fifo_not_empty
*/
static const int pis_smX_rx_fifo_not_empty(int sm)
{
    static_assert(pis_sm3_rx_fifo_not_empty == pis_sm0_rx_fifo_not_empty+3);
    static_assert(pis_sm2_rx_fifo_not_empty == pis_sm0_rx_fifo_not_empty+2);
    static_assert(pis_sm1_rx_fifo_not_empty == pis_sm0_rx_fifo_not_empty+1);
    return pis_sm0_rx_fifo_not_empty + sm;
}


/*! \brief Measure the frequncy of a digital signal on a gpio input.
 *
 * \param gpio GPIO number
 *
 * Returns the average frequncy in Hz of the signal sampled during the call.
 *
 * \return the frequncy of the signal
 *
 */
static int measure_freq(int gpio)
{
    const int sample_count = 10000;
    int count = 0;
    int prev_val = gpio_get(gpio);

    absolute_time_t start_time2 = get_absolute_time();
    for (int i = 0; i < sample_count; i++)
    {
        int current_val = gpio_get(gpio);
        if (current_val != prev_val)
        {
            prev_val = current_val;
            count++;
        }
    }
    absolute_time_t end_time2 = get_absolute_time();
    int elapsed_us = absolute_time_diff_us(start_time2, end_time2);

    int freq = (count * 500000) / elapsed_us;
    return freq;
}


static void screen_dump(char *buffer)
{
    char buf[33];
    printf("\033[?25l\033[0;0H");
    for (int row = 0; row < 16; row++)
    {
        for (int i = 0; i < 32; i++)
        {
            int x = (unsigned char)buffer[i + row * 32];
            if (x < 0x80)
            {
                x = x ^ 0x60;
            }
            x = x - 0x20;
            if (x < ' ' || x > '~') {
                x = '.';
            }
            buf[i] = x;
        }
        buf[32] = 0;

        puts(buf);
    }
}

static void fifo_dump(PIO pio, int sm) 
{
    for (;;)
    {
        int i;
        sleep_ms(1000);
        while (pio_sm_get_rx_fifo_level(pio, sm) > 0)
        {
            if (!(i++ % 10))
            {
                printf("\n");
            }
            unsigned int x = pio_sm_get(pio, sm);
            printf("%8x ", x);
        }
        printf("-\n");
    }
}