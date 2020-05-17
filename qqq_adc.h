//################################################################
// ADC
//################################################################
#ifndef ADC_H_
#define ADC_H_

#ifndef BANDGAP
#define BANDGAP 1107
#endif

//VREF settle time 25us
//ADC clock 50kHz - 1.5Mhz
#if F_CPU > 12000000L
#define ADC_PRESC_gc ADC_PRESC_DIV16_gc
#elif F_CPU >  6000000L
#define ADC_PRESC_gc ADC_PRESC_DIV8_gc
#elif F_CPU >  3000000L
#define ADC_PRESC_gc ADC_PRESC_DIV4_gc
#else
#define ADC_PRESC_gc ADC_PRESC_DIV2_gc
#endif

#include <util/delay.h>

void adc_disable() {
	ADC0.CTRLA = 0;
}

int16_t temp_read_raw() {
  VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc; //vref 1.1V
  VREF.CTRLB = VREF_ADC0REFEN_bm;
  _delay_us(25); //settle vref 

  ADC0.CTRLB = ADC_SAMPNUM_ACC1_gc; //accumulate 1 sample
  ADC0.CTRLC = ADC_REFSEL_INTREF_gc | ADC_PRESC_gc | ADC_SAMPCAP_bm; //select internal vref | F_CPU/x | select normal 5 pf (0 = normal size, 1 = reduced size)
  ADC0.CTRLD = ADC_INITDLY_DLY32_gc; // >= 32 cycles delay
  ADC0.MUXPOS = ADC_MUXPOS_TEMPSENSE_gc; //select temp sensor
  ADC0.SAMPCTRL = 0x1f; //select >= 32us * fclk_adc ??

  //start conversion
  ADC0.CTRLA = ADC_ENABLE_bm;
  ADC0.COMMAND = ADC_STCONV_bm;
  while(ADC0_COMMAND & ADC_STCONV_bm);

  uint16_t v = ADC0.RES;

  return v;
}

//get temperature in Celcius
int16_t adc_temp_c() {
	VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc; //vref 1.1V
	VREF.CTRLB = VREF_ADC0REFEN_bm;
	//_delay_us(25); //settle vref
	ADC0.CTRLC = ADC_REFSEL_INTREF_gc | ADC_PRESC_gc | ADC_SAMPCAP_bm; //select internal vref | F_CPU/x | select normal 5 pf (0 = normal size, 1 = reduced size)
	ADC0.MUXPOS = ADC_MUXPOS_TEMPSENSE_gc; //select temp sensor
	ADC0.CTRLD = ADC_INITDLY_DLY32_gc; // >= 32 cycles delay
	ADC0.SAMPCTRL = 0x1f; //select >= 32us * fclk_adc ??
	ADC0.CTRLB = ADC_SAMPNUM_ACC1_gc; //accumulate 1 sample

	//start conversion
	ADC0.CTRLA = ADC_ENABLE_bm;
	ADC0.COMMAND = ADC_STCONV_bm;
	while(ADC0_COMMAND & ADC_STCONV_bm);
	uint16_t adc_reading = ADC0.RES; // ADC conversion result with 1.1 V internal reference

/*from datasheet:
	int8_t sigrow_offset = SIGROW.TEMPSENSE1; // Read signed value from signature row
	uint8_t sigrow_gain = SIGROW.TEMPSENSE0; // Read unsigned value from signature row
	uint32_t temp = adc_reading - sigrow_offset;
	temp *= sigrow_gain; // Result might overflow 16 bit variable (10bit+8bit)
	temp += 0x80; // Add 1/2 to get correct rounding on division below
	temp >>= 8; // Divide result to get Kelvin
	uint16_t temperature_in_K = temp;
*/
	//adapted to Celcius
	int8_t sigrow_offset = SIGROW.TEMPSENSE1; // Read signed value from signature row
	uint8_t sigrow_gain = SIGROW.TEMPSENSE0; // Read unsigned value from signature row
	uint32_t temp = adc_reading - sigrow_offset;
	temp *= sigrow_gain; // Result might overflow 16 bit variable (10bit+8bit)
	//temp is now K * 256
	//need to add 0.5K for rounding
	//need to subtract 0.15K to get to Celsius (0C = 273.15K)
	//total: add 0.35K * 256 = 90
	temp += 90; // Add 1/2 to get correct rounding on division below
	temp >>= 8; // Divide result to get Celsius + 273
	int16_t celsius = temp - 273;

	return celsius;
}

