#define UART_BAUD 115200
#define F_CPU 5000000
#include "qqq_uart.h"
#include "qqq_rtc.h"

void setup()
{
	//set f_cpu (Max 1.8V 5MHz, 2.7V 10Mhz, 3.3V 13.33Mhz, 4.5V 20MHz)
	#ifndef F_CPU
	#define F_CPU 3333333
	#endif
	#if   F_CPU == 10000000
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm;
	#elif F_CPU ==  5000000
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_4X_gc | CLKCTRL_PEN_bm;
	#elif F_CPU ==  3333333
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_6X_gc | CLKCTRL_PEN_bm;
	#elif
	error "unsupported F_CPU value"
	#endif

	uart_init(UART_DEFAULT_PORT);
	uart_printf("\n\nstart\n");

	rtc_init();

	sei();
}

void loop() {
		uart_printf("\nstandby");
		rtc_standby_ms(5000);

		uart_printf("\nidle300khz");
		rtc_idle300khz_ms(5000);

		uart_printf("\nidle32khz");
		rtc_idle32khz_ms(5000);
		
		uart_printf("\nidle");
		rtc_idle_ms(5000);

		uart_printf("\ndelay\n");
		_delay_ms(5000);
	}

	return 0;
}
