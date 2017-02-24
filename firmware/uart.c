/*
 *
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "uart.h"

static volatile char uart_inbuf;


SIGNAL(USART_RX_vect)
{
    char c;

    c = uart_getchar();
    uart_inbuf = c;
}


void
uart_init(int16_t brate)
{
    UBRR0H = (brate >> 8) & 0xff;
    UBRR0L = (brate     ) & 0xff;

    UCSR0B = (1 << RXCIE0)  /* enable RX available int */
           | (0 << TXCIE0)  /* disable TX done int */
           | (0 << UDRIE0)  /* disable data reg empty int */
           | (1 << RXEN0)   /* enable RX */
           | (1 << TXEN0)   /* enable TX */
           | (0 << UCSZ02)  /* no 9th data bit */
           | 0;             /* ignore rest */

    UCSR0C = (0 << UMSEL01) /* asynchronous mode */
           | (0 << UMSEL00) /*          "        */
           | (0 << UPM01)   /* no parity */
           | (0 << UPM00)   /*     "     */
           | (0 << USBS0)   /* 1 stop bit */
           | (1 << UCSZ01)  /* 8 data bits */
           | (1 << UCSZ00)  /*      "      */
           | (0 << UCPOL0); /* no polarity */
}


void
uart_putchar(char d)
{
    while (!(UCSR0A & (1 << UDRE0))) {
        /* wait for empty tx buffer */
    }
    UDR0 = d;
}


char
uart_getchar(void)
{
    while (!(UCSR0A & (1 << RXC0))) {
        /* wait for data */
    }
    return UDR0;
}


void
uart_newline(void)
{
    uart_putchar('\r');
    uart_putchar('\n');
}


void
uart_print(char *data)
{
    while (*data) {
        uart_putchar(*data++);
    }
}

/**
 * Print given PROGMEM string via UART
 */
void
uart_print_pgm(const char *data)
{
    while (pgm_read_byte(data) != 0x00) {
        uart_putchar(pgm_read_byte(data++));
    }
}

static const char hexvals[] = "0123456789abcdef";
void
uart_puthex(char c)
{
    uart_putchar(hexvals[(c >> 4) & 0x0f]);
    uart_putchar(hexvals[c & 0x0f]);
}


/**
 * Converts a given number digit by digit to a character buffer.
 * Note: the following limits are applied:
 * <ul>
 * <li>negative numbers are converted to positive numbers</li>
 * <li>base 10 conversion only</li>
 * <li>maximum number of 9 digits are converted</li>
 * </ul>
 *
 * @param number Number to be converted.
 * @param buf    Buffer the digits are put.
 * @return Number of digits converted.
 */
static uint8_t
tobuf(int32_t number, char *buf)
{
	int32_t next_digit;
	int8_t i;

	if (number < 0) {
		number *= -1;
	}

	for (i = 0; i < 10 && number; i++) {
		next_digit = (int32_t) (number / 10);
		buf[i] = ((char) (number - next_digit * 10)) + '0';
		number = next_digit;
	}
	return --i;
}


/**
 * Prints a given signed base 10 number via UART.
 * A number of minumum digits can be specified. If the given number has
 * less digits, the output is filled with leading zeros. If the number
 * has more digits, all digits are printed.
 * Note: this function uses the number_to_buf() method, therefore the
 * same limits apply here.
*
 * @param number Number to be printed.
 * @param digits Minimum number of digits to print.
 */
void
uart_putint(int32_t number, int8_t digits)
{
	char buf[10];
	int8_t i;

	if (number < 0) {
		uart_putchar('-');
		number *= -1;
	}

	i = tobuf(number, buf);

	while (i < --digits) {
		uart_putchar('0');
	}
	while (i >= 0) {
		uart_putchar(buf[i--]);
	}
}


uint8_t
char2hex(char c)
{
    uint8_t ret = 0xff;

    if (c >= '0' && c <= '9') {
        ret = c - '0';
    } else if (c >= 'a' && c <= 'f') {
        ret = c - 'a' + 10;
    }

    return ret;
}


char
uart_get_inbuf(void)
{
    return uart_inbuf;
}

void
uart_reset_inbuf(void)
{
    uart_inbuf = 0;
}

