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

//returns temperature in 1/10 C
uint16_t temp_read() {
	#define OVERSAMPLE_BITS 4

	//uart_printf("temp_read:");
	uint16_t adc_reading = temp_read_raw(); //dummy read - improves accuracy
	adc_reading = 0;
	for(uint8_t i=0;i<(1<<OVERSAMPLE_BITS);i++) {
		uint16_t v = temp_read_raw();
		//uart_printf("%d ",v);
		adc_reading += v;
	}

	int16_t sigrow_offset = ((int16_t)SIGROW.TEMPSENSE1) * (1<<OVERSAMPLE_BITS);
	uint8_t sigrow_gain = SIGROW.TEMPSENSE0;

	uint32_t temp = adc_reading - sigrow_offset;
	//temp *= sigrow_gain; // Result might overflow 16 bit variable (10bit+8bit)
	//temp += 0x80; // Add 1/2 to get correct rounding on division below
	//temp >>= 8; // Divide result to get Kelvin
	//uint16_t temperature_in_C = temp - 273;

	temp *= sigrow_gain; // Result 14bit + 8bit = 22bit - temp in 1/(256 * OVERSAMPLE_NUM) kelvin
	temp *= 10;   // temp in  1/(2560 * OVERSAMPLE_NUM) kelvin
	//temp += (1<<(7+OVERSAMPLE_BITS)); // Add 1/2 to get correct rounding on division below
	temp >>= (8+OVERSAMPLE_BITS);  // temp in 1/10 kelvin
	temp -= 2731; // temp in 1/10 celsius

	//uart_printf(" raw=%d offset=%d gain=%d c=%d ",adc_reading, (int)sigrow_offset, (int)sigrow_gain, (int)temp);
	return (uint16_t)temp;
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
