

#include "Audio.h"
#include "MediaStreamer.h"

#include <implements.h>
#include <wrl/client.h>
#include <d3d11_1.h>
#include <DirectXMath.h>
#include <memory>
#include <agile.h>

#include <Mfapi.h>

using namespace Microsoft::WRL;
using namespace Windows::ApplicationModel;

class MediaEngineNotify : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFMediaEngineNotify>
{
public:
	STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2)
	{
		//printf("%s\r\n", meEvent);
		return S_OK;
	}
};


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        // Set a breakpoint on this line to catch DX API errors.
        throw Platform::Exception::CreateException(hr);
    }
}

Platform::String^ ws2ps(std::wstring s)
{
	return ref new Platform::String(s.c_str());
}

std::wstring s2ws(std::string s)
{
	std::wstring ws;
	ws.assign(s.begin(), s.end());
	return ws;
}

Platform::String^ s2ps(std::string s)
{
	return ref new Platform::String(s2ws(s).c_str());
}

Platform::String^ a2ps(const char *text)
{
	return s2ps(std::string(text));
}

std::wstring a2ws(const char* text)
{
	return s2ws(std::string(text));
}

void AudioEngineCallbacks::Initialize(Audio *audio)
{
    m_audio = audio;
}

// Called in the event of a critical system error which requires XAudio2
// to be closed down and restarted.  The error code is given in error.
void  _stdcall AudioEngineCallbacks::OnCriticalError(HRESULT Error)
{
    UNUSED_PARAM(Error);
    m_audio->SetEngineExperiencedCriticalError();
};

Audio::Audio() :
    m_backgroundID(0),
	m_soundEffctVolume(1.0f),
	m_backgroundMusicVolume(1.0f)
{
	ThrowIfFailed(
		MFStartup(MF_VERSION)
	);
}

Audio::~Audio()
{
	m_soundEffectEngine->StopEngine();
	PauseBackgroundMusic();
	MFShutdown();
}

void Audio::Initialize()
{
    m_engineExperiencedCriticalError = false;

	m_musicEngine = nullptr;
	m_soundEffectEngine = nullptr;
	m_musicMasteringVoice = nullptr;
	m_soundEffectMasteringVoice = nullptr;
}

void Audio::CreateResources()
{
	// create effect resources
    try
    {	
        ThrowIfFailed(
            XAudio2Create(&m_musicEngine)
            );

#if defined(_DEBUG)
        XAUDIO2_DEBUG_CONFIGURATION debugConfig = {0};
        debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
        debugConfig.TraceMask = XAUDIO2_LOG_ERRORS;
        m_musicEngine->SetDebugConfiguration(&debugConfig);
#endif

        m_musicEngineCallback.Initialize(this);
        m_musicEngine->RegisterForCallbacks(&m_musicEngineCallback);

	    // This sample plays the equivalent of background music, which we tag on the mastering voice as AudioCategory_GameMedia.
	    // In ordinary usage, if we were playing the music track with no effects, we could route it entirely through
	    // Media Foundation. Here we are using XAudio2 to apply a reverb effect to the music, so we use Media Foundation to
	    // decode the data then we feed it through the XAudio2 pipeline as a separate Mastering Voice, so that we can tag it
	    // as Game Media.
        // We default the mastering voice to 2 channels to simplify the reverb logic.
	    ThrowIfFailed(
		    m_musicEngine->CreateMasteringVoice(&m_musicMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, nullptr, nullptr, AudioCategory_GameMedia)
        );

        // Create a separate engine and mastering voice for sound effects in the sample
	    // Games will use many voices in a complex graph for audio, mixing all effects down to a
	    // single mastering voice.
	    // We are creating an entirely new engine instance and mastering voice in order to tag
	    // our sound effects with the audio category AudioCategory_GameEffects.
	    ThrowIfFailed(
		    XAudio2Create(&m_soundEffectEngine)
		    );
    
        m_soundEffectEngineCallback.Initialize(this);
        m_soundEffectEngine->RegisterForCallbacks(&m_soundEffectEngineCallback);

        // We default the mastering voice to 2 channels to simplify the reverb logic.
	    ThrowIfFailed(
		    m_soundEffectEngine->CreateMasteringVoice(&m_soundEffectMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, nullptr, nullptr, AudioCategory_GameEffects)
		    );
    }
    catch (...)
    {
        m_engineExperiencedCriticalError = true;
    }

	// create mp3 resources
	try
	{
		m_mediaEngine = nullptr;

		ComPtr<IMFMediaEngineClassFactory> mediaEngineFactory;
		ComPtr<IMFAttributes> mediaEngineAttributes;

		// Create the class factory for the Media Engine.
		ThrowIfFailed(
			CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mediaEngineFactory))
			);

		// Define configuration attributes.
		ThrowIfFailed(
			MFCreateAttributes(&mediaEngineAttributes, 1)
			);

		ComPtr<MediaEngineNotify> notify = Make<MediaEngineNotify>();
		ComPtr<IUnknown> unknownNotify;
		ThrowIfFailed(
			notify.As(&unknownNotify)
			);

		ThrowIfFailed(
			mediaEngineAttributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, unknownNotify.Get())
			);

		// Create the Media Engine.
		ThrowIfFailed(
			mediaEngineFactory->CreateInstance(0, mediaEngineAttributes.Get(), &m_mediaEngine)
			);

		ThrowIfFailed(
			m_mediaEngine->SetLoop(TRUE)
			);
	}
	catch (...)
	{
		m_mediaEngine = nullptr;
		m_engineExperiencedCriticalError = true;
	}

}

