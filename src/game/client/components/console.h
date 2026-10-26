/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CONSOLE_H
#define GAME_CLIENT_COMPONENTS_CONSOLE_H

#include <base/lock.h>

#include <engine/console.h>
#include <engine/shared/ringbuffer.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/lucent/console/copy_feedback.h>
#include <game/client/lucent/console/filter_ui.h>
#include <game/client/lucent/console/presentation.h>
#include <game/client/lucent/console/selection.h>
#include <game/client/lucent/lucent_style.h>
#include <game/client/ui.h>

#include <array>

enum
{
	CONSOLE_CLOSED,
	CONSOLE_OPENING,
	CONSOLE_OPEN,
	CONSOLE_CLOSING,
};

class CConsoleLogger;

class CGameConsole : public CComponent
{
	friend class CConsoleLogger;
	static constexpr float FONT_SIZE = 10.0f;
	static constexpr float LINE_SPACING = 1.0f;

	class CInstance
	{
	public:
		struct CBacklogEntry
		{
			uint64_t m_Id;
			int m_MessageOffset;
			int m_MessageCharacterOffset;
			int m_SourceCharacterOffset;
			int m_Characters;
			int m_Severity;
			EConsoleLogGroup m_Group;
			char m_aTimestamp[80];
			char m_aSource[32];
			float m_MessageX;
			float m_TimestampWidth;
			float m_SourceWidth;
			float m_YOffset;
			int m_LineCount;
			ColorRGBA m_PrintColor;
			size_t m_Length;
			char m_aText[1];
			const char *Message() const { return m_aText + m_MessageOffset; }
		};
		CStaticRingBuffer<CBacklogEntry, 1024 * 1024, CRingBufferBase::FLAG_RECYCLE> m_Backlog;
		LucentConsole::CFilters m_Filters;
		LucentConsole::CFilterPanel m_FilterPanel;
		LucentConsole::CVisibleEntries<CBacklogEntry> m_VisibleEntries;
		LucentConsole::CMetadataTooltip m_MetadataTooltip;
		void EnsureVisibleEntries();
		void ApplyFilters();
		CLock m_BacklogPendingLock;
		CStaticRingBuffer<CBacklogEntry, 1024 * 1024, CRingBufferBase::FLAG_RECYCLE> m_BacklogPending GUARDED_BY(m_BacklogPendingLock);
		CStaticRingBuffer<char, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
		char *m_pHistoryEntry;

		CLineInputBuffered<IConsole::CMDLINE_LENGTH> m_Input;
		const char *m_pName;
		int m_Type;
		int m_BacklogCurLine;
		int m_BacklogLastActiveLine = -1;
		int m_LinesRendered;

		STextBoundingBox m_BoundingBox = {0.0f, 0.0f, 0.0f, 0.0f};
		float m_LastInputHeight = 0.0f;

		bool m_MouseIsPress = false;
		bool m_PointerTouch = false;
		vec2 m_MousePress = vec2(0.0f, 0.0f);
		vec2 m_MouseRelease = vec2(0.0f, 0.0f);
		bool m_HasSelection = false;
		CLucentConsoleSelection m_Selection;
		uint64_t m_NextEntryId = 1; // Assigned while holding the pending lock.
		bool m_Selecting = false;
		CUIRect m_CopyRect = {};
		bool m_CopyVisible = false;
		LucentStyle::Motion::CTransition m_CopyFade;
		LucentConsole::CCopyFeedback m_CopyFeedback;
		int m_NewLineCounter = 0;

		CGameConsole *m_pGameConsole;

		char m_aCompletionBuffer[IConsole::CMDLINE_LENGTH];
		int m_CompletionChosen;
		char m_aCompletionBufferArgument[IConsole::CMDLINE_LENGTH];
		int m_CompletionChosenArgument;
		int m_CompletionFlagmask;
		float m_CompletionRenderOffset;
		float m_CompletionRenderOffsetChange;
		int m_CompletionArgumentPosition;
		int m_CompletionCommandStart = 0;
		int m_CompletionCommandEnd = 0;

		char m_aUser[64];
		bool m_UserGot;
		bool m_UsernameReq;

		bool m_IsCommand;
		const char *m_pCommandName;
		const char *m_pCommandHelp;
		const char *m_pCommandParams;

		bool m_CompletionDirty;
		bool m_QueueResetAnimation;
		std::vector<const char *> m_vpCommandSuggestions;
		std::vector<const char *> m_vpArgumentSuggestions;

		bool m_Searching = false;
		struct SSearchMatch
		{
			int m_Pos;
			int m_StartLine;
			int m_EndLine;
			int m_EntryLine;

			SSearchMatch(int Pos, int StartLine, int EndLine, int EntryLine) :
				m_Pos(Pos), m_StartLine(StartLine), m_EndLine(EndLine), m_EntryLine(EntryLine) {}
		};
		int m_CurrentMatchIndex;
		char m_aCurrentSearchString[IConsole::CMDLINE_LENGTH];
		std::vector<SSearchMatch> m_vSearchMatches;

		CInstance(int t);
		void Init(CGameConsole *pGameConsole);

