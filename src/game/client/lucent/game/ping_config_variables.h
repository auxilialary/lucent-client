MACRO_CONFIG_STR(ClLucentPingRelayUrl, cl_lucent_ping_relay_url, 512, "wss://lucent-ping-relay.apolemia.workers.dev/v2", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Map ping relay WSS endpoint (public Lucent relay by default)")
// This Lucent test build enables relay by default. A saved zero always takes precedence.
MACRO_CONFIG_INT(ClLucentPingOnline, lucent_ping_online, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically connect map pings to the relay while in an online game")
