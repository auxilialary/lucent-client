#ifndef GAME_CLIENT_LUCENT_BINDWHEEL_COMPONENT_H
#define GAME_CLIENT_LUCENT_BINDWHEEL_COMPONENT_H

#include "import.h"
#include "presentation.h"

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/lucent/motion/transition.h>
#include <game/client/ui.h>

class IConfigManager;
class CLucentBindWheel : public CComponent
{
	LucentBindWheel::CPages m_Pages;
	LucentBindWheel::CEditHistory m_History;
	LucentBindWheel::CSession m_Session;
	LucentStyle::Motion::CTransition m_Reveal;
	LucentBindWheel::CFlipPresentation m_Flip;
	CUi::CTouchState m_Touch;
	bool m_LeftShift = false, m_RightShift = false;
	int m_DrawPage = 0, m_DrawSelected = -1;
	std::vector<LucentBindWheel::CSource> m_Sources;
	std::string m_Status;
	bool Allowed() const;
	void Execute(bool Close);
	void UpdateShift();
	static void ConOpen(IConsole::IResult *pResult, void *pUser);
	static void ConHover(IConsole::IResult *pResult, void *pUser);
	static void ConAdd(IConsole::IResult *pResult, void *pUser);
	static void ConLegacy(IConsole::IResult *pResult, void *pUser);
	static void ConPageAdd(IConsole::IResult *pResult, void *pUser);
	static void ConRemove(IConsole::IResult *pResult, void *pUser);
	static void ConClear(IConsole::IResult *pResult, void *pUser);
	static void Save(IConfigManager *pConfig, void *pUser);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnReset() override;
	void OnRelease() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	bool OnCursorMove(float X, float Y, IInput::ECursorType Type) override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool IsActive() const { return m_Session.Active(); }
	const LucentBindWheel::CPage &Page(int Index) const { return m_Pages[std::clamp(Index, 0, 1)]; }
	bool Set(int Page, int Index, const LucentBindWheel::CBind &Bind);
	void Remove(int Page, int Index);
	void Move(int Page, int Index, int Direction);
	void Reorder(int Page, int From, int To);
	bool Transfer(int FromPage, int From, int ToPage, int To);
	bool Undo();
	void RefreshSources();
	bool Import(const LucentBindWheel::CSource &Source, int Page, bool Settings, bool Keys);
	const auto &Sources() const { return m_Sources; }
	const std::string &Status() const { return m_Status; }
	void Draw(CUIRect Rect, int Page, int Selected, float Opacity = 1, float WidthScale = 1, bool Preview = false, int Hovered = -1);
};
#endif
