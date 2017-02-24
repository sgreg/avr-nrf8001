/*
 * nRF8001 handling code
 * Part of the Bluetooth LE example system
 *
 * Copyright 2017 Sven Gregori
 * Released under MIT License
 *
 */
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "uart.h"
#include "nrf.h"
#include "spi.h"
#include "nrf/services.h"

/* String constants stored in PROGMEM */
static const char string_advertising[] PROGMEM  = "Starting advertising\r\n";
static const char string_pipe_closed[] PROGMEM  = "Pipe not open\r\n";
static const char string_pipes_open[] PROGMEM   = "Open Pipes: ";
static const char string_connection[] PROGMEM   = "Connection from: ";
static const char string_received[] PROGMEM     = "Received unhandled data: ";
static const char string_temperature[] PROGMEM  = "Temperature: ";
static const char string_celsius[] PROGMEM      = " C\r\n";

/* BLE connection state */
uint8_t nrf_connect_state = NRF_STATE_DISCONNECT;
/* Global rx data structure */
struct nrf_rx rx;

static uint8_t opmode;
static uint8_t pipes;
static uint64_t pipes_open;
static struct nrf_tx tx;
static const struct service_pipe_mapping service_pipe_map[] = SERVICES_PIPE_TYPE_MAPPING_CONTENT;
static const struct nrf_setup_data setup_data[NB_SETUP_MESSAGES] PROGMEM = SETUP_MESSAGES_CONTENT;


/**
 * Reset the nRF8001 module.
 *
 * Pulls the module's reset pin low and waits for a moment. Afterwards
 * nrf_setup() is called, which will set the pin high again.
 *
 * The return value from nrf_setup() is returned.
 *
 * @param none
 * @return 0 on success, a negative value in case of an error (see nrf_setup())
 */
int8_t
nrf_reset_module(void)
{
    led_setup_off();
    led_connect_off();

    ble_reset_low();
    _delay_ms(10);

    nrf_connect_state = NRF_STATE_DISCONNECT;
    return nrf_setup();
}

/**
 * Setup nRF8001 module.
 *
 * Send all setup data generated from nRFgo Studio in nrf/services.h to the
 * module via SPI and take care that everything is set up properly.
 *
 * If anything goes wrong during the setup phase, the setup process is
 * aborted and the module will not be functional. A negative return value
 * will indicate an error.
 *
 * @param none
 * @return 0 on success, a negative value in case of an error.
 */
int8_t
nrf_setup(void)
{
    uint8_t cnt;
    
    ble_reset_high();
    /* 
     * data sheet says RDYN signal is not valid until 62ms after nRF reset
     * pin goes high. Let's be on the safe side and wait 100ms.
     */
    _delay_ms(100);

    memset(&rx, 0, sizeof(rx));
    nrf_receive(&rx);

    if (rx.data[0] != NRF_EVT_DEVICE_STARTED || rx.data[2] != NRF_ERR_NO_ERROR) {
        return -1;
    }

    opmode = rx.data[1];
    pipes = rx.data[3];

    if (opmode != NRF_OPMODE_SETUP) {
        return -2;
    }

    /* Send all setup data to nRF8001 */
    for (cnt = 0; cnt < NB_SETUP_MESSAGES; cnt++) {
        memset(&rx, 0, sizeof(rx));
        memcpy_P(&tx, &setup_data[cnt].data, sizeof(struct nrf_tx));
        nrf_transmit(&tx, &rx);

        if (rx.length == 0) {
            nrf_print_rx(&rx);
            continue;
        }

        nrf_print_rx(&rx);

        /* Make sure transaction continue command response event is received */
        if (rx.data[0] != NRF_EVT_CMD_RESPONSE ||
            rx.data[1] != NRF_CMD_SETUP ||
            rx.data[2] != ACI_STATUS_TRANSACTION_CONTINUE)
        {
            return -3;
        }
    }
    
    /* Receive all setup command response events */
    do {
        memset(&rx, 0, sizeof(rx));
        nrf_receive(&rx);
        nrf_print_rx(&rx);
    } while (rx.data[0] == NRF_EVT_CMD_RESPONSE &&
             rx.data[1] == NRF_CMD_SETUP &&
             rx.data[2] == ACI_STATUS_TRANSACTION_CONTINUE);

    /* Make sure transaction complete command response event is received */
    if (rx.data[0] != NRF_EVT_CMD_RESPONSE ||
        rx.data[1] != NRF_CMD_SETUP ||
        rx.data[2] != ACI_STATUS_TRANSACTION_COMPLETE)
    {
        return -4;
    }

    /* One last receive loop to wait for DeviceStartedEvent */
    do {
        memset(&rx, 0, sizeof(rx));
        nrf_receive(&rx);
    } while (rx.data[0] != NRF_EVT_DEVICE_STARTED);

    nrf_print_rx(&rx);

    if (rx.data[2] != NRF_ERR_NO_ERROR) {
        return -5;
    }

    opmode = rx.data[1];
    pipes = rx.data[3];

    led_setup_on();
    
    return 0;
}


/**
 * Start advertising, waiting for remote side to connect.
 *
 * @param none
 * @return none
 */
