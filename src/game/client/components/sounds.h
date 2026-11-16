/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SOUNDS_H
#define GAME_CLIENT_COMPONENTS_SOUNDS_H

#include <base/vmath.h>

#include <engine/shared/jobs.h>
#include <engine/shared/sound_pack.h>
#include <engine/sound.h>

#include <game/client/component.h>

class CSoundLoading : public IJob
{
	CGameClient *m_pGameClient;
	bool m_Render;

public:
	CSoundLoading(CGameClient *pGameClient, bool Render);
	void Run() override;
};

class CSounds : public CComponent
{
	enum
	{
		QUEUE_SIZE = 32,
	};
	class CQueueEntry
	{
	public:
		int m_Channel;
		int m_SetId;
	};
	CQueueEntry m_aQueue[QUEUE_SIZE];
	int m_QueuePos;
	int64_t m_QueueWaitTime;
	std::shared_ptr<CSoundLoading> m_pSoundJob;
	bool m_WaitForSoundJob = false;
	bool m_PackInitialized = false;
	bool m_SoundPackComparing = false;
	std::vector<int> m_DefaultSamples;
	LucentSoundPacks::CSelection m_PackSelection, m_ImportSelection;
	LucentSoundPacks::CBytes m_ImportZip;
	std::vector<LucentSoundPacks::CEntry> m_ActiveEntries, m_ImportEntries;
	std::string m_ImportName;
	void PublishSoundSamples(const std::map<size_t, int> &Pending);
	std::string m_ActivePack, m_PackError;
	bool ApplySoundPack(const char *pName, const LucentSoundPacks::CBytes *pImport = nullptr, bool PreviewOnly = false);

	void UpdateChannels();
	int GetSampleId(int SetId);

	float m_GuiSoundVolume = -1.0f;
	float m_GameSoundVolume = -1.0f;
	float m_MapSoundVolume = -1.0f;
	float m_BackgroundMusicVolume = -1.0f;

public:
	bool ImportSoundPack(const char *pAbsolutePath, bool PreviewOnly = false);
	bool CommitSoundImport();
	void SetSoundPackComparing(bool Comparing);
	bool SoundPackComparing() const { return m_SoundPackComparing; }
	void CancelSoundImport();
	bool SaveSoundPreset(const char *pLabel);
	int SoundPackSample(size_t Index, bool Original = false, bool Pending = false) const;
	bool SoundReplaced(size_t Index, bool Pending = false) const;
	size_t SoundReplacementCount(bool Pending = false) const;
	bool SetSoundPack(const char *pName);
	const char *SoundPackError() const { return m_PackError.c_str(); }
	bool SoundPacksReady() const { return m_PackInitialized && !m_WaitForSoundJob && Sound()->IsSoundEnabled(); }

	// sound channels
	enum
	{
		CHN_GUI = 0,
		CHN_MUSIC,
		CHN_WORLD,
		CHN_GLOBAL,
		CHN_MAPSOUND,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;

	void ClearQueue();
	void Enqueue(int Channel, int SetId);
	void Play(int Channel, int SetId, float Volume);
	void PlayAt(int Channel, int SetId, float Volume, vec2 Position);
	void PlayAndRecord(int Channel, int SetId, float Volume, vec2 Position);
	void Stop(int SetId);
	bool IsPlaying(int SetId);

	ISound::CVoiceHandle PlaySample(int Channel, int SampleId, int Flags, float Volume);
	ISound::CVoiceHandle PlaySampleAt(int Channel, int SampleId, int Flags, float Volume, vec2 Position);
};

#endif
