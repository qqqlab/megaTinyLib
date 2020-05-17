//################################################################
// PIT Periodic Interrupt Timer
//################################################################

volatile uint8_t pit_interrupt_cnt = 0;

//#define PIT_ISR() uart_sendchar('p')

#ifndef PIT_ISR
  #define PIT_ISR()
#endif

void pit_disable_inputs() {
	//disable all inputs
	//14 pin devices
	PORTA.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc; //UPDI
	PORTA.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTB.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc; 
	PORTB.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTB.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTB.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
	//for 20 pin devices
#if defined(_AVR_ATTINY3216_H_INCLUDED) || defined(_AVR_ATTINY1616_H_INCLUDED)
	PORTB.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTB.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTC.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTC.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTC.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTC.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
#endif
}

//set period in milliseconds, returns actual interval set
uint16_t pit_setPeriodMs(uint16_t ms)
{
  if(ms>=24000) {RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm; return 32000;}
  if(ms>=12000) {RTC.PITCTRLA = RTC_PERIOD_CYC16384_gc | RTC_PITEN_bm; return 16000;}
  if(ms>=6000) {RTC.PITCTRLA = RTC_PERIOD_CYC8192_gc | RTC_PITEN_bm; return 8000;}
  if(ms>=3000) {RTC.PITCTRLA = RTC_PERIOD_CYC4096_gc | RTC_PITEN_bm; return 4000;}
  if(ms>=1500) {RTC.PITCTRLA = RTC_PERIOD_CYC2048_gc | RTC_PITEN_bm; return 2000;}
  if(ms>=750) {RTC.PITCTRLA = RTC_PERIOD_CYC1024_gc | RTC_PITEN_bm; return 1000;}
  if(ms>=375) {RTC.PITCTRLA = RTC_PERIOD_CYC512_gc | RTC_PITEN_bm; return 500;}
  if(ms>=188) {RTC.PITCTRLA = RTC_PERIOD_CYC256_gc | RTC_PITEN_bm; return 250;}
  if(ms>=94) {RTC.PITCTRLA = RTC_PERIOD_CYC128_gc | RTC_PITEN_bm; return 125;}
  if(ms>=47) {RTC.PITCTRLA = RTC_PERIOD_CYC64_gc | RTC_PITEN_bm; return 63;}
  if(ms>=24) {RTC.PITCTRLA = RTC_PERIOD_CYC32_gc | RTC_PITEN_bm; return 31;}
  if(ms>=12) {RTC.PITCTRLA = RTC_PERIOD_CYC16_gc | RTC_PITEN_bm; return 16;}
  if(ms>=6) {RTC.PITCTRLA = RTC_PERIOD_CYC8_gc | RTC_PITEN_bm; return 8;}
  RTC.PITCTRLA = RTC_PERIOD_CYC4_gc | RTC_PITEN_bm; return 4;
}

//set period in milliseconds, returns actual interval set
uint16_t pit_setup(uint16_t ms) {
	RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;  // Internal 1kHz OSC
	RTC.PITINTCTRL = RTC_PI_bm; // Enable PIT Interrupts
	return pit_setPeriodMs(ms);
}

ISR(RTC_PIT_vect)
{
	RTC.PITINTFLAGS = RTC_PI_bm; //clear interrupt flag
	pit_interrupt_cnt++;
	PIT_ISR();
}
