/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "sounds.h"

#include <base/fs.h>
#include <base/hash.h>
#include <base/mem.h>
#include <base/time.h>

#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/sound.h>

#include <generated/client_data.h>

#include <game/client/components/camera.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

CSoundLoading::CSoundLoading(CGameClient *pGameClient, bool Render) :
	m_pGameClient(pGameClient),
	m_Render(Render)
{
	Abortable(true);
}

void CSoundLoading::Run()
{
	for(int s = 0; s < g_pData->m_NumSounds; s++)
	{
		const char *pLoadingCaption = Localize("Loading Lucent Client");
		const char *pLoadingContent = Localize("Loading sound files");

		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; i++)
		{
			if(State() == IJob::STATE_ABORTED)
				return;

			int Id = m_pGameClient->Sound()->LoadWV(g_pData->m_aSounds[s].m_aSounds[i].m_pFilename);
			g_pData->m_aSounds[s].m_aSounds[i].m_Id = Id;
			// try to render a frame
			if(m_Render)
				m_pGameClient->m_Menus.RenderLoading(pLoadingCaption, pLoadingContent, 0);
		}

		if(m_Render)
			m_pGameClient->m_Menus.RenderLoading(pLoadingCaption, pLoadingContent, 1);
	}
}

void CSounds::UpdateChannels()
{
	const float NewGuiSoundVolume = g_Config.m_SndChatVolume / 100.0f;
	if(NewGuiSoundVolume != m_GuiSoundVolume)
	{
		m_GuiSoundVolume = NewGuiSoundVolume;
		Sound()->SetChannel(CSounds::CHN_GUI, m_GuiSoundVolume, 0.0f);
	}

	const float NewGameSoundVolume = g_Config.m_SndGameVolume / 100.0f;
	if(NewGameSoundVolume != m_GameSoundVolume)
	{
		m_GameSoundVolume = NewGameSoundVolume;
		Sound()->SetChannel(CSounds::CHN_WORLD, 0.9f * m_GameSoundVolume, 1.0f);
		Sound()->SetChannel(CSounds::CHN_GLOBAL, m_GameSoundVolume, 0.0f);
	}

	const float NewMapSoundVolume = g_Config.m_SndMapVolume / 100.0f;
	if(NewMapSoundVolume != m_MapSoundVolume)
	{
		m_MapSoundVolume = NewMapSoundVolume;
		Sound()->SetChannel(CSounds::CHN_MAPSOUND, m_MapSoundVolume, 1.0f);
	}

	const float NewBackgroundMusicVolume = g_Config.m_SndBackgroundMusicVolume / 100.0f;
	if(NewBackgroundMusicVolume != m_BackgroundMusicVolume)
	{
		m_BackgroundMusicVolume = NewBackgroundMusicVolume;
		Sound()->SetChannel(CSounds::CHN_MUSIC, m_BackgroundMusicVolume, 1.0f);
	}
}

int CSounds::GetSampleId(int SetId)
{
	if(!g_Config.m_SndEnable || !Sound()->IsSoundEnabled() || m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return -1;

	CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	if(!pSet->m_NumSounds)
		return -1;

	if(pSet->m_NumSounds == 1)
		return pSet->m_aSounds[0].m_Id;

	// return random one
	int Id;
	do
	{
		Id = rand() % pSet->m_NumSounds;
	} while(Id == pSet->m_Last);
	pSet->m_Last = Id;
	return pSet->m_aSounds[Id].m_Id;
}

void CSounds::OnInit()
{
	UpdateChannels();
	ClearQueue();

	// load sounds
	if(g_Config.m_ClThreadsoundloading)
	{
		m_pSoundJob = std::make_shared<CSoundLoading>(GameClient(), false);
		GameClient()->Engine()->AddJob(m_pSoundJob);
		m_WaitForSoundJob = true;
		GameClient()->m_Menus.RenderLoading(Localize("Loading Lucent Client"), Localize("Loading sound files"), 0);
	}
	else
	{
		CSoundLoading(GameClient(), true).Run();
		m_WaitForSoundJob = false;
	}
}

void CSounds::OnReset()
{
	if(Client()->State() >= IClient::STATE_ONLINE)
	{
		Sound()->StopAll();
		ClearQueue();
	}
}

void CSounds::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
		OnReset();
}

