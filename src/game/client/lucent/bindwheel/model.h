#ifndef GAME_CLIENT_LUCENT_BINDWHEEL_MODEL_H
#define GAME_CLIENT_LUCENT_BINDWHEEL_MODEL_H

#include <base/vmath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace LucentBindWheel
{
	constexpr int MaxBinds = 64, MaxName = 63, MaxCommand = 1023;
	struct CBind
	{
		std::string m_Name, m_Command;
		bool operator==(const CBind &Other) const { return m_Name == Other.m_Name && m_Command == Other.m_Command; }
	};
	using CPage = std::vector<CBind>;
	inline bool Reorder(CPage &Page, int From, int To)
	{
		if(From < 0 || To < 0 || From >= int(Page.size()) || To >= int(Page.size()) || From == To)
			return false;
		if(From < To)
			std::rotate(Page.begin() + From, Page.begin() + From + 1, Page.begin() + To + 1);
		else
			std::rotate(Page.begin() + To, Page.begin() + From, Page.begin() + From + 1);
		return true;
	}
	using CPages = std::array<CPage, 2>;
	inline bool CanTransfer(const CPages &Pages, int FromPage, int From, int ToPage, int To)
	{
		return FromPage >= 0 && FromPage < 2 && ToPage >= 0 && ToPage < 2 && FromPage != ToPage &&
		       From >= 0 && From < int(Pages[FromPage].size()) && To >= 0 && To <= int(Pages[ToPage].size()) &&
		       Pages[ToPage].size() < MaxBinds;
	}
	inline bool Transfer(CPages &Pages, int FromPage, int From, int ToPage, int To)
	{
		if(!CanTransfer(Pages, FromPage, From, ToPage, To))
			return false;
		Pages[ToPage].insert(Pages[ToPage].begin() + To, Pages[FromPage][From]);
		Pages[FromPage].erase(Pages[FromPage].begin() + From);
		return true;
	}
	class CEditHistory
	{
		std::vector<CPages> m_Undo;

	public:
		void Clear() { m_Undo.clear(); }
		void Remember(const CPages &Pages)
		{
			if(!m_Undo.empty() && m_Undo.back() == Pages)
				return;
			if(m_Undo.size() == 32)
				m_Undo.erase(m_Undo.begin());
			m_Undo.push_back(Pages);
		}
		bool Undo(CPages &Pages)
		{
			if(m_Undo.empty())
				return false;
			Pages = std::move(m_Undo.back());
			m_Undo.pop_back();
			return true;
		}
	};
	inline bool Valid(const CBind &Bind)
	{
		return Bind.m_Name.size() <= MaxName && Bind.m_Command.size() <= MaxCommand &&
		       Bind.m_Name.find_first_of("\r\n") == std::string::npos && Bind.m_Command.find_first_of("\r\n") == std::string::npos;
	}
	inline std::string Quote(const std::string &Text)
	{
		std::string Result = "\"";
		for(char Ch : Text)
		{
			if(Ch == '\\' || Ch == '"')
				Result += '\\';
			Result += Ch;
		}
		return Result + '"';
	}
	inline std::string SaveLine(int Page, const CBind &Bind)
	{
		return "lucent_add_bindwheel " + std::to_string(Page) + " " + Quote(Bind.m_Name) + " " + Quote(Bind.m_Command);
	}
	// DDNet-style tokens. The remainder argument keeps unquoted command text.
	inline bool Token(const std::string &Text, size_t &Pos, std::string &Out, bool Remainder = false)
	{
		Out.clear();
		while(Pos < Text.size() && (Text[Pos] == ' ' || Text[Pos] == '\t' || Text[Pos] == '\r'))
			++Pos;
		if(Pos == Text.size() || Text[Pos] == '#')
			return false;
		const bool Quoted = Text[Pos] == '"';
		if(Quoted)
			++Pos;
		while(Pos < Text.size())
		{
			char Ch = Text[Pos++];
			if(Quoted && Ch == '"')
				return true;
			if(Quoted && Ch == '\\' && Pos < Text.size() && (Text[Pos] == '\\' || Text[Pos] == '"'))
				Ch = Text[Pos++];
			else if(!Quoted && (!Remainder && (Ch == ' ' || Ch == '\t' || Ch == '\r')))
				return true;
			else if(!Quoted && Ch == '#')
				break;
			Out += Ch;
		}
		while(!Out.empty() && (Out.back() == '\r' || Out.back() == ' ' || Out.back() == '\t'))
			Out.pop_back();
		return !Quoted;
	}
	inline bool End(const std::string &Text, size_t Pos)
	{
		while(Pos < Text.size() && (Text[Pos] == ' ' || Text[Pos] == '\t' || Text[Pos] == '\r'))
			++Pos;
		return Pos == Text.size() || Text[Pos] == '#';
	}
	inline bool Number(const std::string &Text, int &Value)
	{
		if(Text.empty() || Text.size() > 5)
			return false;
		Value = 0;
		for(char Ch : Text)
		{
			if(Ch < '0' || Ch > '9')
				return false;
			Value = Value * 10 + Ch - '0';
		}
		return true;
	}
	struct CImport
	{
		CPage m_Binds;
		std::vector<std::string> m_Keys;
		int m_ResetMouse = -1, m_Animation = -1, m_Scale = -1;
		int m_Errors = 0;
	};
	inline void ParseRecord(const std::string &Line, CImport &Out)
	{
		size_t Pos = 0;
		std::string Command, A, B, C;
		if(!Token(Line, Pos, Command))
			return;
		if(Command == "add_bindwheel" || Command == "bindwheel" || Command == "remove_bindwheel")
		{
			int Index = -1;
			if(Command == "bindwheel" && (!Token(Line, Pos, C) || !Number(C, Index) || Index >= MaxBinds))
			{
				++Out.m_Errors;
				return;
			}
			if(!Token(Line, Pos, A) || !Token(Line, Pos, B, true) || !End(Line, Pos) || !Valid({A, B}))
			{
				++Out.m_Errors;
				return;
			}
			if(Index >= 0)
			{
				if(int(Out.m_Binds.size()) <= Index)
					Out.m_Binds.resize(Index + 1, {"*", ""});
				Out.m_Binds[Index] = {A, B};
			}
			else if(Command == "remove_bindwheel")
			{
				auto It = std::find(Out.m_Binds.begin(), Out.m_Binds.end(), CBind{A, B});
				if(It != Out.m_Binds.end())
					Out.m_Binds.erase(It);
			}
			else if(Out.m_Binds.size() < MaxBinds)
				Out.m_Binds.push_back({A, B});
			else
				++Out.m_Errors;
		}
		else if(Command == "delete_all_bindwheel_binds" && End(Line, Pos))
			Out.m_Binds.clear();
		else if(Command == "bind" && Token(Line, Pos, A) && Token(Line, Pos, B) && End(Line, Pos) && B == "+bindwheel")
			Out.m_Keys.push_back(A);
		else if(Command == "ec_reset_bindwheel_mouse" || Command == "tc_reset_bindwheel_mouse" || Command == "ec_animate_wheel_time" || Command == "tc_animate_wheel_time" || Command == "bc_wheel_scale")
		{
			int Value;
			if(!Token(Line, Pos, A) || !Number(A, Value) || !End(Line, Pos))
			{
				++Out.m_Errors;
				return;
			}
			if(Command == "bc_wheel_scale")
				Out.m_Scale = std::clamp(Value, 50, 100);
			else if(Command.find("reset") != std::string::npos)
				Out.m_ResetMouse = std::clamp(Value, 0, 1);
			else
				Out.m_Animation = std::clamp(Value, 0, 1000);
		}
	}
	inline void Parse(const std::string &Text, CImport &Out)
	{
		// Split only outside quoted strings; payloads are data, never executed.
		bool Quoted = false, Escaped = false, Comment = false;
		size_t Start = Text.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
		for(size_t i = Start; i <= Text.size(); ++i)
		{
			const char Ch = i < Text.size() ? Text[i] : '\n';
			if(Ch == '\n' || (!Quoted && !Comment && Ch == ';'))
			{
				ParseRecord(Text.substr(Start, i - Start), Out);
				Start = i + 1;
				Quoted = Escaped = Comment = false;
			}
			else if(!Comment)
			{
				if(Escaped)
					Escaped = false;
				else if(Quoted && Ch == '\\')
					Escaped = true;
				else if(Ch == '"')
					Quoted = !Quoted;
				else if(!Quoted && Ch == '#')
					Comment = true;
			}
		}
	}
	inline int Select(vec2 Cursor, int Count, float DeadZone)
	{
		if(Count <= 0 || length(Cursor) <= DeadZone)
			return -1;
		const int Index = int(std::round(angle(Cursor) / (2 * pi) * Count));
		return (Index % Count + Count) % Count;
	}
	// Execution is taken out before calling the console, allowing self-modifying binds.
	class CSession
	{
		bool m_Active = false;
		bool m_Executed = false;
		bool m_StrokeHeld = false;

	public:
		int m_Page = 0;
		vec2 m_Cursor = vec2(0, 0);
		bool Active() const { return m_Active; }
		bool Press(bool NewBind)
		{
			if(m_StrokeHeld && !NewBind)
				return false;
			m_StrokeHeld = true;
			return true;
		}
		void ReleaseStroke() { m_StrokeHeld = false; }
		void Open(bool Shift, bool Reset)
		{
			if(m_Active)
				return;
			m_Active = true;
			m_Executed = false;
			m_Page = Shift ? 1 : 0;
			if(Reset)
				m_Cursor = vec2(0, 0);
		}
		void Cancel()
		{
			m_Active = false;
			m_Executed = true;
		}
		void Shift(bool Held)
		{
			if(m_Active)
				m_Page = Held ? 1 : 0;
		}
		std::string Execute(const std::array<CPage, 2> &Pages, float DeadZone, bool Close)
		{
			if(!m_Active)
				return {};
			if(Close)
				m_Active = false;
			if(m_Executed)
				return {};
			const int Selected = Select(m_Cursor, int(Pages[m_Page].size()), DeadZone);
			if(Selected < 0)
				return {};
			m_Executed = true;
			return Pages[m_Page][Selected].m_Command;
		}
	};
}
#endif
