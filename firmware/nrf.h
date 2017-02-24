/*
 * nRF8001 handling code
 * Part of the Bluetooth LE example system
 *
 * Copyright 2017 Sven Gregori
 * Released under MIT License
 *
 */
#ifndef _NRF_H_
#define _NRF_H_

#include "nrf/services.h"

#define NRF_OPMODE_TEST     0x01
#define NRF_OPMODE_SETUP    0x02
#define NRF_OPMODE_STANDBY  0x03

#define NRF_STATE_DISCONNECT 0x00
#define NRF_STATE_CONNECTING 0x01
#define NRF_STATE_CONNECTED  0x02

#define NRF_CMD_SETUP           0x06
#define NRF_CMD_GET_TEMPERATURE 0x0c
#define NRF_CMD_CONNECT         0x0f
#define NRF_CMD_SEND_DATA       0x15
#define NRF_ERR_NO_ERROR        0x00
#define NRF_EVT_DEVICE_STARTED  0x81
#define NRF_EVT_CMD_RESPONSE    0x84
#define NRF_EVT_CONNECTED       0x85
#define NRF_EVT_DISCONNECTED    0x86
#define NRF_EVT_PIPE_STATUS     0x88
#define NRF_EVT_DATA_RECEIVED   0x8c

typedef union {
    uint8_t byte[2]; /* byte[0] = lsb, byte[1] = msb */
    uint16_t word;
    int16_t s_word;
    struct {
        uint8_t lsb;
        uint8_t msb;
    };
} data16_t;

struct nrf_setup_data {
    uint8_t status;
    uint8_t data[32];
};

struct nrf_tx {
    uint8_t length;
    uint8_t command;
    uint8_t data[30];
};

struct nrf_rx {
    uint8_t debug;
    uint8_t length;
    uint8_t data[30];
};


int8_t nrf_reset_module(void);
int8_t nrf_setup(void);
void nrf_advertise(void);

int8_t nrf_transmit(struct nrf_tx *tx, struct nrf_rx *rx);
#define nrf_send(tx) nrf_transmit(tx, NULL)
#define nrf_receive(rx) nrf_transmit(NULL, rx)
#define nrf_txrx(tx, rx) do { nrf_transmit(tx, NULL); nrf_transmit(NULL, rx); } while (0);

int8_t nrf_send_button_data(uint8_t button);
void nrf_parse(struct nrf_rx *rx);
void nrf_print_rx(struct nrf_rx *rx);
void nrf_print_temperature(void);

extern uint8_t nrf_connect_state;
extern struct nrf_rx rx;

#define led_setup_on()      do { PORTD |= 0x80; } while (0)
#define led_setup_off()     do { PORTD &= ~(0x80); } while (0)
#define led_connect_on()    do { PORTD |= 0x10; } while (0)
#define led_connect_off()   do { PORTD &= ~(0x10); } while (0)

#define ble_reset_high()    do { PORTB |= 0x01; } while(0)
#define ble_reset_low()     do { PORTB &= ~(0x01); } while (0)
#define reqn_set_high()     do { PORTB |= 0x04; } while (0)
#define reqn_set_low()      do { PORTB &= ~(0x04); } while (0)
#define rdyn_is_high()      (PINB & 0x02)
#define rdyn_is_low()       (!(PINB & 0x02))

struct service_pipe_mapping {
    aci_pipe_store_t store;
    aci_pipe_type_t type;
};

void nrf_tx_map_pipes(void);

#endif /* _NRF_H_ */