unsigned int Audio::Hash(const char *key)
{
    unsigned int len = strlen(key);
    const char *end=key+len;
    unsigned int hash;

    for (hash = 0; key < end; key++)
    {
        hash *= 16777619;
        hash ^= (unsigned int) (unsigned char) toupper(*key);
    }
    return (hash);
}

void Audio::ReleaseResources()
{
	if (m_musicMasteringVoice != nullptr) 
    {
        m_musicMasteringVoice->DestroyVoice();
        m_musicMasteringVoice = nullptr;
    }
	if (m_soundEffectMasteringVoice != nullptr) 
    {
        m_soundEffectMasteringVoice->DestroyVoice();
        m_soundEffectMasteringVoice = nullptr;
    }

    EffectList::iterator EffectIter = m_soundEffects.begin();
    for (; EffectIter != m_soundEffects.end(); EffectIter++)
	{
        if (EffectIter->second.m_soundEffectSourceVoice != nullptr) 
        {
            EffectIter->second.m_soundEffectSourceVoice->DestroyVoice();
            EffectIter->second.m_soundEffectSourceVoice = nullptr;
        }
	}
    m_soundEffects.clear();

    m_musicEngine = nullptr;
    m_soundEffectEngine = nullptr;

	// release mp3 player
	m_mediaEngine = nullptr;
}

void Audio::Start()
{	 
    if (m_engineExperiencedCriticalError)
    {
        return;
    }
	if (m_mediaEngine)
	{
		m_mediaEngine->Play(); // Ignore return result, the emulator may error out.
	}
	m_isAudioStarted = true;
}

// This sample processes audio buffers during the render cycle of the application.
// As long as the sample maintains a high-enough frame rate, this approach should
// not glitch audio. In game code, it is best for audio buffers to be processed
// on a separate thread that is not synced to the main render loop of the game.
void Audio::Render()
{
    if (m_engineExperiencedCriticalError)
    {
        ReleaseResources();
        Initialize();
        CreateResources();
        Start();
        if (m_engineExperiencedCriticalError)
        {
            return;
        }
    }
}

void Audio::PlayBackgroundMusic(const char* pszFilePath, bool bLoop)
{
	m_backgroundFile = pszFilePath;
    m_backgroundLoop = bLoop;

    if (m_engineExperiencedCriticalError) {
        return;
    }
	if (m_currentMusic != pszFilePath || m_mediaEngine->IsEnded())
	{
		// Set the music source.
		ThrowIfFailed(
			m_mediaEngine->SetSource(const_cast<wchar_t *>(a2ws(pszFilePath).c_str()))
			);

		ThrowIfFailed(
			m_mediaEngine->SetLoop(bLoop)
			);

		ThrowIfFailed(
			m_mediaEngine->SetVolume(m_backgroundMusicVolume)
			);

		m_currentMusic = pszFilePath;
		Start();
	}

}

void Audio::StopBackgroundMusic(bool bReleaseData)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    //StopSoundEffect(m_backgroundID);

    //if (bReleaseData)
    //    UnloadSoundEffect(m_backgroundID);
	if (m_mediaEngine)
	{
		// Reset music 
		m_currentMusic = NULL;
		m_mediaEngine->Pause();
		
	}

	m_isAudioStarted = false;

}

void Audio::PauseBackgroundMusic()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }
	if (m_isAudioStarted)
	{
		if (m_mediaEngine)
		{
			m_mediaEngine->Pause();
		}

		//m_soundEffectEngine->StopEngine();
	}
	m_isAudioStarted = false;

}

void Audio::ResumeBackgroundMusic()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }
	if (!m_isAudioStarted)
	{
		if (m_mediaEngine && m_mediaEngine->IsPaused())
		{
			m_mediaEngine->Play();
		}
	}
	m_isAudioStarted = true;
}

