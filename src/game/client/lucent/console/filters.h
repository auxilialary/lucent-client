#ifndef GAME_CLIENT_LUCENT_CONSOLE_FILTERS_H
#define GAME_CLIENT_LUCENT_CONSOLE_FILTERS_H

#include <game/client/lucent/console/log.h>

#include <cstdint>
#include <vector>

namespace LucentConsole
{
	class CFilters
	{
		uint32_t m_Categories = AllCategories;
		uint32_t m_Severities = AllSeverities;

	public:
		static constexpr uint32_t AllCategories = (1u << static_cast<unsigned>(EConsoleLogGroup::COUNT)) - 1;
		static constexpr uint32_t AllSeverities = (1u << 6) - 1;
		static constexpr unsigned SeverityIndex(int Severity) { return Severity >= 0 && Severity <= 4 ? Severity : 5; }
		bool Category(EConsoleLogGroup Group) const { return (m_Categories & (1u << static_cast<unsigned>(Group))) != 0; }
		bool Severity(int Level) const { return (m_Severities & (1u << SeverityIndex(Level))) != 0; }
		bool Allows(EConsoleLogGroup Group, int Level) const { return Category(Group) && Severity(Level); }
		void ToggleCategory(EConsoleLogGroup Group)
		{
			const uint32_t Bit = 1u << static_cast<unsigned>(Group);
			m_Categories = CategoriesUnrestricted() ? Bit : m_Categories ^ Bit;
		}
		void ToggleSeverity(int Level) { m_Severities ^= 1u << SeverityIndex(Level); }
		void ResetCategories() { m_Categories = AllCategories; }
		bool CategoriesUnrestricted() const { return m_Categories == AllCategories; }
		bool HasAdditionalCategories() const
		{
			return !CategoriesUnrestricted() && (Category(EConsoleLogGroup::CLIENT) || Category(EConsoleLogGroup::ASSETS) || Category(EConsoleLogGroup::RECORD) || Category(EConsoleLogGroup::EDITOR) || Category(EConsoleLogGroup::OTHER));
		}
	};

	// A cache over the existing ring, never an owning backlog. Invalidate BEFORE
	// recycling storage; no cached address survives a mutation of the ring.
	template<typename T>
	class CVisibleEntries
	{
		bool m_Dirty = true;
		std::vector<T *> m_vEntries;
		int m_Lines = 0;
		unsigned m_Rebuilds = 0;

	public:
		void Invalidate()
		{
			m_Dirty = true;
			m_vEntries.clear();
		}
		template<typename Ring, typename Predicate>
		void Update(Ring &Backlog, Predicate Visible)
		{
			if(!m_Dirty)
				return;
			m_Lines = 0;
			for(auto *pEntry = Backlog.First(); pEntry; pEntry = Backlog.Next(pEntry))
				if(Visible(*pEntry))
				{
					m_vEntries.push_back(pEntry);
					m_Lines += pEntry->m_LineCount;
				}
			m_Dirty = false;
			++m_Rebuilds;
		}
		const std::vector<T *> &Entries() const { return m_vEntries; }
		int Lines() const { return m_Lines; }
		unsigned Rebuilds() const { return m_Rebuilds; }
	};
}
#endif