void CSounds::OnRender()
{
	// check for sound initialisation
	if(m_WaitForSoundJob)
	{
		if(m_pSoundJob->State() == IJob::STATE_DONE)
			m_WaitForSoundJob = false;
		else
			return;
	}

	if(!m_PackInitialized)
	{
		for(int s = 0; s < g_pData->m_NumSounds; ++s)
			for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; ++i)
				m_DefaultSamples.push_back(g_pData->m_aSounds[s].m_aSounds[i].m_Id);
		m_PackInitialized = true;
	}
	if(Sound()->IsSoundEnabled() && m_ActivePack != g_Config.m_ClLucentSoundPack)
		SetSoundPack(g_Config.m_ClLucentSoundPack);

	Sound()->SetListenerPosition(GameClient()->m_Camera.m_Center);
	UpdateChannels();

	// play sound from queue
	if(m_QueuePos > 0)
	{
		int64_t Now = time();
		if(m_QueueWaitTime <= Now)
		{
			Play(m_aQueue[0].m_Channel, m_aQueue[0].m_SetId, 1.0f);
			m_QueueWaitTime = Now + time_freq() * 3 / 10; // wait 300ms before playing the next one
			if(--m_QueuePos > 0)
				mem_move(m_aQueue, m_aQueue + 1, m_QueuePos * sizeof(CQueueEntry));
		}
	}
}

void CSounds::ClearQueue()
{
	mem_zero(m_aQueue, sizeof(m_aQueue));
	m_QueuePos = 0;
	m_QueueWaitTime = time();
}

void CSounds::Enqueue(int Channel, int SetId)
{
	if(m_SoundPackComparing && Channel == CHN_MUSIC && SetId == SOUND_MENU)
		return;
	if(GameClient()->m_SuppressEvents)
		return;
	if(m_QueuePos >= QUEUE_SIZE)
		return;
	if(Channel != CHN_MUSIC && g_Config.m_ClEditor)
		return;

	m_aQueue[m_QueuePos].m_Channel = Channel;
	m_aQueue[m_QueuePos++].m_SetId = SetId;
}

void CSounds::PlayAndRecord(int Channel, int SetId, float Volume, vec2 Position)
{
	// TODO: Volume and position are currently not recorded for sounds played with this function
	// TODO: This also causes desync sounds during demo playback of demos recorded on high ping servers:
	//       https://github.com/ddnet/ddnet/issues/1282
	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundId = SetId;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_NOSEND | MSGFLAG_RECORD);

	PlayAt(Channel, SetId, Volume, Position);
}

void CSounds::Play(int Channel, int SetId, float Volume)
{
	if(m_SoundPackComparing && Channel == CHN_MUSIC && SetId == SOUND_MENU)
		return;
	PlaySample(Channel, GetSampleId(SetId), 0, Volume);
}

void CSounds::PlayAt(int Channel, int SetId, float Volume, vec2 Position)
{
	PlaySampleAt(Channel, GetSampleId(SetId), 0, Volume, Position);
}

