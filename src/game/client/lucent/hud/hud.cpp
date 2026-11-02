#include "layout.h"
#include "visuals.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/demo.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>

#include <game/client/components/chat.h>
#include <game/client/components/hud.h>
#include <game/client/components/scoreboard.h>
#include <game/client/gameclient.h>
#include <game/layers.h>
#include <game/localization.h>

float CHud::LucentText(int Slot, const char *pText, float X, float Y, float Size, ColorRGBA Color, bool Center)
{
	auto &Text = m_aLucentText[Slot];
	const bool TimerText = Slot == LUCENT_TIMER || Slot == LUCENT_SECONDARY || Slot == LUCENT_CLOCK;
	const int PixelHeight = Graphics()->ScreenHeight();
	Size = LucentHud::TargetFontSize(Size, PixelHeight, m_Height);
	if((Text.m_Size != Size || Text.m_PixelHeight != PixelHeight) && Text.m_Index.Valid())
		TextRender()->DeleteTextContainer(Text.m_Index);
	if(!Text.m_Index.Valid() || str_comp(Text.m_aText, pText) != 0 || Text.m_Size != Size)
	{
		CTextCursor Cursor;
		Cursor.m_FontSize = Size;
		// A positive line width preserves full first/last advances instead of ink trimming.
		if(TimerText)
			Cursor.m_LineWidth = m_Width;
		const auto Flags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(TimerText ? TEXT_RENDER_FLAG_ONE_TIME_USE : ((Flags & ~TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT) | TEXT_RENDER_FLAG_ONE_TIME_USE));
		TextRender()->TextColor(1, 1, 1, 1);
		if(Text.m_Index.Valid())
			TextRender()->RecreateTextContainerSoft(Text.m_Index, &Cursor, pText);
		else
			TextRender()->CreateTextContainer(Text.m_Index, &Cursor, pText);
		TextRender()->SetRenderFlags(Flags);
		str_copy(Text.m_aText, pText);
		Text.m_Size = Size;
		Text.m_PixelHeight = PixelHeight;
	}
	float Width = 0;
	if(Text.m_Index.Valid())
	{
		Width = TextRender()->GetBoundingBoxTextContainer(Text.m_Index).m_W;
		const float Left = X - (Center ? Width / 2 : 0);
		// The shared surface supplies contrast without an offset glyph pass that
		// blurs small utility labels at fractional HUD scales.
		const vec2 Anchor = LucentHud::PixelGrid(Graphics()).Snap({Left, Y});
		TextRender()->RenderTextContainer(Text.m_Index, Color, ColorRGBA(0, 0, 0, 0), Anchor.x, Anchor.y);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	return Width;
}

void CHud::RenderLucentStatus(int ClientId, const CNetObj_Character *pCharacter, bool Extended)
{
	const float Alpha = LucentHud::Opacity(g_Config.m_LucentHudOpacity);
	if(Alpha <= 0 || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	const LucentHud::CLayout Layout(m_Width, m_Height, g_Config.m_LucentHudScale);
	const float UtilityScale = .75f * Layout.m_Scale;
	// Health/armor stay in the utility corner; jump/modifier art is not rendered there.
	Graphics()->MapScreenToSize(m_Width / UtilityScale, m_Height / UtilityScale);
	Graphics()->SetColor(1, 1, 1, Alpha);
	if(g_Config.m_ClShowhudHealthAmmo)
		RenderAmmoHealthAndArmor(pCharacter);
	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->MapScreenToSize(m_Width, m_Height);
	if(!Extended || GameClient()->m_Scoreboard.IsActive())
		return;
	const auto &Core = GameClient()->m_aClients[ClientId].m_Predicted;
	const auto &Player = GameClient()->m_aClients[ClientId].m_RenderCur;
	const auto &Snapshot = GameClient()->m_Snap.m_aCharacters[ClientId];
	struct CItem
	{
		IGraphics::CTextureHandle m_Texture;
		int m_Quad;
		bool m_Jump;
		bool m_Number;
	};
	std::array<CItem, LucentHud::MaxStatusItems> aItems{};
	std::array<float, LucentHud::MaxStatusItems> aWidths{};
	int Count = 0;
	auto Add = [&](bool Active, IGraphics::CTextureHandle Texture, int Quad, bool Jump = false, bool Number = false) {
		if(Active && Count < LucentHud::MaxStatusItems)
		{
			aItems[Count] = {Texture, Quad, Jump, Number};
			aWidths[Count++] = 8;
		}
	};
	char aJumps[96] = "";
	if(g_Config.m_ClShowhudJumpsIndicator)
	{
		const bool Grounded = Collision()->IsOnGround(vec2(Player.m_X, Player.m_Y), CCharacterCore::PhysicalSize());
		const auto Jumps = LucentHud::Jumps(Snapshot.m_HasExtendedDisplayInfo ? Core.m_Jumps : Snapshot.m_ExtendedData.m_Jumps,
			Core.m_JumpedTotal, Player.m_Jumped, Grounded, Core.m_EndlessJump, Snapshot.m_HasExtendedDisplayInfo);
		if(Jumps.m_Total > 0 && Jumps.m_Known && Jumps.m_Total <= 6)
		{
			for(int i = 0; i < Jumps.m_Total; ++i)
				Add(true, i < Jumps.m_Available ? GameClient()->m_HudSkin.m_SpriteHudAirjump : GameClient()->m_HudSkin.m_SpriteHudAirjumpEmpty,
					i < Jumps.m_Available ? m_AirjumpOffset : m_AirjumpEmptyOffset, true);
		}
		else if(Jumps.m_Total > 0)
		{
			if(Jumps.m_Known)
				str_format(aJumps, sizeof(aJumps), "%d/%d (%d %s)", Jumps.m_Available, Jumps.m_Total, Jumps.m_Used, Localize("used"));
			else
				str_format(aJumps, sizeof(aJumps), "?/%d", Jumps.m_Total);
			Add(true, GameClient()->m_HudSkin.m_SpriteHudAirjump, m_AirjumpOffset, true, true);
			aWidths[Count - 1] = 11.f + TextRender()->TextWidth(Layout.Unit(LucentHud::CaptionSize), aJumps) / Layout.m_Scale;
		}
	}
	const int JumpCount = Count;
	Add(Core.m_EndlessJump, GameClient()->m_HudSkin.m_SpriteHudEndlessJump, m_EndlessJumpOffset);
	Add(Core.m_EndlessHook, GameClient()->m_HudSkin.m_SpriteHudEndlessHook, m_EndlessHookOffset);
	Add(Core.m_Jetpack, GameClient()->m_HudSkin.m_SpriteHudJetpack, m_JetpackOffset);
	Add(Core.m_HasTelegunGun && Core.m_aWeapons[WEAPON_GUN].m_Got, GameClient()->m_HudSkin.m_SpriteHudTeleportGun, m_TeleportGunOffset);
	Add(Core.m_HasTelegunGrenade && Core.m_aWeapons[WEAPON_GRENADE].m_Got, GameClient()->m_HudSkin.m_SpriteHudTeleportGrenade, m_TeleportGrenadeOffset);
	Add(Core.m_HasTelegunLaser && Core.m_aWeapons[WEAPON_LASER].m_Got, GameClient()->m_HudSkin.m_SpriteHudTeleportLaser, m_TeleportLaserOffset);
	Add(Core.m_Solo, GameClient()->m_HudSkin.m_SpriteHudSolo, m_SoloOffset);
	Add(Core.m_CollisionDisabled, GameClient()->m_HudSkin.m_SpriteHudCollisionDisabled, m_CollisionDisabledOffset);
	Add(Core.m_HookHitDisabled, GameClient()->m_HudSkin.m_SpriteHudHookHitDisabled, m_HookHitDisabledOffset);
	Add(Core.m_HammerHitDisabled, GameClient()->m_HudSkin.m_SpriteHudHammerHitDisabled, m_HammerHitDisabledOffset);
	Add(Core.m_GrenadeHitDisabled && Core.m_HasTelegunGun && Core.m_aWeapons[WEAPON_GUN].m_Got, GameClient()->m_HudSkin.m_SpriteHudGunHitDisabled, m_GunHitDisabledOffset);
	Add(Core.m_ShotgunHitDisabled && Core.m_aWeapons[WEAPON_SHOTGUN].m_Got, GameClient()->m_HudSkin.m_SpriteHudShotgunHitDisabled, m_ShotgunHitDisabledOffset);
	Add(Core.m_GrenadeHitDisabled && Core.m_aWeapons[WEAPON_GRENADE].m_Got, GameClient()->m_HudSkin.m_SpriteHudGrenadeHitDisabled, m_GrenadeHitDisabledOffset);
	Add(Core.m_LaserHitDisabled && Core.m_aWeapons[WEAPON_LASER].m_Got, GameClient()->m_HudSkin.m_SpriteHudLaserHitDisabled, m_LaserHitDisabledOffset);
	const int Flags = Snapshot.m_HasExtendedDisplayInfo ? Snapshot.m_ExtendedData.m_Flags : 0;
	Add(Flags & CHARACTERFLAG_LOCK_MODE, GameClient()->m_HudSkin.m_SpriteHudLockMode, m_LockModeOffset);
	Add(Flags & CHARACTERFLAG_PRACTICE_MODE, GameClient()->m_HudSkin.m_SpriteHudPracticeMode, m_PracticeModeOffset);
	Add(Flags & CHARACTERFLAG_TEAM0_MODE, GameClient()->m_HudSkin.m_SpriteHudTeam0Mode, m_Team0ModeOffset);
	Add(Core.m_DeepFrozen, GameClient()->m_HudSkin.m_SpriteHudDeepFrozen, m_DeepFrozenOffset);
	Add(Core.m_LiveFrozen, GameClient()->m_HudSkin.m_SpriteHudLiveFrozen, m_LiveFrozenOffset);
	bool ChatVisible = g_Config.m_ClShowChat != 0;
#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current())
		ChatVisible = g_Config.m_ClVideoShowChat;
#endif
	const float ChatSide = ChatVisible ? 5.f + g_Config.m_ClChatWidth + Layout.Unit(LucentHud::Gap) : 0;
	const auto Status = Layout.Status(aWidths, Count, ChatSide, JumpCount);
	m_LucentStatusBounds = Status.m_Bounds;
	for(const auto &Group : Status.m_aGroups)
		if(Group.w > 0)
			LucentHud::Panel(Graphics(), Group, Alpha, Layout.m_Scale);
	for(int i = 0; i < Count; ++i)
	{
		const auto &Rect = Status.m_aItems[i];
		const float IconScale = Layout.Unit(8) / 12.f;
		Graphics()->TextureSet(aItems[i].m_Texture);
		Graphics()->SetColor(1, 1, 1, Alpha);
		Graphics()->QuadsSetRotation(0);
		Graphics()->RenderQuadContainerEx(m_HudQuadContainerIndex, aItems[i].m_Quad, 1,
			Rect.x - (aItems[i].m_Jump ? 5 * IconScale : 0), Rect.y - (aItems[i].m_Jump ? 29 * IconScale : 0), IconScale, IconScale);
		if(aItems[i].m_Number)
		{
			const float Width = TextRender()->TextWidth(Layout.Unit(LucentHud::CaptionSize), aJumps);
			const float Fit = std::min(1.f, (Rect.w - Layout.Unit(11)) / std::max(1.f, Width));
			LucentText(LUCENT_JUMPS, aJumps, Rect.x + Layout.Unit(11), Rect.y + Layout.Unit(1.5f), Layout.Unit(LucentHud::CaptionSize) * Fit, LucentHud::Text(Alpha));
		}
	}
	Graphics()->SetColor(1, 1, 1, 1);
	Graphics()->QuadsSetRotation(0);
}

void CHud::RenderLucentHotbar(int ClientId, const CNetObj_Character *pCharacter)
{
	if(!pCharacter || ClientId < 0 || ClientId >= MAX_CLIENTS || !g_Config.m_ClShowhudHealthAmmo || GameClient()->m_Scoreboard.IsActive())
		return;
	if(GameClient()->m_MultiViewActivated && !GameClient()->m_MultiViewShowHud)
		return;
	const float Alpha = LucentHud::Opacity(g_Config.m_LucentHudOpacity);
	if(Alpha <= 0)
		return;
	const auto &Snapshot = GameClient()->m_Snap.m_aCharacters[ClientId];
	const auto &Core = GameClient()->m_aClients[ClientId].m_Predicted;
	unsigned Owned = 0;
	for(int Weapon = 0; Weapon < NUM_WEAPONS; ++Weapon)
		if(Core.m_aWeapons[Weapon].m_Got)
			Owned |= 1u << Weapon;
	const int ActiveWeapon = GameClient()->m_aClients[ClientId].m_RenderCur.m_Weapon;
	static_assert(NUM_WEAPONS == 6);
	const auto Weapons = LucentHud::Weapons(Owned, ActiveWeapon, Snapshot.m_HasExtendedData);
	const double Now = time() / (double)time_freq();
	m_LucentWeaponScale.Update(Weapons, ActiveWeapon, ClientId, Now);
	if(Weapons.m_Count == 0)
		return;
	const LucentHud::CLayout Layout(m_Width, m_Height, g_Config.m_LucentHudScale);
	const auto Bar = Layout.Hotbar(Weapons.m_Count);
	m_LucentHotbarBounds = Bar;
	const float BarScale = Layout.m_Scale * LucentHud::HotbarFactor;
	LucentHud::Panel(Graphics(), Bar, Alpha, BarScale);
	std::array<float, NUM_WEAPONS> aSelection;
	for(int Weapon = 0; Weapon < NUM_WEAPONS; ++Weapon)
		aSelection[Weapon] = m_aLucentWeaponSelection[Weapon].Update(Weapon == ActiveWeapon, Now, true, LucentStyle::Motion::Selection);
	for(int Slot = 0; Slot < Weapons.m_Count; ++Slot)
	{
		const int Weapon = Weapons.m_aIds[Slot];
		const bool Active = Weapon == ActiveWeapon;
		const auto Rect = Layout.Slot(Slot, Weapons.m_Count);
		const float Selection = aSelection[Weapon];
		if(Selection > 0)
			LucentHud::SelectedSlot(Graphics(), Rect, Alpha, BarScale, Selection);
		const auto HudTexture = GameClient()->m_GameSkin.m_aLucentHudWeapons[Weapon];
		Graphics()->TextureSet(HudTexture.IsValid() ? HudTexture : GameClient()->m_GameSkin.m_aSpritePickupWeapons[Weapon]);
		Graphics()->SetColor(1, 1, 1, Alpha * (.90f + .10f * Selection));
		Graphics()->QuadsSetRotation(pi * 7 / 4);
		// The source quad is centered at the origin; keep its center fixed while
		// scaling both axes, independently of the slot highlight/opacity transition.
		const float SpriteScale = LucentHud::WeaponIconScale * BarScale * m_LucentWeaponScale.Scale(Weapon);
		Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, m_aWeaponOffset[Weapon], Rect.x + Rect.w / 2, Rect.y + Rect.h / 2, SpriteScale, SpriteScale);
		Graphics()->QuadsSetRotation(0);
		Graphics()->SetColor(1, 1, 1, 1);
		if(!Active)
			continue;
		if(Weapon == WEAPON_NINJA)
		{
			float Progress = -1;
			const int Max = g_pData->m_Weapons.m_Ninja.m_Duration * Client()->GameTickSpeed() / 1000;
			if(Max > 0 && Snapshot.m_HasExtendedDisplayInfo)
				Progress = (Core.m_Ninja.m_ActivationTick + Max - Client()->GameTick(g_Config.m_ClDummy)) / (float)Max;
			else if(Max > 0 && Client()->IsSixup() && !GameClient()->m_GameInfo.m_HudDDRace)
				Progress = (pCharacter->m_AmmoCount - Client()->GameTick(g_Config.m_ClDummy)) / (float)Max;
			if(Progress >= 0)
				Graphics()->DrawRect(Rect.x + Layout.BarUnit(3), Rect.y + Layout.BarUnit(17), (Rect.w - Layout.BarUnit(6)) * std::clamp(Progress, 0.f, 1.f), Layout.BarUnit(1), LucentHud::Accent(Alpha), 0, 0);
		}
		else if(GameClient()->m_GameInfo.m_HudAmmo && ActiveWeapon == pCharacter->m_Weapon && pCharacter->m_AmmoCount >= 0)
		{
			char aAmmo[16];
			str_format(aAmmo, sizeof(aAmmo), "%d", pCharacter->m_AmmoCount);
			LucentText(LUCENT_AMMO, aAmmo, Rect.x + Rect.w / 2, Rect.y + Layout.BarUnit(14), Layout.BarUnit(4), LucentHud::Text(Alpha), true);
		}
	}
}

void CHud::RenderLucentTimer(int Time)
{
	const float Alpha = LucentHud::Opacity(g_Config.m_LucentHudOpacity);
	if(Alpha <= 0 || GameClient()->m_Scoreboard.IsActive())
		return;
	const LucentHud::CLayout Layout(m_Width, m_Height, g_Config.m_LucentHudScale);
	char aMain[64] = "", aSecondary[128] = "", aDiff[32], aClock[6] = "";
	if(g_Config.m_ClShowLocalTimeAlways)
		str_timestamp_format(aClock, sizeof(aClock), "%H:%M");
	float SecondaryAlpha = Alpha;
	ColorRGBA SecondaryColor = LucentHud::Secondary(Alpha);
	if(g_Config.m_ClShowhudTimer && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
		str_time((int64_t)Time * 100, ETimeFormat::DAYS, aMain, sizeof(aMain));
	const bool Local = GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_Snap.m_SpecInfo.m_Active && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER);
	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	const int Speed = Client()->GameTickSpeed();
	const bool Finish = Local && m_DDRaceTime && m_ShowFinishTime && m_FinishTimeLastReceivedTick + Speed * 6 > Tick;
	const bool Checkpoint = Local && m_DDRaceTime && !m_ShowFinishTime && g_Config.m_ClShowhudTimeCpDiff && m_TimeCpLastReceivedTick + Speed * 6 > Tick;
	if(Finish || Checkpoint)
	{
		const int Received = Finish ? m_FinishTimeLastReceivedTick : m_TimeCpLastReceivedTick;
		SecondaryAlpha *= std::clamp((Received + Speed * 6 - Tick) / (float)(Speed * 2), 0.f, 1.f);
		const float Diff = Finish ? m_FinishTimeDiff : m_TimeCpDiff;
		str_time_float(std::abs(Diff), ETimeFormat::HOURS_CENTISECS, aDiff, sizeof(aDiff));
		if(Finish)
		{
			char aFinish[32];
			str_time(m_DDRaceTime, ETimeFormat::HOURS_CENTISECS, aFinish, sizeof(aFinish));
			str_format(aSecondary, sizeof(aSecondary), "Finish %s  %s%s", aFinish, Diff < 0 ? "-" : "+", aDiff);
		}
		else
			str_format(aSecondary, sizeof(aSecondary), "CP %s%s", Diff < 0 ? "-" : "+", aDiff);
		SecondaryColor = Diff < 0 ? LucentHud::Positive(SecondaryAlpha) : Diff > 0 ? LucentHud::Negative(SecondaryAlpha) :
											     LucentHud::Secondary(SecondaryAlpha);
	}
	if(!aMain[0] && !aSecondary[0] && !aClock[0])
		return;
	const float MainSize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::TimerSize), Graphics()->ScreenHeight(), m_Height);
	const float SecondarySize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::BodySize), Graphics()->ScreenHeight(), m_Height);
	const auto MeasureFlags = TextRender()->GetRenderFlags();
	TextRender()->SetRenderFlags(TEXT_RENDER_FLAG_ONE_TIME_USE);
	auto FormatWidth = [&](float Size, const char *pText) {
		char aFormat[128];
		LucentHud::TimerFormatTemplate(aFormat, pText);
		return TextRender()->TextWidth(Size, aFormat, -1, m_Width);
	};
	const float Width = std::max(FormatWidth(MainSize, aMain), FormatWidth(SecondarySize, aSecondary)) / Layout.m_Scale;
	const float ClockSize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::CaptionSize), Graphics()->ScreenHeight(), m_Height);
	const float ClockWidth = FormatWidth(ClockSize, aClock) / Layout.m_Scale;
	TextRender()->SetRenderFlags(MeasureFlags);
	auto Panel = Layout.Timer(Width, aMain[0] != 0, aSecondary[0] != 0, ClockWidth);
	if(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH)
		Panel.y = std::max(Panel.y, 18.f);
	m_LucentTimerBounds = Panel;
	LucentHud::Panel(Graphics(), Panel, Alpha, Layout.m_Scale);
	const float GameCenter = m_Width / 2;
	if(aClock[0])
		LucentText(LUCENT_CLOCK, aClock, GameCenter, Layout.TimerClockY(Panel, aMain[0] != 0, aSecondary[0] != 0), ClockSize, LucentHud::Secondary(Alpha), true);
	if(aMain[0])
	{
		ColorRGBA Color = LucentHud::Text(Alpha);
		if(GameClient()->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 && GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0)
			Color = LucentHud::Negative(Alpha * (Time <= 10 ? .75f + .25f * std::cos(2 * pi * time() / (double)time_freq()) : 1.f));
		LucentText(LUCENT_TIMER, aMain, GameCenter, Layout.TimerLineY(Panel, false, aMain[0] != 0), MainSize, Color, true);
	}
	if(aSecondary[0])
		LucentText(LUCENT_SECONDARY, aSecondary, GameCenter, Layout.TimerLineY(Panel, true, aMain[0] != 0), SecondarySize, SecondaryColor, true);
}

