MACRO_CONFIG_INT(ClLucentLogoAnimation, cl_lucent_logo_animation, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Animate the Lucent start menu (0 for static)")

MACRO_CONFIG_INT(ClLucentSkinTextureLimit, cl_lucent_skin_texture_limit, 1024, 128, 8192, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum skin texture edge in pixels; larger images are reduced before GPU upload")

#include "gameview/config_variables.h"

#include "hud/config_variables.h"

MACRO_CONFIG_INT(ClLucentSettingsWorkspace, cl_lucent_settings_workspace, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Settings workspace: 0 DDNet, 1 Lucent")
MACRO_CONFIG_INT(ClLucentSettingsPage, cl_lucent_settings_page, 0, 0, 7, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Last Lucent settings section")

MACRO_CONFIG_STR(ClLucentFont, cl_lucent_font, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lucent custom font filename; empty uses standard DDNet")

MACRO_CONFIG_STR(ClLucentSoundPack, cl_lucent_sound_pack, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lucent sound pack; empty uses original DDNet sounds")

MACRO_CONFIG_INT(ClLucentBindwheelResetMouse, cl_lucent_bindwheel_reset_mouse, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset bindwheel cursor on opening")
MACRO_CONFIG_INT(ClLucentBindwheelAnimation, cl_lucent_bindwheel_animation, 150, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bindwheel reveal duration in milliseconds; 0 disables animation")
MACRO_CONFIG_INT(ClLucentBindwheelScale, cl_lucent_bindwheel_scale, 100, 50, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bindwheel size in percent")
MACRO_CONFIG_INT(ClLucentBindwheelMigrated, cl_lucent_bindwheel_migrated, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether initial other-client bindwheel discovery has completed")

MACRO_CONFIG_INT(ClLucentChatMentions, cl_lucent_chat_mentions, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show animated mention cards")

MACRO_CONFIG_INT(ClLucentChatSystem, cl_lucent_chat_system, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show other system messages and play their sounds")

MACRO_CONFIG_INT(ClLucentChatWelcome, cl_lucent_chat_welcome, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show recognized welcome and help messages and play their sounds")

MACRO_CONFIG_INT(ClLucentChatRace, cl_lucent_chat_race, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show recognized race reminders and play their sounds")

MACRO_CONFIG_INT(ClLucentChatConnections, cl_lucent_chat_connections, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show join, leave and rename messages and play their sounds")

MACRO_CONFIG_INT(ClLucentHoldHammer, cl_lucent_hold_hammer, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Repeat hammer attacks while holding fire")

MACRO_CONFIG_INT(ClLucentHoldGun, cl_lucent_hold_gun, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Repeat gun shots while holding fire")

MACRO_CONFIG_INT(ClLucentHoldJump, cl_lucent_hold_jump, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Repeat jumps while holding jump, including air jumps")