//get temperature in Celcius*10 (i.e. 245 is 24.5 Celcius)
int16_t adc_temp_c10() {
	VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc; //vref 1.1V
	VREF.CTRLB = VREF_ADC0REFEN_bm;
	//_delay_us(25); //settle vref
	ADC0.CTRLC = ADC_REFSEL_INTREF_gc | ADC_PRESC_gc | ADC_SAMPCAP_bm; //select internal vref | F_CPU/x | select normal 5 pf (0 = normal size, 1 = reduced size)
	ADC0.MUXPOS = ADC_MUXPOS_TEMPSENSE_gc; //select temp sensor
	ADC0.CTRLD = ADC_INITDLY_DLY32_gc; // >= 32 cycles delay
	ADC0.SAMPCTRL = 0x1f; //select >= 32us * fclk_adc ??
	ADC0.CTRLB = ADC_SAMPNUM_ACC64_gc; //accumulate 64 samples (16 bit result)

	//start conversion
	ADC0.CTRLA = ADC_ENABLE_bm;
	ADC0.COMMAND = ADC_STCONV_bm;
	while(ADC0_COMMAND & ADC_STCONV_bm);
	uint16_t adc_reading = ADC0.RES; // ADC conversion result with 1.1 V internal reference

	int8_t sigrow_offset = SIGROW.TEMPSENSE1; // Read signed value from signature row
	int16_t sigrow_offset64 = sigrow_offset * 64;
	uint8_t sigrow_gain = SIGROW.TEMPSENSE0; // Read unsigned value from signature row
	uint32_t temp = adc_reading - sigrow_offset64;
	temp *= sigrow_gain; // Result 16bit+8bit = 24bit - temp is now Kelvin * 64 * 256
	temp *= 10; // Result now 28 bit = Kelvin * 64 * 256 * 10 = kelvin
	//need to add 0.05K for rounding to next 0.1K
	//need to subtract 0.05K to get to Celcius (0C = 273.15K)
	//total: add 0
	//divide by 64 * 256 (14 bits) to get Celcius*10 + 2731 value (don't shift bits, temp is signed)
	temp >>= 14;
	int16_t celsius10 = temp - 2731;

	return celsius10;
}

uint16_t adc_get_result() {
	//start conversion
	ADC0.CTRLA = ADC_ENABLE_bm; //RUNSTBY=0,RESSEL=0 10bit,FREERUN=0,ENABLE=1
	ADC0.COMMAND = ADC_STCONV_bm;
	while(ADC0_COMMAND & ADC_STCONV_bm);
	return ADC0.RES;
}

uint16_t adc_read(uint8_t channel) {
  ADC0.CTRLB = ADC_SAMPNUM_ACC1_gc;
  ADC0.CTRLC = ADC_REFSEL_VDDREF_gc | ADC_PRESC_gc; // select VDD as reference | F_CPU/x
  ADC0.CTRLD = ADC_INITDLY_DLY0_gc; // 0 cycles delay
  ADC0_MUXPOS = channel;
//  ADC0_MUXPOS = ADC_MUXPOS_AIN3_gc; //PA3
  ADC0.SAMPCTRL = 0;

  return adc_get_result();
}

uint16_t vcc_read() {
  VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc; //vref 1.1V
  VREF.CTRLB = VREF_ADC0REFEN_bm;
  //_delay_us(25); //settle vref -> does not help, not even _ms

  ADC0.CTRLB = ADC_SAMPNUM_ACC1_gc;
  ADC0.CTRLC = ADC_REFSEL_VDDREF_gc | ADC_PRESC_gc; // select VDD as reference | F_CPU/x
  ADC0.CTRLD = ADC_INITDLY_DLY16_gc; // Delay 16 CLK_ADC cycles <- needed with 0 cycles low volt values read too high
  ADC0_MUXPOS = ADC_MUXPOS_INTREF_gc; //select internal reference
  ADC0.SAMPCTRL = 0;

  //_delay_us(25); //settle vref -> does not help, not even _ms

  uint16_t v = adc_get_result();
  return v ? ((1<<10)-1L) * BANDGAP / v : 0; //bandgap voltage in mV
}

#endif /* ADC_H_ */
