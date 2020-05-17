//#############################################################
// SPI blocking driver mode0
//#############################################################

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

//find out what device we have
#if defined(_AVR_ATTINY3216_H_INCLUDED) || defined(_AVR_ATTINY1616_H_INCLUDED)
	//attinyx16 1 series soic-20, alternate SPI port: SCK=PC0 MISO=PC1 MOSI=PC2 NSS=PC3
	#define QQQ_ATTINYx16
#elif defined(_AVR_ATTINY1614_H_INCLUDED) || defined(_AVR_ATTINY814_H_INCLUDED)  || defined(_AVR_ATTINY414_H_INCLUDED)
	//attinyx14 1 series soic-14
	#define QQQ_ATTINYx14
#else
	#error "unknown micro controller in qqq_spi.h"
#endif

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
	//setup pins
#if defined(QQQ_ATTINYx16) && !SPI_ALTERNATE_PINS
	//14 pin - regular pins
	PORTMUX.CTRLB &= ~PORTMUX_SPI0_ALTERNATE_gc; 
	PORTA.DIRSET = PIN1_bm | PIN3_bm; //MOSI + SCK
	PORTA.DIRCLR = PIN2_bm; //MISO
#elif defined(QQQ_ATTINYx16) && SPI_ALTERNATE_PINS
	//14 pin - alternate pins
	PORTMUX.CTRLB |= PORTMUX_SPI0_ALTERNATE_gc; //select alternate pins
	PORTC.DIRSET = PIN2_bm | PIN0_bm; //MOSI + SCK
	PORTC.DIRCLR = PIN1_bm; //MISO
#elif defined(QQQ_ATTINYx14)
	//14 pin - regular pins (no alternate pins)
	PORTA.DIRSET = PIN1_bm | PIN3_bm; //MOSI + SCK
	PORTA.DIRCLR = PIN2_bm; //MISO     
#endif

    //setup SS pin
	SPI_SS_PORT.DIRSET = SPI_SS_PIN_bm ;
	spi_nss();

	//setup SPI
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