void CSounds::Stop(int SetId)
{
	if(m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return;

	const CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	for(int i = 0; i < pSet->m_NumSounds; i++)
		if(pSet->m_aSounds[i].m_Id != -1)
			Sound()->Stop(pSet->m_aSounds[i].m_Id);
}

bool CSounds::IsPlaying(int SetId)
{
	if(m_WaitForSoundJob || SetId < 0 || SetId >= g_pData->m_NumSounds)
		return false;

	const CDataSoundset *pSet = &g_pData->m_aSounds[SetId];
	for(int i = 0; i < pSet->m_NumSounds; i++)
		if(pSet->m_aSounds[i].m_Id != -1 && Sound()->IsPlaying(pSet->m_aSounds[i].m_Id))
			return true;
	return false;
}

ISound::CVoiceHandle CSounds::PlaySample(int Channel, int SampleId, int Flags, float Volume)
{
	if(GameClient()->m_SuppressEvents || (Channel == CHN_MUSIC && !g_Config.m_SndMusic) || SampleId == -1)
		return ISound::CVoiceHandle();

	if(Channel == CHN_MUSIC)
		Flags |= ISound::FLAG_LOOP;

	return Sound()->Play(Channel, SampleId, Flags, Volume);
}

ISound::CVoiceHandle CSounds::PlaySampleAt(int Channel, int SampleId, int Flags, float Volume, vec2 Position)
{
	if(GameClient()->m_SuppressEvents || (Channel == CHN_MUSIC && !g_Config.m_SndMusic) || SampleId == -1)
		return ISound::CVoiceHandle();

	if(Channel == CHN_MUSIC)
		Flags |= ISound::FLAG_LOOP;

	return Sound()->PlayAt(Channel, SampleId, Flags, Volume, Position);
}

// All mutations run on the game thread after the asynchronous default loader completes.
bool CSounds::ApplySoundPack(const char *pName, const LucentSoundPacks::CBytes *pImport, bool PreviewOnly)
{
	using namespace LucentSoundPacks;
	if(!SoundPacksReady())
	{
		m_PackError = "Sound device is disabled or sounds are still loading";
		return false;
	}
	if(*pName && !Managed(pName))
	{
		m_PackError = "Invalid sound pack name";
		return false;
	}
	CBytes Zip;
	std::vector<CEntry> Entries;
	std::vector<std::string> Catalog;
	for(int s = 0; s < g_pData->m_NumSounds; ++s)
		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; ++i)
			Catalog.emplace_back(g_pData->m_aSounds[s].m_aSounds[i].m_pFilename);
	std::map<size_t, const CEntry *> Matches;
	const std::string Path = std::string(Directory) + "/" + pName;
	if(*pName && ((!pImport && !Read(Storage(), Path.c_str(), IStorage::TYPE_SAVE, Zip)) || !Parse(pImport ? *pImport : Zip, Entries) || !Match(Entries, Catalog, Matches)))
	{
		m_PackError = "Invalid sound pack or no matching DDNet filenames";
		return false;
	}
	const std::string Requested = pName;
	double DecodedBytes = 0;
	auto &Selection = PreviewOnly ? m_ImportSelection : m_PackSelection;
	const bool Success = Selection.Replace(Matches, [&](const CEntry &Entry) {
		const int Id = str_endswith_nocase(Entry.m_Path.c_str(), ".opus") ?
			Sound()->LoadOpusFromMem(Entry.m_Data.data(), Entry.m_Data.size(), false, Entry.m_Path.c_str(), 32 * 1024 * 1024) :
			Sound()->LoadWVFromMem(Entry.m_Data.data(), Entry.m_Data.size(), false, Entry.m_Path.c_str(), 32 * 1024 * 1024);
		if(Id >= 0)
		{
			DecodedBytes += Sound()->GetSampleTotalTime(Id) * Sound()->MixingRate() * 4.0;
			if(DecodedBytes > 128 * 1024 * 1024)
			{
				Sound()->UnloadSample(Id);
				return -1;
			}
		}
		return Id; }, [&](int Id) { Sound()->UnloadSample(Id); }, [&](const std::map<size_t, int> &Pending) {
		if(PreviewOnly) return true;
		if(pImport && !Save(Storage(), Requested.c_str(), *pImport)) return false;
		PublishSoundSamples(Pending);
		return true; });
	if(!Success)
	{
		m_PackError = "Could not load or save sound pack. Previous sounds retained.";
		return false;
	}
	std::vector<CEntry> Normalized;
	for(const auto &[Index, pEntry] : Matches)
	{
		std::string Path = Catalog[Index];
		if(str_endswith_nocase(pEntry->m_Path.c_str(), ".opus"))
			Path.replace(Path.size() - 3, 3, ".opus");
		Normalized.push_back({Path, pEntry->m_Data});
	}
	if(PreviewOnly)
	{
		m_ImportZip = *pImport;
		m_ImportName = Requested;
		m_ImportEntries = std::move(Normalized);
	}
	else
	{
		m_ActiveEntries = std::move(Normalized);
		m_ActivePack = Requested;
		str_copy(g_Config.m_ClLucentSoundPack, m_ActivePack.c_str());
	}
	m_PackError.clear();
	return true;
}

bool CSounds::SetSoundPack(const char *pName)
{
	if(ApplySoundPack(pName))
		return true;
	str_copy(g_Config.m_ClLucentSoundPack, m_ActivePack.c_str());
	return false;
}

bool CSounds::ImportSoundPack(const char *pAbsolutePath, bool PreviewOnly)
{
	using namespace LucentSoundPacks;
	const char *pFilename = fs_filename(pAbsolutePath);
	if(!ImportSupported(pFilename))
	{
		m_PackError = "Choose a ZIP pack or an individual WV file";
		return false;
	}
	const bool SingleWv = str_endswith_nocase(pFilename, ".wv");
	CBytes Zip;
	if(!Read(Storage(), pAbsolutePath, IStorage::TYPE_ABSOLUTE, Zip, SingleWv ? MaxFile : MaxArchive))
	{
		m_PackError = SingleWv ? "Could not read WV (maximum 16 MiB)" : "Could not read ZIP (maximum 64 MiB)";
		return false;
	}
	if(SingleWv)
	{
		CBytes Data = std::move(Zip);
		std::vector<std::string> Catalog;
		for(int s = 0; s < g_pData->m_NumSounds; ++s)
			for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; ++i)
				Catalog.emplace_back(g_pData->m_aSounds[s].m_aSounds[i].m_pFilename);
		if(!BuildSingleImport(m_ActiveEntries, pFilename, Data, Catalog, Zip))
		{
			m_PackError = "WV filename must match a standard DDNet sound";
			return false;
		}
	}
	char aHash[SHA256_MAXSTRSIZE];
	sha256_str(sha256(Zip.data(), Zip.size()), aHash, sizeof(aHash));
	const auto Name = Slug(pFilename);
	return ApplySoundPack((Name + "-" + aHash + ".zip").c_str(), &Zip, PreviewOnly);
}

