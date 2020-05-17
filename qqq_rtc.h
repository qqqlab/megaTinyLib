//################################################################
// RTC  Delay using the RTC counter and sleep
//################################################################
// Benchmark at 3.3v 5Mhz
// delay       2.2 mA  cpu spin loop
// idle        1.1 mA  cpu stopped, peripherals at full speed    
// idle300khz  0.5 mA  cpu stopped, peripherals at 300 kHz
// idle32khz   0.1 mA  cpu stopped, peripherals at 32 kHz
// standby     0.1 mA  cpu stopped, peripherals stopped
// power down - can not use, RTC stopped
//################################################################
volatile bool rtc_delay_wake;

//run RTC of 1kHz internal osc
void rtc_setup() {
	while(RTC.STATUS & (RTC_CTRLABUSY_bm | RTC_CMPBUSY_bm | RTC_CNTBUSY_bm)); //wait for busy to clear
	RTC.CTRLA = RTC_RUNSTDBY_bm | RTC_PRESCALER_DIV1_gc | RTC_RTCEN_bm;
	RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;
	RTC.INTCTRL = RTC_CMP_bm;
}

void rtc_idle_ms(uint16_t ms) {
	if(ms==0) return;
	SLPCTRL.CTRLA = SLPCTRL_SMODE_IDLE_gc | SLPCTRL_SEN_bm;
	rtc_delay_wake = false;
	RTC.CMP = RTC.CNT + ms;
	while(!rtc_delay_wake) __asm__("sleep");
}

void rtc_idle300khz_ms(uint16_t ms) {
	if(ms==0) return;

	//reduce clock speed to div64
	volatile uint8_t saved = CLKCTRL.MCLKCTRLB;
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_64X_gc | CLKCTRL_PEN_bm;

	//go to idle sleep
	SLPCTRL.CTRLA = SLPCTRL_SMODE_IDLE_gc | SLPCTRL_SEN_bm;
	rtc_delay_wake = false;
	RTC.CMP = RTC.CNT + ms;
	while(!rtc_delay_wake) __asm__("sleep");

	//restore main clock speed
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = saved;
}

void rtc_idle32khz_ms(uint16_t ms) {
	if(ms==0) return;

	//select 32kHz osc as clock
	volatile uint8_t saved = CLKCTRL.MCLKCTRLA;
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSCULP32K_gc;

	//go to idle sleep
	SLPCTRL.CTRLA = SLPCTRL_SMODE_IDLE_gc | SLPCTRL_SEN_bm;
	rtc_delay_wake = false;
	RTC.CMP = RTC.CNT + ms;
	while(!rtc_delay_wake) __asm__("sleep");

	//restore main clock speed
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLA = saved;
}

void rtc_standby_ms(uint16_t ms) {
	if(ms==0) return;
	SLPCTRL.CTRLA = SLPCTRL_SMODE_STDBY_gc | SLPCTRL_SEN_bm;
	rtc_delay_wake = false;
	RTC.CMP = RTC.CNT + ms;
	while(!rtc_delay_wake) __asm__("sleep");
}

ISR(RTC_CNT_vect) {
	rtc_delay_wake = true;
	RTC.INTFLAGS = RTC_CMP_bm | RTC_OVF_bm; //clear both flags
}