void
nrf_advertise(void)
{
    data16_t timeout;
    data16_t advival;

    memset(&tx, 0, sizeof(tx));

    timeout.word = 0;
    advival.word = 128;

    tx.length = 5;
    tx.command = NRF_CMD_CONNECT;
    /* send LSB first */
    tx.data[0] = timeout.lsb;
    tx.data[1] = timeout.msb;
    tx.data[2] = advival.lsb;
    tx.data[3] = advival.msb;

    nrf_send(&tx);
}


/*
 * ACI pipe closing handling
 *
 * There is no PipeStatusEvent on disconnect, so pipes opened by a remote
 * client (most likely notification pipes) have to be manually removed
 * from the pipes_open variable.
 *
 * This will be handled by using the nrf_tx_pipe_map bitfield that is
 * initialized during start to contain all pipe numbers that are defined
 * as local storage and TX pipe. Once a disconnect event is received, the
 * internal pipes_open bitfield is adjusted with it.
 */

/** bitfield of ACI pipes to be closed on remote disconnect */
uint64_t nrf_tx_pipe_map;

/**
 * Set up nrf_tx_pipe_map bitfield.
 *
 * All pipes defined as ACI_STORE_LOCAL and ACI_TX are added to the bitfield.
 *
 * @param none
 * @return none
 */
void
nrf_tx_map_pipes(void)
{
    uint8_t i;

    for (i = 0; i < NUMBER_OF_PIPES; i++) {
        if (service_pipe_map[i].store == ACI_STORE_LOCAL &&
            service_pipe_map[i].type == ACI_TX)
        {
            nrf_tx_pipe_map |= 1 << (i + 1);
        }
    }
}

/**
 * Internally close all ACI pipes.
 *
 * Pipes that where opened by a remote client and not the nRF8001 module
 * need to be closed manually.
 *
 * @param none
 * @return none
 */
void
nrf_close_tx_pipes(void)
{
    uint8_t i;

    pipes_open &= ~(nrf_tx_pipe_map);
    uart_print_pgm(string_pipes_open);
    for (i = 1; i <= NUMBER_OF_PIPES; i++) {
        if (pipes_open & (1 << i)) {
            uart_putint(i, 1);
            uart_putchar(' ');
        }
    }
    uart_newline();
}



/* Dummy tx and rx data structures */
static struct nrf_tx dummy_tx;
static struct nrf_rx dummy_rx;

/**
 * nRF8001 transmission function.
 * Send and simultaniously receive data to and from the nRF8001 module.
 * Note, received data is always related to a previous transmission or
 * an otherwise asynchronous event, but will never be the direct result of
 * the call in progress. If data is expected to be sent from the nRF8001
 * module afterwards, a subsequent transmit call has to be made in order to
 * receive the data. But keep in mind that asynchronous data might be sent,
 * so ignoring the received data during a send-only operation is not
 * recommended.
 *
 * In cases where received data really can be safely ignored, or no data
 * has to be trasmitted in the first place, but only received, NULL values
 * can be given for the tx and rx parameter respectively. In addition, two
 * macros exist as a shortcut for these cases:
 *      nrf_send(struct nrf_tx *)       for send-only
 *      nrf_receive(struct nrf_rx *)    for receive-only
 *
 * In addition, a third macro, nrf_txrx(struct nrf_tx *, struct nrf_rx *)
 * exists to have a sequential send and receive operation, i.e calling
 * a send-only nrf_transmit() followed directly by a receive-only one.
 *
 * @param tx nrf_tx structure for sending, make sure the data buffer is
 *           filled with zeroes. Can be NULL
 * @param rx nrf_rx structure for receiving, can be NULL
 * @return 0
 *
 */
int8_t
nrf_transmit(struct nrf_tx *tx, struct nrf_rx *rx)
{
    uint8_t i;

    reqn_set_low();
    while (rdyn_is_high()) {
        /* wait */
    }

    /*
     * Check if given tx struct is NULL and only rx is of interest.
     * Use global dummy_tx struct (all fields zero) for sending.
     */
    if (tx == NULL) {
        tx = &dummy_tx;
    }

    /*
     * Check if given rx struct is NULL and only tx is of interst.
     * Receive into global dummy_rx structure and ignore it.
     */
    if (rx == NULL) {
        memset(&dummy_rx, 0, sizeof(dummy_rx));
        rx = &dummy_rx;
    }

    /*
     * Each ACI transmission consists of at least two bytes (packet length
     * and opcode). Each receiving package also has at least two bytes
     * (debug byte and receiving length, which might be zero if there is
     * no actual data available).
     */
    rx->debug  = spi_transmit(tx->length);
    rx->length = spi_transmit(tx->command);
    
    /* Send and receive data while there is data is to send or receive */
    for (i = 0; i < tx->length - 1 || i < rx->length; i++) {
        rx->data[i] = spi_transmit(tx->data[i]);
    }

    reqn_set_high();
    while (rdyn_is_low()) {
        /* wait */
    }

    /*
     * Make sure REQN inactive time (Tcwh, nRF datasheet page 26) is given.
     * Experienced some timing issues, i.e. empty events read after requesting
     * data (simplest case reading temperature) without a small delay here.
     */
    _delay_ms(1);

    return 0;
}

