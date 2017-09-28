// SoftwareSerialTest.ino
//

#include <SoftwareSerial.h>
#include <NmeaParserEx.h>


SoftwareSerial SerialGPS(8, 9);
NmeaParserEx nmeaParser(SerialGPS);

void setup()
{
	Serial.begin(115200);
	while (! Serial);
	
	SerialGPS.begin(9600);
}

void loop()
{
	nmeaParser.update();
	
	if (nmeaParser.available())
		Serial.write(nmeaParser.read());
}