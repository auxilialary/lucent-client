#ifndef GAME_CLIENT_LUCENT_BROWSER_STATE_H
#define GAME_CLIENT_LUCENT_BROWSER_STATE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

// Retarget from the rendered position, so rapid tab changes never jump.
class CLucentBrowserIndicator
{
	float m_X = 0, m_Width = 0, m_FromX = 0, m_FromWidth = 0;
	float m_TargetX = 0, m_TargetWidth = 0, m_Time = 0;
	bool m_Initialized = false;

public:
	void Update(float X, float Width, float Dt, bool Animate)
	{
		if(!m_Initialized || !Animate)
		{
			m_X = m_FromX = m_TargetX = X;
			m_Width = m_FromWidth = m_TargetWidth = Width;
			m_Time = .22f;
			m_Initialized = true;
		}
		if(X != m_TargetX || Width != m_TargetWidth)
		{
			m_FromX = m_X;
			m_FromWidth = m_Width;
			m_TargetX = X;
			m_TargetWidth = Width;
			m_Time = 0;
		}
		m_Time = std::clamp(m_Time + std::max(0.f, Dt), 0.f, .22f);
		const float T = 1 - std::pow(1 - m_Time / .22f, 3.f);
		m_X = m_FromX + (m_TargetX - m_FromX) * T;
		m_Width = m_FromWidth + (m_TargetWidth - m_FromWidth) * T;
	}
	float X() const { return m_X; }
	float Width() const { return m_Width; }
};

// Fade through zero before exchanging content. Retargeting retains rendered opacity.
template<typename T>
struct CLucentBrowserContent
{
	T m_Target{}, m_Visible{};
	float m_Alpha = 1;
	void Update(float Dt, bool Animate, float Duration)
	{
		if(!Animate)
		{
			m_Visible = m_Target;
			m_Alpha = 1;
			return;
		}
		const float Step = std::max(0.f, Dt) * 2 / Duration;
		m_Alpha = std::clamp(m_Alpha + (m_Target == m_Visible ? Step : -Step), 0.f, 1.f);
		if(m_Alpha == 0)
			m_Visible = m_Target;
	}
	float Opacity() const { return m_Alpha * m_Alpha * (3 - 2 * m_Alpha); }
};