/**
 * Parse received data from nRF8001 module.
 *
 * @param rx Data received from nRF8001 module
 * @return none
 */
void
nrf_parse(struct nrf_rx *rx)
{
    uint8_t i;

    if (rx->length == 0) {
        return;
    }

    switch (rx->data[0]) {
        case NRF_EVT_CMD_RESPONSE:
            if (rx->data[1] == NRF_CMD_CONNECT &&
                rx->data[2] == NRF_ERR_NO_ERROR)
            {
                uart_print_pgm(string_advertising);
            }
            break;

        case NRF_EVT_CONNECTED:
            nrf_connect_state = NRF_STATE_CONNECTED;
            led_connect_on();

            /* Print MAC address of new connection */
            uart_print_pgm(string_connection);
            for (i = 0; i < 5; i++) {
                uart_puthex(rx->data[7 - i]);
                uart_putchar(':');
            }
            uart_puthex(rx->data[8 - i]);
            uart_newline();
            break;

        case NRF_EVT_DISCONNECTED:
            led_connect_off();
            nrf_close_tx_pipes();
            nrf_connect_state = NRF_STATE_DISCONNECT;
            break;

        case NRF_EVT_PIPE_STATUS:
            /* Assemble pipes_open information from received pipe status */
            for (pipes_open = 0, i = 0; i < 8; i++) {
                pipes_open |= rx->data[i+1] << (8 * i);
            }

            uart_print_pgm(string_pipes_open);
            for (i = 1; i <= NUMBER_OF_PIPES; i++) {
                if (pipes_open & (1 << i)) {
                    uart_putint(i, 1);
                    uart_putchar(' ');
                }
            }
            uart_newline();
            break;

        case NRF_EVT_DATA_RECEIVED:
            if (rx->data[1] == PIPE_EXAMPLE_SERVICE_PWM_DUTY_CYCLE_RX) {
                if (rx->data[2] > 0) {
                    OCR0A = rx->data[2];
                    TCCR0A = 0x83; // Fast PWM (mode 4), clear on match and set on bottom
                    TCCR0B = 0x04; // Fast PWM (mode 4), prescaler 256
                } else {
                    TCCR0A = 0x00;
                    TCCR0B = 0x00;
                    PORTD &= ~(1 << PD6);
                }
            }
            break;

        default:
            uart_print_pgm(string_received);
            for (i = 0; i < rx->length; i++) {
                uart_putchar(' ');
                uart_puthex(rx->data[i]);
            }
            uart_newline();
            
    }
}


/**
 * Send the given button state to the remote side.
 *
 * If the button state tx pipe is not open (i.e. remote side is not waiting
 * for notifications about the new state), nothing will happen.
 *
 * @param button Button state to send
 * @return 0 on success, -1 if tx pipe is closed
 */
int8_t
nrf_send_button_data(uint8_t button)
{
    if (!(pipes_open & (1 << PIPE_EXAMPLE_SERVICE_BUTTON_STATE_TX))) {
        uart_print_pgm(string_pipe_closed);
        return -1;
    }

    memset(&tx, 0, sizeof(tx));

    tx.length = 3;
    tx.command = NRF_CMD_SEND_DATA;
    tx.data[0] = PIPE_EXAMPLE_SERVICE_BUTTON_STATE_TX;
    tx.data[1] = button;

    nrf_send(&tx);

    return 0;
}

/**
 * Print the given rx struct content.
 *
 * Mainly meant for debug
 *
 * @param rx Received data to print
 * @return none
 */
void
nrf_print_rx(struct nrf_rx *rx)
{
    uint8_t i;

    uart_putchar('[');
    uart_putint(rx->length, 2);
    uart_putchar(']');
    
    for (i = 0; i < rx->length; i++) {
        uart_putchar(' ');
        uart_puthex(rx->data[i]);
    }
    uart_newline();
}

/**
 * Print the nRF8001 module's on-chip temperature.
 *
 * Send temperature command to the module and receive it,
 * then output it via UART.
 *
 * @param none
 * @return none
 */
void
nrf_print_temperature(void)
{
    data16_t raw;

    memset(&tx, 0, sizeof(tx));
    memset(&rx, 0, sizeof(rx));

    tx.length = 0x01;
    tx.command = NRF_CMD_GET_TEMPERATURE;
    nrf_txrx(&tx, &rx);

    raw.lsb = rx.data[3];
    raw.msb = rx.data[4];

    uart_print_pgm(string_temperature);
    uart_putint(raw.word >> 2, 1);
    uart_putchar('.');
    switch (raw.word & 0x03) {
        case 0:
            uart_putchar('0');
            uart_putchar('0');
            break;
        case 1:
            uart_putchar('2');
            uart_putchar('5');
            break;
        case 2:
            uart_putchar('5');
            uart_putchar('0');
            break;
        case 3:
            uart_putchar('7');
            uart_putchar('5');
            break;
    }

    uart_print_pgm(string_celsius);
}

