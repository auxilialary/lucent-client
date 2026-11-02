#ifndef GAME_CLIENT_LUCENT_GAME_PING_IDENTITY_H
#define GAME_CLIENT_LUCENT_GAME_PING_IDENTITY_H
namespace LucentPing
{
	// DDNet's normalized 0.6/0.7 client-slot space. This is a claim, not authentication.
	constexpr int MaxSenderClients = 128;
	constexpr bool ValidSenderClientId(int Id) { return Id >= 0 && Id < MaxSenderClients; }
	constexpr int LocalSenderClientId(int SnapshotLocal, int Main, int Dummy, bool ControlDummy, bool DummyConnected)
	{
		const int Selected = ControlDummy && DummyConnected ? Dummy : Main;
		return ValidSenderClientId(Selected) && SnapshotLocal == Selected ? Selected : -1;
	}
	constexpr bool SenderPresent(int Id, bool SnapshotPlayerInfo, bool ClientActive)
	{
		return ValidSenderClientId(Id) && SnapshotPlayerInfo && ClientActive;
	}
}
#endif
