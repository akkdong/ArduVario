// BatteryVoltage.h
//

#ifndef __BATTERYVOLTAGE_H__
#define __BATTERYVOLTAGE_H__

#include <VarioSettings.h>

///////////////////////////////////////////////////////////////////////////
//

#define SCALE_FACTOR		(1000.0 / (270.0 + 1000.0))	// voltage divisor : R1(270K), R2(1M)
#define	LPF_FACTOR			(0.2)

#define ADC_VREF			(5.0)
#define ADC_CH				((VOLTAGE_DIVISOR_PIN - 14) & 0x07)
#define ADC_TO_VOLTAGE(x)	((x) *  ADC_VREF / 1024.0 / SCALE_FACTOR)

#define ADC_MEASURE_COUNT	(10)



///////////////////////////////////////////////////////////////////////////
// class BatteryVoltage

class BatteryVoltage
{
public:
	BatteryVoltage();
	
public:
	void			init();
	
	double			getVoltage();
	void			setVoltage(double volt);
	
protected:
	volatile double	mVoltage;
};



///////////////////////////////////////////////////////////////////////////
//

extern BatteryVoltage battery;


#endif // __BATTERYVOLTAGE_H__
