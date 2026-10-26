#ifndef GAME_CLIENT_LUCENT_CONSOLE_LOG_H
#define GAME_CLIENT_LUCENT_CONSOLE_LOG_H
#include <string_view>

enum class EConsoleLogGroup
{
	GRAPHICS,
	NETWORK,
	CHAT,
	SERVER,
	BROWSER,
	OTHER,
	CLIENT,
	ASSETS,
	RECORD,
	EDITOR,
	COUNT
};
inline EConsoleLogGroup ConsoleLogGroup(std::string_view Source)
{
	if(Source == "chat/server" || Source == "motd" || Source == "broadcast" || Source == "server_alert" || Source == "moderator_alert")
		return EConsoleLogGroup::SERVER;
	if(Source == "chat" || Source.starts_with("chat/") || Source == "chatresp")
		return EConsoleLogGroup::CHAT;
	if(Source == "gfx" || Source.starts_with("gfx/") || Source == "graphics" || Source.starts_with("graphics/") || Source == "sdl" || Source == "shadercompiler" || Source == "textrender" || Source == "png")
		return EConsoleLogGroup::GRAPHICS;
	if(Source == "serverbrowser" || Source == "serverbrowser_http" || Source == "serverbrowse_ping_cache" || Source == "menus/browser")
		return EConsoleLogGroup::BROWSER;
	if(Source == "client/network" || Source == "conn" || Source == "connection" || Source == "net" || Source == "net_ban" || Source == "network" || Source == "security" || Source == "host_lookup" || Source == "http" || Source == "curl" || Source == "webdl" || Source == "websockets" || Source.starts_with("stun/") || Source == "econ")
		return EConsoleLogGroup::NETWORK;
	if(Source == "editor" || Source.starts_with("editor/"))
		return EConsoleLogGroup::EDITOR;
	if(Source == "demo/slice" || Source == "demo_player" || Source == "demo_recorder" || Source == "ghost" || Source == "ghost_loader" || Source == "ghost_recorder" || Source == "replay" || Source == "videorecorder" || Source.starts_with("videorecorder/"))
		return EConsoleLogGroup::RECORD;
	if(Source == "countryflags" || Source == "datafile" || Source == "filesystem" || Source == "storage" || Source == "map/load" || Source == "map_renderer" || Source == "mapbugs" || Source == "mapimages" || Source == "mapsounds" || Source == "menuthemes" || Source == "skins" || Source == "skins7" || Source == "sound" || Source.starts_with("sound/"))
		return EConsoleLogGroup::ASSETS;
	if(Source == "client" || Source == "client/info" || Source == "gameclient" || Source == "engine" || Source == "console" || Source == "config" || Source == "binds" || Source == "camera" || Source == "joystick" || Source == "touch_controls" || Source == "menus" || Source == "localization" || Source == "discord" || Source == "steam" || Source == "steam/dbg" || Source == "updater" || Source == "stress" || Source == "info" || Source == "assert" || Source == "assertion" || Source == "crashdump" || Source == "exception_handling" || Source == "android" || Source == "ios")
		return EConsoleLogGroup::CLIENT;
	return EConsoleLogGroup::OTHER;
}
#endif
