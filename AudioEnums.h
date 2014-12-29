#pragma once

namespace CocosDenshion
{
	public enum class SoundEvent
	{
		cCrowdNoise,
		cSoundEffect1,
		cSoundEffect2,

		// add sound effects
		

		LastSoundEvent
	};

	const static int SOUND_EVENTS = (int) SoundEvent::LastSoundEvent;

	public ref class SoundEventEnum sealed
	{
	public:
		property SoundEvent SoundEventId
		{
			SoundEvent  get()
			{
				return m_soundEvent;
			}
		}

	private:
		SoundEvent m_soundEvent;
	};

}

//namespace AudioConstants
//{
//	static const WCHAR * EFFECT_FILES[] =
//	{
//		L"Assets\\601.wav",
//		L"Assets\\601.wav",
//		L"Assets\\601.wav",
//	};
//}