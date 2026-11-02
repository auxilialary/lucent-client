#ifndef GAME_CLIENT_LUCENT_HUD_TEE_ALIGNMENT_H
#define GAME_CLIENT_LUCENT_HUD_TEE_ALIGNMENT_H

#include "layout.h"

#include <base/str.h>

#include <game/client/lucent/motion/transition.h>

#include <algorithm>
#include <cmath>

namespace LucentHud::Alignment
{
	constexpr float EnterX = 2.f;
	constexpr float LeaveX = 3.f;
	constexpr float CapsuleHeight = 18.f;
	constexpr float StackGap = 2.5f;
	constexpr float LabelSize = 6.5f;
	// The callback measures the actual renderer advances, including the ellipsis.
	template<size_t N, typename F>
	float FitLabel(char (&aOut)[N], const char *pName, float MaxWidth, F Measure)
	{
		str_utf8_copy_num(aOut, pName, N, N);
		float Width = Measure(aOut);
		if(Width <= MaxWidth)
			return Width;
		int End = str_length(aOut);
		do
		{
			End = str_utf8_rewind(aOut, End);
			aOut[End] = 0;
			str_append(aOut, "…");
			Width = Measure(aOut);
		} while(Width > MaxWidth && End > 0);
		if(Width > MaxWidth)
		{
			aOut[0] = 0;
			return 0;
		}
		return Width;
	}
	struct CRowPosition
	{
		float m_Y = 0, m_From = 0, m_To = 0, m_Time = 0;
		bool m_Placed = false;
		void Update(float Target, float Dt)
		{
			if(!m_Placed)
			{
				m_Y = m_From = m_To = Target;
				m_Placed = true;
			}
			if(Target != m_To)
			{
				m_From = m_Y;
				m_To = Target;
				m_Time = 0;
			}
			m_Time = std::clamp(m_Time + (std::isfinite(Dt) ? std::max(0.f, Dt) : 0.f) / LucentStyle::Motion::Selection, 0.f, 1.f);
			m_Y = m_From + (m_To - m_From) * (LucentStyle::Motion::CubicOut(m_Time));
		}
	};
	inline float BottomAbove(const CRect *pBounds, int Count, float Width, float Height, float GroupWidth, float Gap)
	{
		const float Left = (Width - GroupWidth) / 2;
		float Bottom = Height - Gap;
		for(int i = 0; i < Count; ++i)
			if(pBounds[i].w > 0 && pBounds[i].x < Width - Left && pBounds[i].x + pBounds[i].w > Left)
				Bottom = std::min(Bottom, pBounds[i].y - Gap);
		return Bottom;
	}
	struct CTarget
	{
		int m_Id = -1;
		float m_X = 0, m_Y = 0;
		bool m_Valid = false, m_Frozen = false;
	};
	inline bool Frozen(bool Known, int End, int Flags, int Tick, int LiveFlag)
	{
		return Known && (End == -1 || End > Tick || (Flags & LiveFlag) != 0);
	}
	inline int Select(const CTarget *pTargets, int Count, int Self, int Dummy, bool DummyMode, int Retained)
	{
		if(Self < 0 || Self >= Count || !pTargets[Self].m_Valid)
			return -1;
		int Best = -1;
		for(int i = 0; i < Count; ++i)
		{
			if(i == Self || !pTargets[i].m_Valid || (DummyMode ? i != Dummy : i == Dummy))
				continue;
			const auto &Target = pTargets[i];
			if(Target.m_Y <= pTargets[Self].m_Y || std::abs(Target.m_X - pTargets[Self].m_X) > (i == Retained ? LeaveX : EnterX))
				continue;
			if(Best < 0 || Target.m_Y < pTargets[Best].m_Y)
				Best = i;
		}
		return Best;
	}
	struct CLayer
	{
		int m_Id = -1;
		float m_Alpha = 0, m_Reveal = 0, m_Freeze = 0;
	};
	struct CMode
	{
		CLayer m_aLayers[2];
		int m_Current = 0;
		int Target() const { return m_aLayers[m_Current].m_Id; }
		void Update(int Id, bool FrozenState, float Dt)
		{
			if(!std::isfinite(Dt) || Dt < 0)
				Dt = 0;
			if(Id >= 0 && Id != Target())
			{
				// Reuse the outgoing layer when a target returns during its fade.
				m_Current = 1 - m_Current;
				if(m_aLayers[m_Current].m_Id != Id)
					m_aLayers[m_Current] = {Id, 0, 0, 0};
			}
			for(int i = 0; i < 2; ++i)
			{
				auto &Layer = m_aLayers[i];
				const bool Active = i == m_Current && Id >= 0;
				if(Active)
					Layer.m_Reveal = std::min(Layer.m_Reveal, 1.f - std::cbrt(1.f - Layer.m_Alpha));
				Layer.m_Reveal = std::min(1.f, Layer.m_Reveal + Dt / LucentStyle::Motion::Selection);
				Layer.m_Alpha = Active ? LucentStyle::Motion::CubicOut(Layer.m_Reveal) : std::max(0.f, Layer.m_Alpha - Dt / LucentStyle::Motion::TextAppear);
				const float Accent = Active && FrozenState ? 1.f : 0.f;
				Layer.m_Freeze += (Accent - Layer.m_Freeze) * (1.f - std::exp(-Dt / .08f));
				if(!Active && Layer.m_Alpha == 0)
					Layer = {};
			}
		}
	};
	struct CState
	{
		CMode m_aModes[2];
		CRowPosition m_aRows[2];
		float m_AnchorBottom = -1;
		int m_Controlled = -1;
	};
}
#endif