void CHud::RenderLucentRecording()
{
	const float Alpha = LucentHud::Opacity(g_Config.m_LucentHudOpacity);
	if(!g_Config.m_LucentHudRecording || Alpha <= 0 || GameClient()->m_Scoreboard.IsActive() || !GameClient()->m_Chat.IsExpanded())
		return;
#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
		return;
	const LucentHud::CLayout Layout(m_Width, m_Height, g_Config.m_LucentHudScale);
	const int aRecorders[] = {RECORDER_MANUAL, RECORDER_AUTO, RECORDER_RACE, RECORDER_REPLAYS};
	const char *apLabels[] = {"REC", "AUTO", "RACE", "REPLAY"};
	bool aActive[4] = {};
	char aaTimes[2][32] = {};
	const float LabelSize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::CaptionSize), Graphics()->ScreenHeight(), m_Height),
		    TimeSize = LucentHud::TargetFontSize(Layout.Unit(LucentHud::BodySize), Graphics()->ScreenHeight(), m_Height);
	const float LabelGap = Layout.Unit(2.5f), ItemGap = Layout.Unit(5.f);
	float ContentWidth = 0;
	for(int i = 0; i < 4; ++i)
	{
		const auto *pRecorder = DemoRecorder(aRecorders[i]);
		aActive[i] = pRecorder->IsRecording();
		if(!aActive[i])
			continue;
		if(ContentWidth > 0)
			ContentWidth += ItemGap;
		ContentWidth += TextRender()->TextWidth(LabelSize, apLabels[i]);
		if(i < 2)
		{
			str_time((int64_t)pRecorder->Length() * 100, ETimeFormat::HOURS, aaTimes[i], sizeof(aaTimes[i]));
			ContentWidth += LabelGap + TextRender()->TextWidth(TimeSize, aaTimes[i]);
		}
	}
	if(ContentWidth <= 0)
		return;
	// Stack below optional existing FPS/prediction readouts, keeping timer clearance.
	const float Offset = (g_Config.m_ClShowfps ? 18.f : 0.f) + (g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK ? 18.f : 0.f);
	const auto Panel = LucentHud::BelowIfOverlapping(Layout.TopRight(ContentWidth / Layout.m_Scale + 14.f, 12.f, Offset), m_LucentTimerBounds, Layout.Unit(LucentHud::Gap));
	LucentHud::Panel(Graphics(), Panel, Alpha, Layout.m_Scale);
	Graphics()->DrawRect(Panel.x + Layout.Unit(4), Panel.y + Layout.Unit(5), Layout.Unit(2), Layout.Unit(2), LucentHud::Negative(Alpha), IGraphics::CORNER_ALL, Layout.Unit(1));
	float X = Panel.x + Layout.Unit(9);
	for(int i = 0; i < 4; ++i)
	{
		if(!aActive[i])
			continue;
		LucentText(LUCENT_RECORDING + i, apLabels[i], X, Panel.y + Layout.Unit(LucentHud::TextPaddingY + (LucentHud::BodySize - LucentHud::CaptionSize) / 2), LabelSize, LucentHud::Secondary(Alpha));
		X += TextRender()->TextWidth(LabelSize, apLabels[i]);
		if(i < 2)
		{
			X += LabelGap;
			LucentText(LUCENT_RECORDING_TIME + i, aaTimes[i], X, Panel.y + Layout.Unit(LucentHud::TextPaddingY), TimeSize, LucentHud::Text(Alpha));
			X += TextRender()->TextWidth(TimeSize, aaTimes[i]);
		}
		X += ItemGap;
	}
}