		void ClearBacklog() REQUIRES(!m_BacklogPendingLock);
		void UpdateBacklogTextAttributes();
		void PumpBacklogPending() REQUIRES(!m_BacklogPendingLock);
		void ClearHistory();
		void Reset();

		void ExecuteLine(const char *pLine);

		bool OnInput(const IInput::CEvent &Event);
		void PrintLine(const char *pLine, int Len, ColorRGBA PrintColor, const class CLogMessage *pMessage = nullptr) REQUIRES(!m_BacklogPendingLock);
		void CopySelection(bool VisibleAnchor = true);
		int GetLinesToScroll(int Direction, int LinesToScroll);
		void ScrollToCenter(int StartLine, int EndLine);
		void Dump() REQUIRES(!m_BacklogPendingLock);

		const char *GetString() const { return m_Input.GetString(); }
		/**
		 * Gets the command at the current cursor including trailing spaces.
		 * Commands are split by semicolons.
		 *
		 * So if the current console input is for example "hello; world ;foo"
		 *                                                        ^
		 *                   and the cursor is here  -------------/
		 * The result would be "world "
		 *
		 * @param pInput the console input line
		 * @param aCmd the command the cursor is at
		 */
		void GetCommand(const char *pInput, char (&aCmd)[IConsole::CMDLINE_LENGTH]);

		void UpdateEntryTextAttributes(CBacklogEntry *pEntry) const;

		bool IsInputHidden() const;
		void UpdateCompletionSuggestions();

	private:
		void MeasureLogMetadata(CBacklogEntry *pEntry, float Width) const;
		void SetSearching(bool Searching);
		void ClearSearch();
		void UpdateSearch();

		friend class CGameConsole;
	};

	class IConsole *m_pConsole;
	CConsoleLogger *m_pConsoleLogger = nullptr;

	CInstance m_LocalConsole;
	CInstance m_RemoteConsole;

	CInstance *ConsoleForType(int ConsoleType);
	CInstance *CurrentConsole();

	int m_ConsoleType;
	int m_ConsoleState;
	float m_StateChangeEnd;
	float m_StateChangeDuration;

	bool m_WantsSelectionCopy = false;
	CUi::CTouchState m_TouchState;

	vec2 m_ButtonPressPosition = vec2(0.0f, 0.0f);
	bool m_ButtonPressed = false;

	bool DoButton(const CUIRect &Rect, const char *pIcon, vec2 MousePosition, bool Released);
	// Per-frame geometry for the existing copy overlay; the range model stays in CInstance.
	struct SSelectionOverlay
	{
		bool m_Visible = false;
		LucentConsole::COverlayPosition m_Fragment{};
		std::array<LucentConsole::COverlayPosition, 128> m_aOccupied{};
		size_t m_OccupiedCount = 0;
	};

	void RenderLogMetadata(const CInstance::CBacklogEntry *pEntry, float Y, int SearchMatch, vec2 Mouse);
	void RenderSelectionCopy(CInstance *pConsole, bool Visible, const LucentConsole::COverlayPosition &Fragment, std::span<const LucentConsole::COverlayPosition> Occupied, vec2 Mouse, bool Released, const CUIRect &Bounds);

	void PrepareLogMetadataHit(const CInstance::CBacklogEntry *pEntry, vec2 Mouse, float EntryTop, CTextCursor &Hit, const char *&pHitText, int &CharacterOffset, int &HitCharacters);
	void MeasureLogSelectionFragments(const CInstance::CBacklogEntry *pEntry, const CTextCursor &EntryCursor, int SelectionStart, int SelectionStop, int BodyStart, int BodyStop, float EntryTop, float LineHeight, float ClipTop, float BottomY, float Width, SSelectionOverlay &Overlay);

	static constexpr ColorRGBA ms_SearchHighlightColor = ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f);
	static constexpr ColorRGBA ms_SearchSelectedColor = ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f);

	int PossibleMaps(const char *pStr, IConsole::FPossibleCallback pfnCallback = IConsole::EmptyPossibleCommandCallback, void *pUser = nullptr);
	void PossibleSettings(const char *pStr, int FlagMask, IConsole::FPossibleCallback pfnCallback, void *pUser);

	static void PossibleCommandsRenderCallback(int Index, const char *pStr, void *pUser);
	static void ConToggleLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConClearRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpLocalConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpRemoteConsole(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageUp(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageDown(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageTop(IConsole::IResult *pResult, void *pUserData);
	static void ConConsolePageBottom(IConsole::IResult *pResult, void *pUserData);
	static void ConchainConsoleOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

public:
	enum
	{
		CONSOLETYPE_LOCAL = 0,
		CONSOLETYPE_REMOTE,
		NUM_CONSOLETYPES
	};

	CGameConsole();
	~CGameConsole() override;
	int Sizeof() const override { return sizeof(*this); }

	void PrintLine(int Type, const char *pLine);
	void RequireUsername(bool UsernameReq);

	void OnStateChange(int NewState, int OldState) override;
	void OnConsoleInit() override;
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void Prompt(char (&aPrompt)[32]);

	void Toggle(int Type);
	bool IsActive() const { return m_ConsoleState != CONSOLE_CLOSED; }

	void ForceUpdateRemoteCompletionSuggestions();
};
#endif
