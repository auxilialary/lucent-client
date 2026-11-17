#ifndef GAME_CLIENT_LUCENT_GAME_CHAT_COMPONENT_H
#define GAME_CLIENT_LUCENT_GAME_CHAT_COMPONENT_H

#include <game/client/component.h>
#include <game/client/lucent/hud/tee_alignment.h>
#include <game/client/render.h>

#include <deque>
#include <memory>
#include <string>

class CLucentChat : public CComponent
{
	struct CMention
	{
		std::string m_Name, m_Text;
		double m_Start = -1;
		std::shared_ptr<CManagedTeeRenderInfo> m_pTee;
	};
	std::deque<CMention> m_Queue;
	LucentHud::Alignment::CRowPosition m_Width;
	double m_LastRender = -1;
	LucentHud::Alignment::CRowPosition m_PreviewWidth;
	double m_LastPreview = -1;
	void RenderCards(const std::deque<CMention> &Messages, LucentHud::Alignment::CRowPosition &WidthState, double &LastRender, double Now, float CenterX, float Top, float MaxWidth, bool Preview);

public:
	int Sizeof() const override { return sizeof(*this); }
	bool Allow(int ClientId, const char *pText) const;
	void Mention(int ClientId, const char *pText, bool Highlighted, const std::shared_ptr<CManagedTeeRenderInfo> &pTee);
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override { OnReset(); }
	void OnRender() override;
	void RenderPreview(CUIRect View, double Elapsed);
	void ResetPreview()
	{
		m_PreviewWidth = {};
		m_LastPreview = -1;
	}
};
#endif
