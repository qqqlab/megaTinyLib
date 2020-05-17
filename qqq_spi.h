
#ifndef SPI_SS_PORT
#error "SPI_SS_PORT not defined"
#endif
#ifndef SPI_SS_PIN_bm
#error "SPI_SS_PIN_bm not defined"
#endif
#ifndef SPI_ALTERNATE_PINS
#define SPI_ALTERNATE_PINS 0
#endif

#include <avr/io.h>

void spi_ss()
{
	SPI_SS_PORT.OUTCLR = SPI_SS_PIN_bm; //output low SS
	SPI0.CTRLA |= SPI_ENABLE_bm;  //enable SPI
}

void spi_nss() 
{
	SPI0.CTRLA &= ~SPI_ENABLE_bm; //disable SPI
	SPI_SS_PORT.OUTSET = SPI_SS_PIN_bm; //output high SS
}

uint8_t spi_transfer(uint8_t data)
{
	SPI0.DATA = data;
	while (!(SPI0.INTFLAGS & SPI_RXCIF_bm));
	return SPI0.DATA;
}

void spi_setup() {
//set alternate pins (not all devices have alternate pins)
#ifdef PORTMUX_SPI0_ALTERNATE_gc
  #if SPI_ALTERNATE_PINS
	  PORTMUX.CTRLB |= PORTMUX_SPI0_ALTERNATE_gc; //select alternate pins
  #else
	  PORTMUX.CTRLB &= ~PORTMUX_SPI0_ALTERNATE_gc; //select regular pins
  #endif
#endif
	
	SPI_SS_PORT.DIRSET = SPI_SS_PIN_bm ;
	spi_nss();
	SPI0.CTRLA = 
	  1 << SPI_CLK2X_bp  /* Enable Double Speed */
	| 0 << SPI_DORD_bp   /* Data Order Setting */
	| 0 << SPI_ENABLE_bp /* Enable Module */  //enabled with spi_ss()
	| 1 << SPI_MASTER_bp /* SPI module in master mode */
	| SPI_PRESC_DIV4_gc; /* System Clock / 4 */

	SPI0.CTRLB = 
	  0 << SPI_BUFEN_bp /* Buffer Mode Enable */
	| 0 << SPI_BUFWR_bp /* Buffer Write Mode */
	| SPI_MODE_0_gc     /* SPI Mode 0 */
	| 1 << SPI_SSD_bp;  /* Slave Select Disable */ //manual control of SS pin

	SPI0.INTCTRL = 
	  0 << SPI_DREIE_bp  /* Data Register Empty Interrupt Enable */
	| 0 << SPI_IE_bp     /* Interrupt Enable */
	| 0 << SPI_RXCIE_bp  /* Receive Complete Interrupt Enable */
	| 0 << SPI_SSIE_bp   /* Slave Select Trigger Interrupt Enable */
	| 0 << SPI_TXCIE_bp; /* Transfer Complete Interrupt Enable */
}

