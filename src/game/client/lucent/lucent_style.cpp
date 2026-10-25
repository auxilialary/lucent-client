#include "lucent_style.h"

LucentStyle::Motion::CClock &LucentStyle::Motion::Clock()
{
	static CClock s_Clock;
	return s_Clock;
}