void CHud::RenderLucentCoordinates()
{
	const float Alpha = LucentHud::Opacity(g_Config.m_LucentHudOpacity);
	const int ClientId = GameClient()->m_Snap.m_SpecInfo.m_Active ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : GameClient()->m_Snap.m_LocalClientId;
	if(Alpha <= 0 || ClientId < SPEC_FREEVIEW || ClientId >= MAX_CLIENTS || GameClient()->m_Scoreboard.IsActive())
		return;
	const bool PosOnly = ClientId == SPEC_FREEVIEW || GameClient()->m_aClients[ClientId].m_SpecCharPresent;
	const bool Speed = !PosOnly && g_Config.m_ClShowhudPlayerSpeed;
	if(!g_Config.m_LucentHudCoordinates && !Speed)
		return;
	const auto Info = GetMovementInformation(ClientId, g_Config.m_ClDummy);
	char aText[160] = "", aSpeedX[48] = "", aSpeedY[48] = "";
	if(g_Config.m_LucentHudCoordinates)
	{
		if(PosOnly)
			str_format(aText, sizeof(aText), "X %.2f Y %.2f", Info.m_Pos.x, Info.m_Pos.y);
		else
			str_format(aText, sizeof(aText), "X %.2f Y %.2f ∠ %.1f", Info.m_Pos.x, Info.m_Pos.y, Info.m_Angle);
	}
	if(Speed)
	{
		str_format(aSpeedX, sizeof(aSpeedX), "Vx %.2f", Info.m_Speed.x);
		str_format(aSpeedY, sizeof(aSpeedY), "Vy %.2f", Info.m_Speed.y);
	}
	const LucentHud::CLayout Layout(m_Width, m_Height, g_Config.m_LucentHudScale);
	const float Size = LucentHud::TargetFontSize(Layout.Unit(LucentHud::CoordinatesSize), Graphics()->ScreenHeight(), m_Height);
	// Reserve format cells so signs/digit counts cannot move the second axis or
	// resize the panel every frame. Larger map coordinates can still grow to fit.
	const float AxisWidth = std::max({TextRender()->TextWidth(Size, "Vx -000.00"), TextRender()->TextWidth(Size, aSpeedX), TextRender()->TextWidth(Size, aSpeedY)});
	const float SpeedWidth = 2 * AxisWidth + Layout.Unit(LucentHud::Gap);
	const float CoordinateWidth = aText[0] ? std::max(TextRender()->TextWidth(Size, aText), TextRender()->TextWidth(Size, PosOnly ? "X -0000.00 Y -0000.00" : "X -0000.00 Y -0000.00 \u2220 000.0")) : 0.f;
	const float Width = std::max(CoordinateWidth, Speed ? SpeedWidth : 0.f) + Layout.Unit(2 * LucentHud::CoordinatesPadding);
	const auto Panel = Layout.Coordinates(Width, g_Config.m_ClShowhudScore, Speed && aText[0], m_LucentStatusBounds);
	m_LucentCoordinatesBounds = Panel;
	LucentHud::Panel(Graphics(), Panel, Alpha, Layout.m_Scale);
	if(aText[0])
		LucentText(LUCENT_COORDINATES, aText, Panel.x + Layout.Unit(LucentHud::CoordinatesPadding), Panel.y + Layout.Unit(1.5f), Size, LucentHud::Text(Alpha));
	if(Speed)
	{
		const float Y = Panel.y + Layout.Unit(aText[0] ? 8.5f : 1.5f);
		auto SpeedColor = [&](int Axis) {
			return m_aLastPlayerSpeedChange[Axis] == ESpeedChange::INCREASE ? LucentHud::Positive(Alpha) : m_aLastPlayerSpeedChange[Axis] == ESpeedChange::DECREASE ? LucentHud::Negative(Alpha) :
																						  LucentHud::Secondary(Alpha);
		};
		const float Left = Panel.x + Layout.Unit(LucentHud::CoordinatesPadding);
		LucentText(LUCENT_SPEED_X, aSpeedX, Left, Y, Size, SpeedColor(0));
		LucentText(LUCENT_SPEED_Y, aSpeedY, Left + AxisWidth + Layout.Unit(LucentHud::Gap), Y, Size, SpeedColor(1));
	}
}
