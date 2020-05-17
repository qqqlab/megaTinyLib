/*################################################################
UART blocking output only uart driver
##################################################################
Config:
#define UART_BAUD 115200
#define UART_ALTERNATE_PINS 0
#define UART_DISABLE 0
#include <qqq_uart.h>
void setup() {uart_setup();}
void loop() {uart_printf("hello\n");}
################################################################*/

#ifndef UART_H_
#define UART_H_

//set defaults
#ifndef UART_BAUD
  #define UART_BAUD 115200
#endif
#ifndef UART_ALTERNATE_PINS
  #define UART_ALTERNATE_PINS 0
#endif
#ifndef UART_DISABLE
  #define UART_DISABLE 0
#endif

#if UART_DISABLE
	//saves approx 2100 bytes flash
	#define uart_sendchar(...)
	#define uart_printf(...)
	#define uart_init(...)
#else



#include <avr/io.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


// Send Character
void uart_sendchar(uint8_t c)
{
	USART0_TXDATAL = c;
	while((USART0_STATUS &(1<<USART_TXCIF_bp)) == 0);
	USART0_STATUS = (1<<USART_TXCIF_bp);			// clear flag
}

void uart_printf(const char* fmt, ...) {
	char buf[80];
	va_list args;
	va_start (args, fmt);
	vsnprintf_P(buf, sizeof(buf), (const char *) fmt, args);
	va_end(args);
	char *p = buf;
	while(*p) {
		uart_sendchar(*p);
		p++;
	}
}

void uart_setup()
{
#if UART_ALTERNATE_PINS
  //TX:PA1 RX:PA2 (attinyx14:pin11,12)
	PORTMUX.CTRLB |= PORTMUX_USART0_bm;
	PORTA.DIRSET = PIN1_bm;
	PORTA.DIRCLR = PIN2_bm;
#else
	//TX:PB2 RX:PB3 (attinyx14:pin7,6)
	PORTMUX.CTRLB &= ~PORTMUX_USART0_bm;
	PORTB.DIRSET = PIN2_bm;
	PORTB.DIRCLR = PIN3_bm;
#endif

	USART0.CTRLB = USART_TXEN_bm;
	USART0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;
	USART0.CTRLA = 0;

	USART0.BAUD = ((8*F_CPU/UART_BAUD)+1)/2; //Calculate baud rate setting,	with rounding	BAUD = 64*CLK_PER/S*fBAUD = 64*3333333/16*9600 = 1388.88 -> 1389
	USART0.STATUS = USART_RXCIF_bm | USART_TXCIF_bm | USART_DREIF_bm | USART_RXSIF_bm | USART_ISFIF_bm | USART_BDF_bm; //clear interrupt flags
}

#endif /* ifndef UART_DISABLED */

#endif /* UART_H_ */