void CSounds::PublishSoundSamples(const std::map<size_t, int> &Pending)
{
	const bool MusicPlaying = IsPlaying(SOUND_MENU);
	size_t Index = 0;
	for(int s = 0; s < g_pData->m_NumSounds; ++s)
	{
		for(int i = 0; i < g_pData->m_aSounds[s].m_NumSounds; ++i, ++Index)
		{
			auto &Sample = g_pData->m_aSounds[s].m_aSounds[i];
			const auto It = Pending.find(Index);
			const int NewId = It == Pending.end() ? m_DefaultSamples[Index] : It->second;
			if(Sample.m_Id != NewId && Sample.m_Id >= 0)
				Sound()->Stop(Sample.m_Id);
			Sample.m_Id = NewId;
		}
		g_pData->m_aSounds[s].m_Last = -1;
	}
	if(MusicPlaying && !IsPlaying(SOUND_MENU))
		Play(CHN_MUSIC, SOUND_MENU, 1.0f);
}

int CSounds::SoundPackSample(size_t Index, bool Original, bool Pending) const
{
	if(!SoundPacksReady() || Index >= m_DefaultSamples.size())
		return -1;
	if(Original)
		return m_DefaultSamples[Index];
	const auto &Samples = Pending ? m_ImportSelection.Samples() : m_PackSelection.Samples();
	const auto It = Samples.find(Index);
	return It == Samples.end() ? m_DefaultSamples[Index] : It->second;
}

bool CSounds::SoundReplaced(size_t Index, bool Pending) const
{
	const auto &Samples = Pending ? m_ImportSelection.Samples() : m_PackSelection.Samples();
	return Samples.find(Index) != Samples.end();
}

size_t CSounds::SoundReplacementCount(bool Pending) const
{
	return Pending ? m_ImportSelection.Samples().size() : m_PackSelection.Samples().size();
}

void CSounds::CancelSoundImport()
{
	m_ImportSelection.Replace({}, [](const auto &) { return -1; }, [&](int Id) { Sound()->UnloadSample(Id); }, [](const auto &) { return true; });
	m_ImportZip.clear();
	m_ImportEntries.clear();
	m_ImportName.clear();
}

bool CSounds::CommitSoundImport()
{
	if(m_ImportName.empty() || !SoundPacksReady())
		return false;
	if(!m_PackSelection.Adopt(m_ImportSelection, [&](int Id) { Sound()->UnloadSample(Id); }, [&](const auto &Pending) {
		if(!LucentSoundPacks::Save(Storage(), m_ImportName.c_str(), m_ImportZip)) return false;
		PublishSoundSamples(Pending);
		return true; }))
	{
		m_PackError = "Could not save import. Previous sounds retained.";
		return false;
	}
	m_ActivePack = m_ImportName;
	m_ActiveEntries = std::move(m_ImportEntries);
	str_copy(g_Config.m_ClLucentSoundPack, m_ActivePack.c_str());
	CancelSoundImport();
	m_PackError.clear();
	return true;
}

bool CSounds::SaveSoundPreset(const char *pLabel)
{
	using namespace LucentSoundPacks;
	CBytes Zip;
	if(!SoundPacksReady() || !*pLabel || !Encode(m_ActiveEntries, Zip))
	{
		m_PackError = "Choose custom sounds and enter a preset name";
		return false;
	}
	char aHash[SHA256_MAXSTRSIZE];
	sha256_str(sha256(Zip.data(), Zip.size()), aHash, sizeof(aHash));
	return ApplySoundPack((Slug(pLabel) + "-" + aHash + ".zip").c_str(), &Zip);
}

void CSounds::SetSoundPackComparing(bool Comparing)
{
	if(Comparing && !m_SoundPackComparing)
		Stop(SOUND_MENU);
	m_SoundPackComparing = Comparing;
}
