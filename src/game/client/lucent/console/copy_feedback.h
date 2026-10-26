#ifndef GAME_CLIENT_LUCENT_CONSOLE_COPY_FEEDBACK_H
#define GAME_CLIENT_LUCENT_CONSOLE_COPY_FEEDBACK_H

#include <game/client/lucent/style/tokens.h>

namespace LucentConsole
{
	// Presentation state only. The existing selection remains the sole range model.
	class CCopyFeedback
	{
		bool m_Copied = false;
		bool m_HasAnchor = false;
		double m_Until = 0;

	public:
		void BeginSelection()
		{
			m_Copied = false;
			m_HasAnchor = false;
			m_Until = 0;
		}
		void Copied(double Now, bool VisibleAnchor = true)
		{
			m_Copied = true;
			m_HasAnchor = VisibleAnchor;
			m_Until = Now + LucentStyle::Console::CopiedDuration;
		}
		bool IsCopied() const { return m_Copied; }
		void HideAnchor() { m_HasAnchor = false; }
		bool Visible(double Now, bool Selecting, bool SelectionVisible, bool HasSelection) const
		{
			return !Selecting && (m_Copied ? m_HasAnchor && Now < m_Until : SelectionVisible && HasSelection);
		}
	};
}
#endif
