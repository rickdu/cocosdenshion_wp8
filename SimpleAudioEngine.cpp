
#include "SimpleAudioEngine.h"
#include "Audio.h"

#include <map>
using namespace std;

namespace CocosDenshion {

Audio* s_audioController = NULL;
bool s_initialized = false;

SimpleAudioEngine* SimpleAudioEngine::getInstance()
{
    static SimpleAudioEngine s_SharedEngine;
    return &s_SharedEngine;
}


static Audio* sharedAudioController()
{
    if (! s_audioController || !s_initialized)
    {
        if(s_audioController == NULL)
        {
            s_audioController = new Audio;
        }
        s_audioController->Initialize();
        s_audioController->CreateResources();
        s_initialized = true;
    }

    return s_audioController;
}

SimpleAudioEngine::SimpleAudioEngine()
{
}

SimpleAudioEngine::~SimpleAudioEngine()
{
}


void SimpleAudioEngine::end()
{
    sharedAudioController()->StopBackgroundMusic(true);
    sharedAudioController()->StopAllSoundEffects();
    sharedAudioController()->ReleaseResources();
    s_initialized = false;
}



//////////////////////////////////////////////////////////////////////////
// BackgroundMusic
//////////////////////////////////////////////////////////////////////////

void SimpleAudioEngine::playBackgroundMusic(const char* pszFilePath, bool bLoop)
{
    if (! pszFilePath)
    {
        return;
    }
    sharedAudioController()->PlayBackgroundMusic(pszFilePath, bLoop);
}

void SimpleAudioEngine::stopBackgroundMusic(bool bReleaseData)
{
    sharedAudioController()->StopBackgroundMusic(bReleaseData);
}

void SimpleAudioEngine::pauseBackgroundMusic()
{
    sharedAudioController()->PauseBackgroundMusic();
}

void SimpleAudioEngine::resumeBackgroundMusic()
{
    sharedAudioController()->ResumeBackgroundMusic();
}

void SimpleAudioEngine::rewindBackgroundMusic()
{
    sharedAudioController()->RewindBackgroundMusic();
}

bool SimpleAudioEngine::willPlayBackgroundMusic()
{
    return false;
}

bool SimpleAudioEngine::isBackgroundMusicPlaying()
{
    return sharedAudioController()->IsBackgroundMusicPlaying();
}

//////////////////////////////////////////////////////////////////////////
// effect function
//////////////////////////////////////////////////////////////////////////

unsigned int SimpleAudioEngine::playEffect(const char* pszFilePath, bool bLoop,float pitch, float pan, float gain)
{
    unsigned int sound;
    sharedAudioController()->PlaySoundEffect(pszFilePath, bLoop, sound);
    // TODO: need to support playEffect parameters
    return sound;
}

void SimpleAudioEngine::stopEffect(unsigned int nSoundId)
{
    sharedAudioController()->StopSoundEffect(nSoundId);
}

void SimpleAudioEngine::preloadEffect(const char* pszFilePath)
{
    sharedAudioController()->PreloadSoundEffect(pszFilePath);
}

void SimpleAudioEngine::pauseEffect(unsigned int nSoundId)
{
    sharedAudioController()->PauseSoundEffect(nSoundId);
}

void SimpleAudioEngine::resumeEffect(unsigned int nSoundId)
{
    sharedAudioController()->ResumeSoundEffect(nSoundId);
}

void SimpleAudioEngine::pauseAllEffects()
{
    sharedAudioController()->PauseAllSoundEffects();
}

void SimpleAudioEngine::resumeAllEffects()
{
    sharedAudioController()->ResumeAllSoundEffects();
}

void SimpleAudioEngine::stopAllEffects()
{
    sharedAudioController()->StopAllSoundEffects();
}

void SimpleAudioEngine::preloadBackgroundMusic(const char* pszFilePath)
{
    UNUSED_PARAM(pszFilePath);
}

void SimpleAudioEngine::unloadEffect(const char* pszFilePath)
{
    sharedAudioController()->UnloadSoundEffect(pszFilePath);
}

//////////////////////////////////////////////////////////////////////////
// volume interface
//////////////////////////////////////////////////////////////////////////

float SimpleAudioEngine::getBackgroundMusicVolume()
{
    return sharedAudioController()->GetBackgroundVolume();
}

void SimpleAudioEngine::setBackgroundMusicVolume(float volume)
{
	sharedAudioController()->SetBackgroundVolume((volume<=0.0f)? 0.0f : volume);
}

float SimpleAudioEngine::getEffectsVolume()
{
    return sharedAudioController()->GetSoundEffectVolume();
}

void SimpleAudioEngine::setEffectsVolume(float volume)
{
    sharedAudioController()->SetSoundEffectVolume((volume<=0.0f)? 0.0f : volume);
}

} // end of namespace CocosDenshion