class CLucentBrowserState
{
public:
	enum class EPrimary
	{
		SERVERS,
		FRIENDS,
		COMMUNITIES
	};
	enum class EScene
	{
		PREVIEW,
		DETAILS,
		PLAYERS
	};
	enum class EOverlay
	{
		NONE,
		FILTERS,
		DIRECT_CONNECT
	};
	// Render/action identifiers, never a shared navigation state.
	enum class EPanel
	{
		NONE,
		FILTERS,
		DETAILS,
		PLAYERS,
		FRIENDS,
		COMMUNITIES,
		DIRECT_CONNECT
	};
	CLucentBrowserContent<EPrimary> m_Primary;
	CLucentBrowserContent<EScene> m_Scene;
	CLucentBrowserContent<std::string> m_MapTitle;
	CLucentBrowserContent<EOverlay> m_Overlay;
	CLucentBrowserIndicator m_Navigation, m_DetailNavigation;
	float m_Expansion = 0;
	float m_FiltersExpansion = 0;
	float m_ScenePosition = 0;
	std::string m_LastSelection;
	std::string m_ServerSelection;
	EPrimary m_SelectionPrimary = EPrimary::SERVERS;
	float m_SelectionY = 0;
	const char *PrimarySelection(const char *pAddress)
	{
		if(m_SelectionPrimary == m_Primary.m_Target)
			return nullptr;
		if(m_SelectionPrimary == EPrimary::SERVERS)
			m_ServerSelection = pAddress;
		m_SelectionPrimary = m_Primary.m_Target;
		return m_SelectionPrimary == EPrimary::SERVERS ? m_ServerSelection.c_str() : "";
	}
	bool FiltersWorkspace() const
	{
		return m_Overlay.m_Target == EOverlay::FILTERS || m_FiltersExpansion > 0;
	}
	float BrowserOpacity() const { return FiltersWorkspace() ? std::max(0.f, 1 - 2 * FiltersExpansion()) : 1.f; }
	float FiltersOpacity() const { return std::max(0.f, 2 * FiltersExpansion() - 1); }
	float m_SelectionPulse = 0;
	bool m_DirectFocusPending = false;
	int m_Focus = 0;
	void Tab(bool Reverse) { m_Focus = (m_Focus + (Reverse ? 6 : 1)) % 7; }
	void Select(std::string_view Address, bool Animate)
	{
		if(Address != m_LastSelection)
		{
			m_LastSelection.assign(Address);
			m_SelectionPulse = Animate && !Address.empty() ? 1.f : 0.f;
		}
		if(!Animate)
			m_SelectionPulse = 0;
	}
	void Servers()
	{
		m_Primary.m_Target = EPrimary::SERVERS;
		m_Overlay.m_Target = EOverlay::NONE;
	}
	void Toggle(EPanel Action)
	{
		switch(Action)
		{
		case EPanel::FRIENDS:
		case EPanel::COMMUNITIES:
		{
			const auto Next = Action == EPanel::FRIENDS ? EPrimary::FRIENDS : EPrimary::COMMUNITIES;
			m_Primary.m_Target = m_Primary.m_Target == Next ? EPrimary::SERVERS : Next;
			m_Overlay.m_Target = EOverlay::NONE;
			break;
		}
		case EPanel::DETAILS:
		case EPanel::PLAYERS:
		{
			const auto Next = Action == EPanel::DETAILS ? EScene::DETAILS : EScene::PLAYERS;
			m_Scene.m_Target = m_Scene.m_Target == Next ? EScene::PREVIEW : Next;
			break;
		}
		case EPanel::FILTERS:
		case EPanel::DIRECT_CONNECT:
		{
			const auto Next = Action == EPanel::FILTERS ? EOverlay::FILTERS : EOverlay::DIRECT_CONNECT;
			m_Overlay.m_Target = m_Overlay.m_Target == Next ? EOverlay::NONE : Next;
			break;
		}
		case EPanel::NONE: break;
		}
	}
	bool Escape()
	{
		if(m_Overlay.m_Target != EOverlay::NONE)
			m_Overlay.m_Target = EOverlay::NONE;
		else if(m_Scene.m_Target != EScene::PREVIEW)
			m_Scene.m_Target = EScene::PREVIEW;
		else if(m_Primary.m_Target != EPrimary::SERVERS)
			m_Primary.m_Target = EPrimary::SERVERS;
		else
			return false;
		return true;
	}
	bool HasLocalState() const
	{
		return m_Overlay.m_Target != EOverlay::NONE || m_Scene.m_Target != EScene::PREVIEW || m_Primary.m_Target != EPrimary::SERVERS;
	}
	void Update(float Dt, bool Animate)
	{
		const float Step = std::clamp(Dt, 0.f, .1f);
		m_Primary.Update(Step, Animate, .30f);
		m_Scene.Update(Step, Animate, .36f);
		if(m_Overlay.m_Visible == EOverlay::NONE && m_Overlay.m_Target != EOverlay::NONE)
			m_Overlay.m_Visible = m_Overlay.m_Target;
		if(m_Overlay.m_Target != EOverlay::NONE)
			m_Overlay.Update(Step, Animate, .32f);
		const auto Move = [&](float Value, bool Target, float Duration) {
			return Animate ? std::clamp(Value + (Target ? Step : -Step) / Duration, 0.f, 1.f) : float(Target);
		};
		m_FiltersExpansion = Move(m_FiltersExpansion, m_Overlay.m_Target == EOverlay::FILTERS, .38f);
		m_Expansion = Move(m_Expansion, m_Overlay.m_Target != EOverlay::NONE, .38f);
		if(m_Expansion == 0 && m_Overlay.m_Target == EOverlay::NONE)
		{
			m_Overlay.m_Visible = EOverlay::NONE;
			m_Overlay.m_Alpha = 1;
		}
		m_ScenePosition = Move(m_ScenePosition, m_Scene.m_Target != EScene::PREVIEW, .36f);
		m_SelectionPulse = Animate ? std::max(0.f, m_SelectionPulse - Step / .36f) : 0;
	}
	float FiltersExpansion() const { return m_FiltersExpansion * m_FiltersExpansion * (3 - 2 * m_FiltersExpansion); }
	float Expansion() const { return m_Expansion * m_Expansion * (3 - 2 * m_Expansion); }
	float ScenePosition() const { return m_ScenePosition * m_ScenePosition * (3 - 2 * m_ScenePosition); }
	template<typename T>
	static int FindSelection(int Count, std::string_view Address, T GetAddress)
	{
		for(int i = 0; i < Count; ++i)
			if(Address == GetAddress(i))
				return i;
		return -1;
	}
	// Prefer first-party community metadata, then a canonical DDNet identity.
	// Unknown identities remain intact and are clipped by the presentation layer.
	static std::string_view Descriptor(std::string_view Name, std::string_view VerifiedType = {})
	{
		if(!VerifiedType.empty() && VerifiedType != "None")
			return VerifiedType;
		constexpr std::string_view Prefix = "DDNet ";
		if(Name.substr(0, Prefix.size()) != Prefix)
			return Name;
		const auto Separator = Name.find(" - ", Prefix.size());
		if(Separator == std::string_view::npos)
			return Name;
		const auto Region = Name.substr(Prefix.size(), Separator - Prefix.size());
		if(Region.size() < 2 || Region.size() > 3 || !std::all_of(Region.begin(), Region.end(), [](char C) { return C >= 'A' && C <= 'Z'; }))
			return Name;
		const auto Mode = Name.substr(Separator + 3);
		for(const auto Known : {"Novice", "Moderate", "Brutal", "Insane", "Dummy", "Solo", "Oldschool", "DDmaX", "Race", "Fun"})
			if(Mode == Known)
				return Mode;
		return Name;
	}
	static void Sort(int Column, int &Sort, int &Order, bool ThreeOrders)
	{
		Order = Sort == Column ? (Order + 1) % (ThreeOrders ? 3 : 2) : 0;
		Sort = Column;
	}
};

// Independent of HTTP and graphics: only the current generation may be published.
class CLucentPreviewSelection
{
	std::string m_Key;
	double m_Changed = 0;
	uint64_t m_Generation = 0;
	bool m_Started = false;

public:
	bool Select(std::string_view Key, double Now)
	{
		if(Key == m_Key)
			return false;
		m_Key.assign(Key);
		m_Changed = Now;
		m_Started = false;
		++m_Generation;
		return true;
	}
	bool Ready(double Now) const { return !m_Started && !m_Key.empty() && Now - m_Changed >= .22; }
	void Start() { m_Started = true; }
	uint64_t Generation() const { return m_Generation; }
	bool Accept(uint64_t Generation) const { return Generation == m_Generation; }
	const std::string &Key() const { return m_Key; }
};
#endif
