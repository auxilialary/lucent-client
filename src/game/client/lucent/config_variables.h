MACRO_CONFIG_INT(ClLucentLogoAnimation, cl_lucent_logo_animation, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate the Lucent start menu (0 for static)")

MACRO_CONFIG_INT(ClLucentSkinTextureLimit, cl_lucent_skin_texture_limit, 1024, 128, 8192, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum skin texture edge in pixels; larger images are reduced before GPU upload")

#include "gameview/config_variables.h"

#include "hud/config_variables.h"

MACRO_CONFIG_INT(ClLucentSettingsWorkspace, cl_lucent_settings_workspace, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Settings workspace: 0 DDNet, 1 Lucent")
MACRO_CONFIG_INT(ClLucentSettingsPage, cl_lucent_settings_page, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Last Lucent settings section")

MACRO_CONFIG_STR(ClLucentFont, cl_lucent_font, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lucent custom font filename; empty uses standard DDNet")
