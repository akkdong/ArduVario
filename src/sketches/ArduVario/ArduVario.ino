#include <Arduino.h>
#include <SPI.h>
#include <VarioSettings.h>
#include <I2Cdev.h>
#include <ms5611.h>
#include <vertaccel.h>
#include <EEPROM.h>
#include <inv_mpu.h>
#include <kalmanvert.h>
#include <toneAC.h>
#include <avr/pgmspace.h>
#include <digit.h>
#include <SerialNmea.h>
#include <NmeaParser.h>
#include <LxnavSentence.h>
#include <LK8Sentence.h>
#include <IGCSentence.h>
#include <FirmwareUpdater.h>
#include <BatteryVoltage.h>
#include <VarioBeeper.h>
#include <SoftwareSerial.h>
#include <NmeaParserEx.h>


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!            !!! WARNING  !!!              !!*/
/*!! Before building check :                  !!*/
/*!! libraries/VarioSettings/VarioSettings.h  !!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/


/*******************/
/* General objects */
/*******************/
#define VARIOMETER_STATE_INITIAL 			0
#define VARIOMETER_STATE_DATE_RECORDED 		1
#define VARIOMETER_STATE_CALIBRATED 		2
#define VARIOMETER_STATE_FLIGHT_STARTED 	3

#ifdef HAVE_GPS
uint8_t 		variometerState = VARIOMETER_STATE_INITIAL;
#else
uint8_t 		variometerState = VARIOMETER_STATE_CALIBRATED;
#endif //HAVE_GPS

/**********************/
/* alti/vario objects */
/**********************/
#define POSITION_MEASURE_STANDARD_DEVIATION 		0.1
#define ACCELERATION_MEASURE_STANDARD_DEVIATION 	0.3

kalmanvert 		kalmanvert;

/**********************/
/* sound/beeper objects */
/**********************/
SoundPlayer		player;
VarioBeeper 	beeper(player);

/***************/
/* gps objects */
/***************/

SoftwareSerial SerialGPS(8, 5);
NmeaParserEx nmeaParserEx(SerialGPS);


#ifdef HAVE_GPS

NmeaParser 		nmeaParser;
boolean 		lastSentence = false;

unsigned long 	RMCSentenceTimestamp; //for the speed filter
double 			RMCSentenceCurrentAlti; //for the speed filter
//unsigned long 	speedFilterTimestamps[VARIOMETER_SPEED_FILTER_SIZE];
//double 			speedFilterSpeedValues[VARIOMETER_SPEED_FILTER_SIZE];
//double 			speedFilterAltiValues[VARIOMETER_SPEED_FILTER_SIZE];
//int8_t 			speedFilterPos = 0;

#endif //HAVE_GPS

/*********************/
/* bluetooth objects */
/*********************/
#if defined(VARIOMETER_SENT_LXNAV_SENTENCE)
LxnavSentence 	bluetoothNMEA;
#elif defined(VARIOMETER_SENT_LK8000_SENTENCE)
LK8Sentence 	bluetoothNMEA;
#else
#error No bluetooth sentence type specified !
#endif

#ifndef HAVE_GPS
unsigned long 	lastVarioSentenceTimestamp = 0;
#endif // !HAVE_GPS

static Tone startTone[] = {
	{ 262, 1000 / 4 }, 
	{ 196, 1000 / 8 }, 
	{ 196, 1000 / 8 }, 
	{ 220, 1000 / 4 }, 
	{ 196, 1000 / 4 }, 
	{   0, 1000 / 4 }, 
	{ 247, 1000 / 4 }, 
	{ 262, 1000 / 4 },
	{   0, 1000 / 8 }, 
};


