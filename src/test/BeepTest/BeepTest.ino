// BeepTest.ino
//

#include "SoundPlayer.h"
#include "VarioBeeper.h"

static Tone playTone[] = {
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

SoundPlayer player;
VarioBeeper	beeper(player);

char bufPtr[16];
int bufLen;

void setup()
{
	//
	Serial.begin(115200);
	while(! Serial);
	
	//
	player.setVolume(3);
	//player.setMelody(&playTone[0], sizeof(playTone) / sizeof(playTone[0]), 2);
	//player.setBeep(262, 200, 10);
	
	//
	bufLen = 0;
	bufPtr[bufLen] = 0;
}

void loop() 
{
	if (Serial.available()) 
	{
		int c = Serial.read();
		bufPtr[bufLen++] = c;
		
		if (c == 0x0D || c == 0x0A)
		{
			double num = atof(bufPtr);
			
			Serial.print("Velocity : "); Serial.println(num);
			beeper.setVelocity(num);
			
			bufLen = 0;
			bufPtr[bufLen] = 0;
		}
		
		/*
		switch (c)
		{
		case '0' :
			player.setMute();
			break;
		case '9' :
			player.setMute(0);
			break;			
		case '1' :
			player.setBeep(1000, 100, 30);
			break;
		case '2' :
			player.setBeep(600, 200, 150);
			break;
		case '3' :
			player.setBeep(200, 0, 0);
			break;
			
		case 'm' :
		case 'M' :
			player.setMelody(&playTone[0], sizeof(playTone) / sizeof(playTone[0]), 1, 0);
			break;
			
		case 'i' :
		case 'I' :
			player.setMelody(&playTone[0], sizeof(playTone) / sizeof(playTone[0]), 1, 1);
			break;
		}
		*/
	}
	
	//
	player.update();
}
