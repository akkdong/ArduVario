// SoundPlayer.cpp
//

#include <toneAC.h>
#include "SoundPlayer.h"


////////////////////////////////////////////////////////////////////////////////////
//

static Tone muteTone[] = 
{ 
	{ 0, 0 }
};

static Tone beepTone[] = 
{
	{ 100, 100 },
	{   0, 100 }
};

static Tone beepToneNext[] = 
{
	{ 100, 100 },
	{   0, 100 }
};


////////////////////////////////////////////////////////////////////////////////////
//

SoundPlayer::SoundPlayer()
{
	playCurr.playType	= PLAY_NONE;
	playNext.playType	= PLAY_NONE;
	
	/*
	//
	soundPlayPtr	= 0;
	soundPlayCount 	= 0;
	soundPlayType	= PLAY_NONE;
	
	//
	soundNextPtr 	= 0;
	soundNextCount	= 0;
	soundNextType	= PLAY_NONE;
	*/
	
	//
	toneIndex 		= 0;
	toneStartTick 	= 0;
	//
	playCount 		= 0;
	
	//
	volume			= 10;
}

void SoundPlayer::setMelody(Tone * tonePtr, int toneCount, int repeat, int instant)
{
	playNext.tonePtr 		= tonePtr;
	playNext.toneCount		= toneCount;
	playNext.repeatCount	= repeat;
	playNext.playType		= PLAY_MELODY;
	
	if (instant)
		updateNow();
}

void SoundPlayer::setBeep(int freq, int period, int duty) 
{
	// update next beep tone
	beepToneNext[0].freq = freq;
	beepToneNext[0].length	= duty;

	beepToneNext[1].freq = 0;
	beepToneNext[1].length	= period - duty;
	
	//
	playNext.tonePtr 		= &beepTone[0];	// beepToneNext? ??? ?? updateNow ???? beepTone?? ????.
	playNext.toneCount		= sizeof(beepTone) / sizeof(beepTone[0]); // 2
	playNext.repeatCount	= 0; // infinite repeat
	playNext.playType		= PLAY_BEEP;
}

void SoundPlayer::setMute(int instant)
{
	playNext.tonePtr 		= &muteTone[0];
	playNext.toneCount		= sizeof(muteTone) / sizeof(muteTone[0]); // 1
	playNext.repeatCount	= 0; // infinite repeat
	playNext.playType		= PLAY_MUTE;
	
	if (instant)
		updateNow();
}

void SoundPlayer::setVolume(int value)
{
	if (0 < value && value <= 10)
		volume = value;
}

void SoundPlayer::update()
{
	if (updateCheck())
		updateNow();
}

int SoundPlayer::updateCheck()
{
	if (playCurr.playType == PLAY_NONE)
	{
		//if (playNext.playType != PLAY_NONE)
		//	return 1;
		//
		//return 0;
		
		return 1;
	}
	
	if (playCurr.tonePtr[toneIndex].length > 0)
	{
		unsigned long now = millis();

		if (toneStartTick + playCurr.tonePtr[toneIndex].length <= now)
		{
			toneIndex += 1;
			
			if (toneIndex < playCurr.toneCount)
			{
				toneStartTick = now;
				toneAC(playCurr.tonePtr[toneIndex].freq, volume);
			}
			else
			{
				// now all tone is played
				// move next or repeat again ?
				playCount += 1;
				
				if (playCurr.repeatCount == 0 || playCount < playCurr.repeatCount)
				{
					if (playCurr.repeatCount == 0 && playNext.playType != PLAY_NONE)
						return 1; // play next
					// else play again
					
					// ??? tone ?? ??...
					toneIndex = 0;
					toneStartTick = now;
					toneAC(playCurr.tonePtr[toneIndex].freq, volume);
				}
				else // repeatCount > 0 && playerCount == repeatCount
				{
					playCurr.playType = PLAY_NONE; // stop play
					toneAC(); // mute
					
					return 1; // play next
				}
			}
		}
		else
		{
			// ?? ? ???? ?? ?? ??? ??? ????? ??? ??.
			// !!! BUT !!! ???? ???? ??? ??? ??? ??? ?? ???? ?? ???? ?? ??? ??.
			if (toneStartTick + playCurr.tonePtr[toneIndex].length - GAP_BETWEEN_TONE <= now)
				toneAC(); // mute
		}
	}
	else
	{
		// mute, sinking beep ?? length? 0? ?? ?? ??? ??? ?? ??? ????.
		if (playNext.playType != PLAY_NONE)
			return 1;
	}
	
	return 0;
}

void SoundPlayer::updateNow()
{
	if (playNext.playType == PLAY_NONE)
		return;
	
	// copy next context to current 
	playCurr = playNext;
	// copy beepToneNext to beepTone if next play type is beep
	if (playCurr.playType == PLAY_BEEP)
	{
		beepTone[0] = beepToneNext[0];
		beepTone[1] = beepToneNext[1];
	}

	// reset next play
	playNext.playType = PLAY_NONE;
	
	//
	toneIndex = 0;
	toneStartTick = millis();
	playCount = 0;
	
	toneAC(playCurr.tonePtr[toneIndex].freq, volume);
}
