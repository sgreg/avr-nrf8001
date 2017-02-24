#ifndef _SPI_H_
#define _SPI_H_

#include <stdint.h>

void spi_init(void);
uint8_t spi_transmit(uint8_t data);

#endif /* _SPI_H_ */
