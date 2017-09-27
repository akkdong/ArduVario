// VarioBeeper.cpp
//


#include "VarioBeeper.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

struct VarioTone
{
	double 	velocity;
	int 	freq;
	int 	period;
	int 	duty;
};

static VarioTone varioTone[] = 
{
	{ -10.00,  200, 100, 100  },
	{  -3.00,  280, 100, 100  },
	{  -0.51,  300, 500, 100  },
	{  -0.50,  200, 800,   5  },
	{   0.09,  400, 600,  10  },
	{   0.10,  400, 600,  50  },
	{   1.16,  550, 552,  52  },
	{   2.67,  763, 483,  55  },
	{   4.24,  985, 412,  58  },
	{   6.00, 1234, 322,  62  },
	{   8.00, 1517, 241,  66 }, 
	{  10.00, 1800, 150,  70 },
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

VarioBeeper::VarioBeeper(SoundPlayer & sp, double ct, double st) :	player(sp)
{
	setThreshold(ct, st);
}

void VarioBeeper::setThreshold(double ct, double st)
{
	climbingThreshold = ct;
	sinkingThreshold = st;
}

void VarioBeeper::setVelocity(double velocity)
{
	boolean typeChanged = false;
	
	switch (beepType)
	{
	case BEEP_TYPE_SINKING :
		if (sinkingThreshold + BEEP_VELOCITY_SISITIVITY < velocity)
			typeChanged = true;
		break;
		
	case BEEP_TYPE_SILENT :
		if (velocity <= sinkingThreshold || climbingThreshold <= velocity)
			typeChanged = true;
		break;
		
	case BEEP_TYPE_CLIMBING :
		if (velocity < climbingThreshold - BEEP_VELOCITY_SISITIVITY)
			typeChanged = true;
		break;
	}
	
	if (typeChanged)
	{
		if (velocity <= sinkingThreshold)
			beepType = BEEP_TYPE_SINKING;
		else if (climbingThreshold <= velocity)
			beepType = BEEP_TYPE_CLIMBING;
		else
			beepType = BEEP_TYPE_SILENT;
	}
	
	if (beepType != BEEP_TYPE_SILENT)
	{
		int freq, period, duty;
		
		findTone(velocity, freq, period, duty);		
		player.setBeep(freq, period, duty);
	}
	else
	{
		player.setMute(0);
	}
}

void VarioBeeper::findTone(double velocity, int & freq, int & period, int & duty)
{
	int index;
	
	for (index = 0; index < (sizeof(varioTone) / sizeof(varioTone[0])); index++)
	{
		if (velocity <= varioTone[index].velocity)
			break;
	}
	
	if (index == 0 || index == (sizeof(varioTone) / sizeof(varioTone[0])))
	{
		if (index != 0)
			index -= 1;
		
		freq = varioTone[index].freq;
		period = varioTone[index].period;
		duty = varioTone[index].duty;
	}
	else
	{
		double ratio = varioTone[index].velocity / velocity;
		
		freq = (varioTone[index].freq - varioTone[index-1].freq) / (varioTone[index].velocity - varioTone[index-1].velocity) * (velocity - varioTone[index-1].velocity) + varioTone[index-1].freq;
		period = (varioTone[index].period - varioTone[index-1].period) / (varioTone[index].velocity - varioTone[index-1].velocity) * (velocity - varioTone[index-1].velocity) + varioTone[index-1].period;
		duty = (varioTone[index].duty - varioTone[index-1].duty) / (varioTone[index].velocity - varioTone[index-1].velocity) * (velocity - varioTone[index-1].velocity) + varioTone[index-1].duty;
	}
		
	period = (int)(period * 0.6);
	duty = (int)(period * duty / 100.0);
	
	if (period == duty)
		period = duty = 0; // infinite beepping
}