void Audio::RewindBackgroundMusic()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }
	// TODO: NOT IMPLEMENTED YET
    //RewindSoundEffect(m_backgroundID);
}

bool Audio::IsBackgroundMusicPlaying()
{
	return m_isAudioStarted;
}

void Audio::SetBackgroundVolume(float volume)
{
    m_backgroundMusicVolume = volume;

    if (m_engineExperiencedCriticalError) {
        return;
    }
	m_backgroundMusicVolume = volume;
	ThrowIfFailed(
		m_mediaEngine->SetVolume(volume)
		);
}

float Audio::GetBackgroundVolume()
{
    return m_backgroundMusicVolume;
}

void Audio::SetSoundEffectVolume(float volume)
{
    m_soundEffctVolume = volume;

    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        if (iter->first != m_backgroundID)
            iter->second.m_soundEffectSourceVoice->SetVolume(m_soundEffctVolume);
	}
}

float Audio::GetSoundEffectVolume()
{
    return m_soundEffctVolume;
}

void Audio::PlaySoundEffect(const char* pszFilePath, bool bLoop, unsigned int& sound, bool isMusic)
{
	if (!pszFilePath || !strcmp(pszFilePath, ""))
		return;

    sound = Hash(pszFilePath);

    if (m_soundEffects.end() == m_soundEffects.find(sound))
    {
        PreloadSoundEffect(pszFilePath, isMusic);
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    m_soundEffects[sound].m_audioBuffer.LoopCount = bLoop ? XAUDIO2_LOOP_INFINITE : 0;

    PlaySoundEffect(sound);
}

void Audio::PlaySoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    StopSoundEffect(sound);

    ThrowIfFailed(
		m_soundEffects[sound].m_soundEffectSourceVoice->SubmitSourceBuffer(&m_soundEffects[sound].m_audioBuffer)
		);

    if (m_engineExperiencedCriticalError) {
        // If there's an error, then we'll recreate the engine on the next render pass
        return;
    }

	SoundEffectData* soundEffect = &m_soundEffects[sound];
	HRESULT hr = soundEffect->m_soundEffectSourceVoice->Start();
	if FAILED(hr)
    {
        m_engineExperiencedCriticalError = true;
        return;
    }

	m_soundEffects[sound].m_soundEffectStarted = true;
}

void Audio::StopSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    HRESULT hr = m_soundEffects[sound].m_soundEffectSourceVoice->Stop();
    HRESULT hr1 = m_soundEffects[sound].m_soundEffectSourceVoice->FlushSourceBuffers();
    if (FAILED(hr) || FAILED(hr1))
    {
        // If there's an error, then we'll recreate the engine on the next render pass
        m_engineExperiencedCriticalError = true;
        return;
    }

    m_soundEffects[sound].m_soundEffectStarted = false;
}

void Audio::PauseSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    HRESULT hr = m_soundEffects[sound].m_soundEffectSourceVoice->Stop();
    if FAILED(hr)
    {
        // If there's an error, then we'll recreate the engine on the next render pass
        m_engineExperiencedCriticalError = true;
        return;
    }
}

void Audio::ResumeSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    HRESULT hr = m_soundEffects[sound].m_soundEffectSourceVoice->Start();
    if FAILED(hr)
    {
        // If there's an error, then we'll recreate the engine on the next render pass
        m_engineExperiencedCriticalError = true;
        return;
    }
}

void Audio::RewindSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    StopSoundEffect(sound);
    PlaySoundEffect(sound);
}

void Audio::PauseAllSoundEffects()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        PauseSoundEffect(iter->first);
	}
}

void Audio::ResumeAllSoundEffects()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        ResumeSoundEffect(iter->first);
	}
}

void Audio::StopAllSoundEffects()
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    EffectList::iterator iter;
	for (iter = m_soundEffects.begin(); iter != m_soundEffects.end(); iter++)
	{
        StopSoundEffect(iter->first);
	}
}

bool Audio::IsSoundEffectStarted(unsigned int sound)
{
    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return false;

    return m_soundEffects[sound].m_soundEffectStarted;
}

std::wstring CCUtf8ToUnicode(const char * pszUtf8Str)
{
    std::wstring ret;
    do
    {
        if (! pszUtf8Str) break;
        size_t len = strlen(pszUtf8Str);
        if (len <= 0) break;
		++len;
        wchar_t * pwszStr = new wchar_t[len];
        if (! pwszStr) break;
        pwszStr[len - 1] = 0;
        MultiByteToWideChar(CP_UTF8, 0, pszUtf8Str, len, pwszStr, len);
        ret = pwszStr;

		if(pwszStr) { 
			delete[] (pwszStr); 
			(pwszStr) = 0; 
		}


    } while (0);
    return ret;
}

