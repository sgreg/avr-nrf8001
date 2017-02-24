#ifndef _AVRLIB_UART_H_
#define _AVRLIB_UART_H_
#include <stdint.h>

/* baud rate values for 8MHz CLK and U2Xn=0 */
#define UART_BRATE_2400_8MHZ 207
#define UART_BRATE_9600_8MHZ 51
#define UART_BRATE_19200_8MHZ 25

void uart_init(int16_t brate);

void uart_putchar(char d);
char uart_getchar(void);
void uart_newline(void);
void uart_print(char *data);
void uart_print_pgm(const char *data);
void uart_puthex(char c);
void uart_putint(int32_t number, int8_t digits);

char uart_get_inbuf(void);
void uart_reset_inbuf(void);
#endif /* _AVRLIB_UART_H_ */

