/*
 * Bluetooth Low Energy example system using the Nordic Semiconductor
 * nRF8001 BLE module (Olimex MOD-nRF8001)
 *
 * Copyright 2017 Sven Gregori
 * Released under MIT License
 *
 * ATmega328 DIP28 Pinout
 *   1  /Reset
 *   2  PD0 UART RXD
 *   3  PD1 UART TXD
 *   4  PD2 INT0 button
 *   5  PD3 (unused)
 *   6  PD4 LED BLE connect
 *   7  VCC
 *   8  GND
 *   9  PB6 (unused)
 *  10  PB7 (unused)
 *  11  PD5 (unused)
 *  12  PD6 OC0A PWM
 *  13  PD7 LED BLE setup
 *  14  PB0 BLE reset
 *
 *  15  PB1 BLE RDYN
 *  16  PB2 BLE REQN / SPI SS
 *  17  PB3 BLE MOSI / SerProg MOSI
 *  18  PB4 BLE MISO / SerProg MISO
 *  19  PB5 BLE SCK / SerProg SCK
 *  20  AVCC
 *  21  AREF
 *  22  GND
 *  23  PC0 (unused)
 *  24  PC1 (unused)
 *  25  PC2 (unused)
 *  26  PC3 (unused)
 *  27  PC4 (unused)
 *  28  PC5 (unused)
 *
 */
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "uart.h"
#include "nrf.h"
#include "spi.h"

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "<unavailable>"
#endif

static const char string_ble_banner[] PROGMEM = "\f\r\n"
    "  m     m\r\n"
    "   \"m m\"  nRF8001 Example System\r\n"
    "    \"#\"   Built " BUILD_TIMESTAMP "\r\n"
    "     #\r\n"
    "     #    sgreg.fi - MIT License\r\n\r\n";

static const char string_setup_done[] PROGMEM = "Setup done: ";
static const char string_reset[] PROGMEM      = "\r\nResetting BLE module\r\n";

static uint8_t button_interrupt;

/**
 * Parse and handle debug interface.
 *
 * Simple debug interface with single button press.
 *
 * @param c character to parse
 * @return none
 */
static void
parse_input(char c)
{
    switch (c) {
        case 'r':   /* reset BLE module */
            uart_print_pgm(string_reset);
            nrf_reset_module();
            break;

        case 't':   /* get module temperature ..because why not. */
            nrf_print_temperature();
            break;
    }
}


/*
 * Main
 */

int
main(void)
{
    volatile char c = 0;
    int8_t ret;

    /* Port setup */
    /* Set PB0 (nRF reset), PB2 (RDYN), PB3 (MOSI) and PB5 (SCK) as output */
    DDRB = (1 << PB0) | (1 << PB2) | (1 << PB3) | (1 << PB5);
    /* Set all outputs high and enable pull-ups on inputs / unused pins */
    PORTB = 0xff;
    /* Port C is unused, set all pins to input with pull-up */
    DDRC = 0x00;
    PORTC = 0xff;
    /* Set PD4 (setup LED) and PD6 (connect LED) as output */
    DDRD = (1 << PD4) | (1 << PD6) | (1 << PD7);
    /* Set all outputs low, enable pull-up on all inputs and unused pins */
    PORTD = (uint8_t) ~((1 << PD4) | (1 << PD6) | (1 << PD7));
    
    /* Make sure pull-up disable (PUD) is not set */
    MCUCR &= ~(1 << PUD);

    /* Initialize UART and print banner */
    uart_init(UART_BRATE_9600_8MHZ);
    uart_print_pgm(string_ble_banner);

    /* Initialize SPI */
    spi_init();

    /*
     * Set up nRF8001 module.
     * nRF states and pins should be properly setup on device initialization.
     * Optionally, use nrf_reset_module() here to be on the safe side
     */
    nrf_tx_map_pipes();
    ret = nrf_setup();

    uart_print_pgm(string_setup_done);
    uart_putint(ret, 1);
    uart_newline();

    /* Enable INT0 interrupt on any logic change */
    EICRA |= (1 << ISC00);
    EIMSK |= (1 << INT0);

    /* Enable all interrupts */
    sei();

    while (1) {
        /* Check button interrupt state */
        if (button_interrupt) {
            nrf_send_button_data(!(PIND & (1 << PD2)));
            button_interrupt = 0;
        }

        /* Check UART command */
        c = uart_get_inbuf();
        if (c != 0) {
            uart_putchar(c);
            parse_input(c);
            uart_reset_inbuf();
        }

        /* Check nRF */
        if (nrf_connect_state == NRF_STATE_DISCONNECT) {
            nrf_advertise();
            nrf_connect_state = NRF_STATE_CONNECTING;
        } else {
            nrf_receive(&rx);
            if (rx.length > 0) {
                nrf_print_rx(&rx);
                nrf_parse(&rx);
                memset(&rx, 0, sizeof(rx));
            } else {
                _delay_ms(20);
            }
        }
    }
}

/**
 * INT0 interrupt handler.
 * Set flag that interrupt happened.
 */
SIGNAL(INT0_vect)
{
    button_interrupt = 1;
}