std::string CCUnicodeToUtf8(const wchar_t* pwszStr)
{
	std::string ret;
	do
	{
		if(! pwszStr) break;
		size_t len = wcslen(pwszStr);
		if (len <= 0) break;
		
		char * pszUtf8Str = new char[len*3 + 1];
		WideCharToMultiByte(CP_UTF8, 0, pwszStr, len+1, pszUtf8Str, len*3 + 1, 0, 0);
		ret = pszUtf8Str;
				
		if(pszUtf8Str) { 
			delete[] (pszUtf8Str); 
			(pszUtf8Str) = 0; 
		}
	}while(0);

	return ret;
}

void Audio::PreloadSoundEffect(const char* pszFilePath, bool isMusic)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    int sound = Hash(pszFilePath);

	MediaStreamer mediaStreamer;
	mediaStreamer.Initialize(CCUtf8ToUnicode(pszFilePath).c_str());
	m_soundEffects[sound].m_soundID = sound;	
	
	uint32 bufferLength = mediaStreamer.GetMaxStreamLengthInBytes();
	m_soundEffects[sound].m_soundEffectBufferData = new byte[bufferLength];
	mediaStreamer.ReadAll(m_soundEffects[sound].m_soundEffectBufferData, bufferLength, &m_soundEffects[sound].m_soundEffectBufferLength);

    if (isMusic)
    {
        XAUDIO2_SEND_DESCRIPTOR descriptors[1];
	    descriptors[0].pOutputVoice = m_musicMasteringVoice;
	    descriptors[0].Flags = 0;
	    XAUDIO2_VOICE_SENDS sends = {0};
	    sends.SendCount = 1;
	    sends.pSends = descriptors;

        ThrowIfFailed(
	    m_musicEngine->CreateSourceVoice(&m_soundEffects[sound].m_soundEffectSourceVoice,
            &(mediaStreamer.GetOutputWaveFormatEx()), 0, 1.0f, &m_voiceContext, &sends)
	    );
		//fix bug: set a initial volume
		m_soundEffects[sound].m_soundEffectSourceVoice->SetVolume(m_backgroundMusicVolume);
    } else
    {
        XAUDIO2_SEND_DESCRIPTOR descriptors[1];
        descriptors[0].pOutputVoice = m_soundEffectMasteringVoice;
	    descriptors[0].Flags = 0;
	    XAUDIO2_VOICE_SENDS sends = {0};
	    sends.SendCount = 1;
	    sends.pSends = descriptors;
		
        ThrowIfFailed(
	    m_soundEffectEngine->CreateSourceVoice(&m_soundEffects[sound].m_soundEffectSourceVoice,
            &(mediaStreamer.GetOutputWaveFormatEx()), 0, 1.0f, &m_voiceContext, &sends, nullptr)
        );
		//fix bug: set a initial volume
		m_soundEffects[sound].m_soundEffectSourceVoice->SetVolume(m_soundEffctVolume);
    }

	m_soundEffects[sound].m_soundEffectSampleRate = mediaStreamer.GetOutputWaveFormatEx().nSamplesPerSec;

	// Queue in-memory buffer for playback
	ZeroMemory(&m_soundEffects[sound].m_audioBuffer, sizeof(m_soundEffects[sound].m_audioBuffer));

	m_soundEffects[sound].m_audioBuffer.AudioBytes = m_soundEffects[sound].m_soundEffectBufferLength;
	m_soundEffects[sound].m_audioBuffer.pAudioData = m_soundEffects[sound].m_soundEffectBufferData;
	m_soundEffects[sound].m_audioBuffer.pContext = &m_soundEffects[sound];
	m_soundEffects[sound].m_audioBuffer.Flags = XAUDIO2_END_OF_STREAM;
    m_soundEffects[sound].m_audioBuffer.LoopCount = 0;
}

void Audio::UnloadSoundEffect(const char* pszFilePath)
{
    int sound = Hash(pszFilePath);

    UnloadSoundEffect(sound);
}

void Audio::UnloadSoundEffect(unsigned int sound)
{
    if (m_engineExperiencedCriticalError) {
        return;
    }

    if (m_soundEffects.end() == m_soundEffects.find(sound))
        return;

    m_soundEffects[sound].m_soundEffectSourceVoice->DestroyVoice();

    if(m_soundEffects[sound].m_soundEffectBufferData)
        delete [] m_soundEffects[sound].m_soundEffectBufferData;

    m_soundEffects[sound].m_soundEffectBufferData = nullptr;
	m_soundEffects[sound].m_soundEffectSourceVoice = nullptr;
	m_soundEffects[sound].m_soundEffectStarted = false;
    ZeroMemory(&m_soundEffects[sound].m_audioBuffer, sizeof(m_soundEffects[sound].m_audioBuffer));

    m_soundEffects.erase(sound);
}
