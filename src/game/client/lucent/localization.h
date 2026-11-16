#ifndef GAME_CLIENT_LUCENT_LOCALIZATION_H
#define GAME_CLIENT_LUCENT_LOCALIZATION_H
#include <game/localization.h>
// Dedicated client catalog: English by default, Russian when selected.
[[gnu::format_arg(1)]] inline const char *LCNTLocalize(const char *pText)
{
	return Localize(pText, "Lucent");
}
#endif
