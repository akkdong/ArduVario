// BatteryVoltage.cpp
//

#include <Arduino.h>
#include "BatteryVoltage.h"


///////////////////////////////////////////////////////////////////////////
//

BatteryVoltage battery;



///////////////////////////////////////////////////////////////////////////
// class BatteryVoltage

BatteryVoltage::BatteryVoltage() : mVoltage(0.0)
{
}

void BatteryVoltage::init()
{
	// measure current voltage
	double voltage= 0;
	int count = 0;
	
	for (int count = 0; count < ADC_MEASURE_COUNT; count++)
		voltage += ADC_TO_VOLTAGE(analogRead(VOLTAGE_DIVISOR_PIN));
	
	setVoltage(voltage / ADC_MEASURE_COUNT);
	
	// setup ADC for battery voltage measurement
	ADMUX  = 0x40 | ADC_CH; // B01000xxx : default to AVCC VRef, ADC Right Adjust, and ADC channel (may be ADC2)
	ADCSRB = 0x00; 			// B00000000 : Analog Input bank 1
	ADCSRA = 0xCF; 			// B11001111 : ADC enable, ADC start, manual trigger mode, ADC interrupt enable, prescaler = 128
}
	
double BatteryVoltage::getVoltage()
{
	return mVoltage;
}

void BatteryVoltage::setVoltage(double volt)
{
	mVoltage = volt;
}


ISR(ADC_vect)
{
	char adc_ch = ADMUX & 0x07;  // extract the channel of the ADC result
	
	if (adc_ch == ADC_CH)
	{
		// read ADC
		uint8_t low, high;
		double volt;
		
		low = ADCL;
		high = ADCH;		
		
		// calculate voltage
		volt = ADC_TO_VOLTAGE((high << 8) | low);
		volt = battery.getVoltage() * (1 - LPF_FACTOR) + volt * LPF_FACTOR;
		
		battery.setVoltage(volt);
		
		// manually trigger the next ADC
		ADCSRA |= 0xC0; // B11000000
	}
}
