#ifndef GAME_CLIENT_LUCENT_LOCALIZATION_H
#define GAME_CLIENT_LUCENT_LOCALIZATION_H
#include <game/localization.h>
// Dedicated Lucent context in the selected client language.
[[gnu::format_arg(1)]] inline const char *LCNTLocalize(const char *pText)
{
	const char *pTranslated = Localize(pText, "Lucent");
	return pTranslated == pText ? Localize(pText) : pTranslated;
}
#endif
