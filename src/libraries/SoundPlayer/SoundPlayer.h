// SoundPlayer.h
//

#ifndef __SOUNDPLAYER_H__
#define __SOUNDPLAYER_H__

////////////////////////////////////////////////////////////////////////////////////
//

#define PLAY_NONE			0
#define PLAY_MUTE			1
#define PLAY_BEEP			2
#define PLAY_MELODY			3

#define SOUND_MUTE			1
#define SOUND_BEEP			2
#define SOUND_MELODY		3


#define GAP_BETWEEN_TONE	1		// 1ms


////////////////////////////////////////////////////////////////////////////////////
//

struct Tone 
{
	int			freq;
	int			length;
};

struct PlayContext
{
	//
	Tone *			tonePtr;
	int				toneCount;
	
	//
	int				playType;
	
	//
	int				repeatCount;
};


////////////////////////////////////////////////////////////////////////////////////
//

class SoundPlayer 
{
public:
	SoundPlayer();
	
public:	
	void			setMelody(Tone * tonePtr, int toneCount, int repeat = 1, int instant = 1);
	void			setBeep(int freq, int period, int duty); // duty --> ms, <= period
	void			setMute(int instant = 1);
	
	void			setVolume(int value);
	
	void			update();

private:
	int				updateCheck();
	void			updateNow();

public:
	//
	PlayContext		playCurr;
	PlayContext		playNext;
	
	/*
	//
	Sound *			soundPlayPtr;
	int				soundPlayCount;
	int				soundPlayType;

	//
	Sound *			soundNextPtr;
	int				soundNextCount;
	int				soundNextType;
	*/

	//
	int				toneIndex;
	unsigned long	toneStartTick;
	//
	int				playCount;
	
	//
	int				volume;
};

#endif // __SOUNDPLAYER_H__