/*-----------------*/
/*      SETUP      */
/*-----------------*/
void setup()
{
	/***************/
	/* init battery voltage */
	/***************/
	battery.init();
	
	//
	delay(1000);

	/************************************/
	/* init altimeter and accelerometer */
	/************************************/
	Fastwire::setup(FASTWIRE_SPEED, 0);
	ms5611_init();
	vertaccel_init();
	if( firmwareUpdateCond() ) 
	{
		firmwareUpdate();
	}

	/**************************/
	/* init gps and bluetooth */
	/**************************/
	//SerialGPS.begin(9600);
	
#ifdef HAVE_GPS
	serialNmea.begin(GPS_BLUETOOTH_BAUDS, true);
#else
	serialNmea.begin(GPS_BLUETOOTH_BAUDS, false);
#endif //HAVE_GPS

	/******************/
	/* get first data */
	/******************/

	/* wait for first alti and acceleration */
	while( ! (ms5611_dataReady() && vertaccel_dataReady()) ) 
	{
		// nop
	}

	/* get first data */
	ms5611_updateData();
	vertaccel_updateData();

	/* init kalman filter */
	kalmanvert.init(ms5611_getAltitude(),
				vertaccel_getValue(),
				POSITION_MEASURE_STANDARD_DEVIATION,
				ACCELERATION_MEASURE_STANDARD_DEVIATION,
				millis());
				
	//
	player.setVolume(VARIOMETER_BEEP_VOLUME);
	player.setMelody(&startTone[0], sizeof(startTone) / sizeof(startTone[0]), 1, 0);
}

void enableflightStartComponents(void);

