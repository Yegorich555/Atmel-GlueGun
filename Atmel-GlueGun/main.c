/*
* Atmel-GlueGun.c
*
* Created: 08.11.2018 14:49:50
* Author : Yegorich555
*/
#define F_CPU 9600000UL
#include <avr/io.h>
#include <extensions.h>
#include <util/delay.h>

#define IO_MoveSens B, 0
#define IO_Heat B, 3
#define IO_ADC_T 2 //portb.4 adc2

#define ADC_T_dt 5 //5 °C
#define ADC_T_SET 180 //180 °C
#define ADC_T_OFF (ADC_T_SET + ADC_T_dt)
#define ADC_T_ON (ADC_T_SET - ADC_T_dt)

#define REF_AVCC (0<<REFS0) // reference = AVCC
#define REF_INT  (1<<REFS0) // internal reference 1.1 V

#define ADC_VREF_TYPE REF_AVCC//((1<<REFS0) | (0<<ADLAR))

#if DEBUG
#define USOFT_BAUD 4800
#define USOFT_IO_TX B, 0
#define USOFT_RXEN 0
#include <uart_soft.h>
#endif

void adc_init(void)
{
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); // prescaler 64 => 125 kHz
	DIDR0 |= (0<<ADC0D)|(0<<ADC1D)|(1<<ADC2D)|(0<<ADC3D); //disable digital input for selected ADC
}

// Read the AD conversion result
unsigned int read_adc(unsigned char adc_input)
{
	ADMUX=adc_input | ADC_VREF_TYPE;
	_delay_us(10); // Delay needed for the stabilization of the ADC input voltage
	ADCSRA|=(1<<ADSC); // Start the AD conversion
	while ((ADCSRA & (1<<ADIF))==0); // Wait for the AD conversion to complete
	ADCSRA|=(1<<ADIF);
	return ADCW;
}

void _puts(char *str)
{
	#if DEBUG
	usoft_putString((unsigned char *)str);
	usoft_putCharf(0x0D);
	#endif
}

const int adcCount = 10;
int main(void)
{
	int tMeasure;
	io_set(DDR, IO_Heat);
	
	adc_init();
	#if DEBUG
	usoft_init();
	#endif

	while (1)
	{
		io_resetPort(IO_Heat);
		
		tMeasure = 0;
		for (uint8_t i = 0; i < adcCount; ++i)
		{
			_delay_us(100);
			tMeasure += read_adc(IO_ADC_T);
		}
		tMeasure = tMeasure/adcCount;
		
		if (tMeasure >= ADC_T_OFF)
		{
			io_resetPort(IO_Heat);
			_puts("Off");
		}
		else if (tMeasure <= ADC_T_ON) {
			io_setPort(IO_Heat);
			_puts("On");
		}
		
		#if DEBUG
		usoft_putStringf("adc:");

		int at = tMeasure /1000;
		usoft_putChar(48 + at);
		tMeasure = tMeasure - at * 1000;
		
		at = tMeasure /100;
		usoft_putChar(48 + at);
		tMeasure = tMeasure - at * 100;
		
		at = tMeasure /10;
		usoft_putChar(48 + at);
		tMeasure = tMeasure - at * 10;

		usoft_putChar(48 + tMeasure);
		usoft_putChar(0x0D);
		#endif
	}
}