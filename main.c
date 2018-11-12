/*
* Atmel-GlueGun.c
*
* Created: 08.11.2018 14:49:50
* Author : Yegorich555
*/
#define F_CPU 9600000UL
#include <avr/io.h>
#include <extensions.h> //add ref to Atmel-Library and reload Studio
#include <util/delay.h>

#define IO_MoveSens B, 0
#define IO_Heat B, 4
#define IO_ADC_T 3 //portb.3 adc3

// 1/T = 1/T0 + 1/B*ln(R/R0) - Steinhart-Hart equation
#define T_B 3950 // B-coef
#define T_SERIAL_R 10000 //kOm, 10
#define T_NOMINAL_R 100000 //kOm, 100
#define T_NOMINAL 25 //�C, T with RT = 100kOm

#define ADC_T_dt 1 //�C
#define ADC_T_SET 160 //�C
#define T_CALC(v) 1023 - (int)(1023.0/T_SERIAL_R*pow(exp(1), T_B*(1.0/(v+273.15) - 1.0/(T_NOMINAL+273.15)) + log(T_NOMINAL_R)))
#define ADC_TD_OFF T_CALC(ADC_T_SET + ADC_T_dt) //calculated into descrets
#define ADC_TD_ON  T_CALC(ADC_T_SET - ADC_T_dt) //calculated into descrets

int calcT(int v) //not used because it's a huge logic for Attiny13
{
	float r = 1023.0 / v - 1;
	r = T_SERIAL_R / r; //resistance
	
	float steinhart;
	steinhart = r / T_NOMINAL_R; // (R/Ro)
	steinhart = log(steinhart); // ln(R/Ro)
	steinhart /= T_B; // 1/B * ln(R/Ro)
	steinhart += 1.0 / (T_NOMINAL + 273.15); // + (1/To)
	steinhart = 1.0 / steinhart; // Invert
	steinhart -= 273.15;
	return (int)steinhart;
}

#define MOVE_SENS_MINUTES 15 //minutes
#define CYCLE_TIMEOUT_MS 100
#define MOVE_SENS_TIMEOUT (MOVE_SENS_MINUTES*60*(1000/CYCLE_TIMEOUT_MS))

#define HEATER_MAX_SECONDS 60 //max time which heater can be supplied
#define HEATER_MAX_TIME (HEATER_MAX_SECONDS*(1000/CYCLE_TIMEOUT_MS))

#define REF_AVCC (0<<REFS0) // reference = AVCC
#define REF_INT  (1<<REFS0) // internal reference 1.1 V

#define ADC_VREF_TYPE REF_AVCC//((1<<REFS0) | (0<<ADLAR))

#if DEBUG
#define USOFT_BAUD 4800
#define USOFT_IO_TX B, 1
#define USOFT_RXEN 0
#include <uart_soft.h>
#endif

void adc_init(void)
{
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); // prescaler 64 => 125 kHz
	DIDR0 |= (0<<ADC0D)|(0<<ADC1D)|(0<<ADC2D)|(1<<ADC3D); //disable digital input for selected ADC
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
	int moveSensCount = 0;
	int heaterMaxCount = 0;
	io_set(DDR, IO_Heat);
	
	adc_init();
	#if DEBUG
	usoft_init();
	#endif

	while (1)
	{
		//moveSensor behavior
		if (io_getPin(IO_MoveSens))
		{
			delay_ms(10);
			if (io_getPin(IO_MoveSens))
			{
				moveSensCount = 0;
			}
			
		}
		if (moveSensCount >= MOVE_SENS_TIMEOUT)
		{
			io_resetPort(IO_Heat);
			_puts("OffMove");
			heaterMaxCount = 0;
			continue;
		}
		
		//heater behavior
		tMeasure = 0;
		for (uint8_t i = 0; i < adcCount; ++i)
		{
			_delay_us(100);
			tMeasure += read_adc(IO_ADC_T);
		}
		//tMeasure = calcT(tMeasure/adcCount);
		tMeasure = tMeasure/adcCount;
		if (tMeasure < 20 || tMeasure > 1010){
			heaterMaxCount = 0;
			io_resetPort(IO_Heat);
			_puts("T broken");
			continue;
		}
		if (tMeasure >= ADC_TD_OFF)
		{
			heaterMaxCount = 0;
			io_resetPort(IO_Heat);
			_puts("Off");
		}
		else if (tMeasure <= ADC_TD_ON) {
			
			if (heaterMaxCount >= HEATER_MAX_TIME)
			{
				_puts("MaxTime");
			}
			else
			{
				++heaterMaxCount;
				io_setPort(IO_Heat);
				_puts("On");
			}
		}
		++moveSensCount;
		
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
		_delay_ms(80); //cycle about 100ms
		#else
		_delay_ms(89); //cycle about 100ms
		#endif
	}
}