/*----------------*/
/*      LOOP      */
/*----------------*/
void loop()
{
	/*****************************/
	/* compute vertical velocity */
	/*****************************/
	if( ms5611_dataReady() && vertaccel_dataReady() )
	{
		ms5611_updateData();
		vertaccel_updateData();

		kalmanvert.update( ms5611_getAltitude(),
						vertaccel_getValue(),
						millis() );


		/* set beeper */
		beeper.setVelocity( kalmanvert.getVelocity() );
	}

	/*****************/
	/* update beeper */
	/*****************/
	//beeper.update();
	player.update();

	/*****************/
	/* update nmea parser */
	/*****************/
	//nmeaParserEx.update();
	//
	//if (nmeaParserEx.available())
	//{
	//	while (nmeaParserEx.available())
	//		serialNmea.write(nmeaParserEx.read());
	//}
	
	/********************/
	/* update bluetooth */
	/********************/
#ifdef HAVE_GPS
	/* in priority send vario nmea sentence */
	if( bluetoothNMEA.available() ) 
	{
		while( bluetoothNMEA.available() ) 
		{
			serialNmea.write( bluetoothNMEA.get() );
		}
		serialNmea.release();
	}	
#else //!HAVE_GPS
	/* check the last vario nmea sentence */	
	if( millis() - lastVarioSentenceTimestamp > VARIOMETER_SENTENCE_DELAY ) 
	{
		lastVarioSentenceTimestamp = millis();
#ifdef VARIOMETER_BLUETOOTH_SEND_CALIBRATED_ALTITUDE
		bluetoothNMEA.begin(kalmanvert.getCalibratedPosition(), kalmanvert.getVelocity());
#else
		bluetoothNMEA.begin(kalmanvert.getPosition(), kalmanvert.getVelocity());
#endif
#ifdef USE_MYWAY
		bluetoothNMEA.setExtra(ms5611_getTemperature(), battery.getVoltage());
#endif // USE_MYWAY
		while( bluetoothNMEA.available() ) 
		{
			serialNmea.write( bluetoothNMEA.get() );
		}
	}
#endif //!HAVE_GPS


#ifdef HAVE_GPS
	/**************/
	/* update GPS */
	/**************/
	/* else try to parse GPS nmea */
	else
	{
		/* try to lock sentences */
		if( serialNmea.lockRMC() ) 
		{
			RMCSentenceTimestamp = millis();
			RMCSentenceCurrentAlti = kalmanvert.getPosition(); //useless to take calibrated here
			nmeaParser.beginRMC();
		}
		else if( serialNmea.lockGGA() ) 
		{
			nmeaParser.beginGGA();
			lastSentence = true;
		}

		/* parse if needed */
		if( nmeaParser.isParsing() )
		{
			while( nmeaParser.isParsing() ) 
			{
				uint8_t c = serialNmea.read();

				/* parse sentence */        
				nmeaParser.feed( c );
			}
			serialNmea.release();

			/* if this is the last GPS sentence */
			/* we can send our sentences */
			if( lastSentence ) 
			{
				lastSentence = false;
#ifdef VARIOMETER_BLUETOOTH_SEND_CALIBRATED_ALTITUDE
				bluetoothNMEA.begin(kalmanvert.getCalibratedPosition(), kalmanvert.getVelocity());
#else
				bluetoothNMEA.begin(kalmanvert.getPosition(), kalmanvert.getVelocity());
#endif
#ifdef USE_MYWAY
				bluetoothNMEA.setExtra(ms5611_getTemperature(), battery.getVoltage());
#endif // USE_MYWAY
				serialNmea.lock(); //will be writed at next loop
			}
		}

		/***************************/
		/* update variometer state */
		/*    (after parsing)      */
		/***************************/
		if( variometerState < VARIOMETER_STATE_FLIGHT_STARTED )
		{
			/* if initial state check if date is recorded  */
			if( variometerState == VARIOMETER_STATE_INITIAL )
			{
				if( nmeaParser.haveDate() )
				{
					variometerState = VARIOMETER_STATE_DATE_RECORDED;
				}
			}
			/* check if we need to calibrate the altimeter */
			else if( variometerState == VARIOMETER_STATE_DATE_RECORDED )
			{
				/* we need a good quality value */
				if( nmeaParser.haveNewAltiValue() && nmeaParser.precision < VARIOMETER_GPS_ALTI_CALIBRATION_PRECISION_THRESHOLD )
				{
					/* calibrate */
					double gpsAlti = nmeaParser.getAlti();
					kalmanvert.calibratePosition(gpsAlti);
					variometerState = VARIOMETER_STATE_CALIBRATED;
				}
			}
			/* else check if the flight have started */
			else // variometerState == VARIOMETER_STATE_CALIBRATED
			{
				/* check flight start condition */
				if( (millis() > FLIGHT_START_MIN_TIMESTAMP) &&
					(kalmanvert.getVelocity() < FLIGHT_START_VARIO_LOW_THRESHOLD || kalmanvert.getVelocity() > FLIGHT_START_VARIO_HIGH_THRESHOLD) &&
					(nmeaParser.getSpeed() > FLIGHT_START_MIN_SPEED) )
				{
					variometerState = VARIOMETER_STATE_FLIGHT_STARTED;
					enableflightStartComponents();
				}
			}
		}
	}
#endif //HAVE_GPS

	/* if no GPS, we can't calibrate, and we have juste to check flight start */
#ifndef HAVE_GPS
	if( variometerState == VARIOMETER_STATE_CALIBRATED ) //already calibrated at start 
	{ 
		if( (millis() > FLIGHT_START_MIN_TIMESTAMP) &&
			(kalmanvert.getVelocity() < FLIGHT_START_VARIO_LOW_THRESHOLD || kalmanvert.getVelocity() > FLIGHT_START_VARIO_HIGH_THRESHOLD) )
		{
			variometerState = VARIOMETER_STATE_FLIGHT_STARTED;
			enableflightStartComponents();
		}
	}
#endif // !HAVE_GPS
}

void enableflightStartComponents(void)
{
	/* enable near climbing */
#ifdef HAVE_SPEAKER
#ifdef VARIOMETER_ENABLE_NEAR_CLIMBING_ALARM
	//beeper.setGlidingAlarmState(true);
#endif
#ifdef VARIOMETER_ENABLE_NEAR_CLIMBING_BEEP
	//beeper.setGlidingBeepState(true);
#endif
#endif //HAVE_SPEAKER
}
