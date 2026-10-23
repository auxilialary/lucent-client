"""Temporary join probes. Run from repository root: python .../instrument.py apply|remove.

Backups are byte-exact; removal refuses to overwrite subsequently edited sources.
No external packages. Probes use uncached monotonic time, never time_get().
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
STATE = ROOT / 'out/instant-join-probes'
HEADER = ROOT / 'src/base/instant_join_probe.h'

def main():
    mode = sys.argv[1]
    if mode == 'remove':
        manifest = json.loads((STATE / 'manifest.json').read_text())
        for name, digest in manifest.items():
            if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest:
                raise SystemExit(f'Refusing to overwrite edited file: {name}')
        for name in manifest:
            target = ROOT / name
            if target == HEADER:
                target.unlink()
            else:
                target.write_bytes((STATE / name).read_bytes())
        (STATE / 'manifest.json').unlink()
        return
    assert mode == 'apply'
    assert not HEADER.exists() and not (STATE / 'manifest.json').exists()
    edits = {}
    def edit(name, before, after):
        if name not in edits:
            edits[name] = (ROOT / name).read_text(encoding='utf-8')
        assert edits[name].count(before) == 1, (name, before)
        edits[name] = edits[name].replace(before, after)
    client = 'src/engine/client/client.cpp'
    game = 'src/game/client/gameclient.cpp'
    edit(client, 'void CClient::Connect(const char *pAddress, const char *pPassword)\n{',
         'void CClient::Connect(const char *pAddress, const char *pPassword)\n{\n\tJoinProbeBegin("connect");')
    edit(client, '\tm_ConnectionId = RandomUuid();', '\tJoinProbeEvent("dns_done");\n\tm_ConnectionId = RandomUuid();')
    edit(client, '\t\t\tSendInfo(CONN_MAIN);', '\t\t\tJoinProbeEvent("transport_online");\n\t\t\tSendInfo(CONN_MAIN);')
    edit(client, '\t\t\tstd::optional<CMapDetails> MapDetails = std::nullopt;',
         '\t\t\tif(State() == IClient::STATE_ONLINE) JoinProbeBegin("map_switch");\n\t\t\tJoinProbeEvent("map_change");\n\t\t\tstd::optional<CMapDetails> MapDetails = std::nullopt;')
    edit(client, '\tGameClient()->OnEnterGame();\n}', '\tGameClient()->OnEnterGame();\n\tJoinProbeEvent("enter_game");\n}')
    edit(client, '\t\t\tGameClient()->OnConnected();', '\t\t\tJoinProbeEvent("con_ready");\n\t\t\tGameClient()->OnConnected();')
    edit(client, '\t\t\t\t\tm_aReceivedSnapshots[Conn]++;',
         '\t\t\t\t\tm_aReceivedSnapshots[Conn]++;\n\t\t\t\t\tif(Conn == CONN_MAIN && m_aReceivedSnapshots[Conn] <= 2) JoinProbeEvent(m_aReceivedSnapshots[Conn] == 1 ? "snap1" : "snap2");')
    edit(client, '\t\t\tSendMsg(i, &Msg, MSGFLAG_FLUSH);', '\t\t\tSendMsg(i, &Msg, MSGFLAG_FLUSH);\n\t\t\tif(i == CONN_MAIN && State() == IClient::STATE_ONLINE) JoinProbeOnce(1, "input_sent");')
    edit(client, '\t\t\t\tm_pGraphics->Swap();', '\t\t\t\tm_pGraphics->Swap();\n\t\t\t\tif(g_JoinProbeRendered) JoinProbeOnce(3, "swap_submitted");')
    edit(client, 'void CClient::SendReady(int Conn)\n{', 'void CClient::SendReady(int Conn)\n{\n\tJoinProbeEvent("send_ready");')
    edit(client, 'void CClient::FinishMapDownload()\n{', 'void CClient::FinishMapDownload()\n{\n\tJoinProbeEvent("download_done");')
    edit(game, '\t\t// send the initial info\n\t\tSendInfo(true);', '\t\t// send the initial info\n\t\tJoinProbeEvent("resources_done");\n\t\tSendInfo(true);')
    edit(game, '\tCLineInput::RenderCandidates();', '\tCLineInput::RenderCandidates();\n\tif(Client()->State() == IClient::STATE_ONLINE && m_Snap.m_pLocalCharacter && !m_Menus.IsActive() && !m_Chat.IsActive())\n\t{\n\t\tJoinProbeOnce(2, "character_frame_cpu");\n\t\tg_JoinProbeRendered = true;\n\t}')
    scopes = [
        (client, 'const char *CClient::LoadMap(', 'load_map'),
        (game, 'void CGameClient::OnConnected()', 'on_connected'),
        ('src/engine/shared/datafile.cpp', 'bool CDataFileReader::Open(const char *', 'datafile_open'),
        ('src/engine/shared/map.cpp', 'bool CMap::Load(const char *', 'map_parse'),
        ('src/game/client/components/mapimages.cpp', 'void CMapImages::OnMapLoadImpl(', 'map_images'),
        ('src/game/client/components/maplayers.cpp', 'void CMapLayers::OnMapLoad()', 'map_layers'),
        ('src/game/client/components/mapsounds.cpp', 'void CMapSounds::OnMapLoad()', 'map_sounds'),
        ('src/game/layers.cpp', 'void CLayers::Init(', 'layers_init'),
        ('src/game/collision.cpp', 'void CCollision::Init(', 'collision_init'),
    ]
    if 'IsPreparing' in (ROOT / 'src/game/client/components/maplayers.h').read_text():
        scopes += [
            ('src/game/client/components/mapimages.cpp', 'void CMapImages::PrepareNext()', 'deferred_image'),
            ('src/game/map/map_renderer.cpp', 'void CMapRenderer::PrepareNext()', 'deferred_layer'),
        ]
    for name, signature, label in scopes:
        source = edits.get(name, (ROOT / name).read_text(encoding='utf-8'))
        start = source.index(signature)
        end = source.index('\n{', start) + 2
        edit(name, source[start:end], source[start:end] + f'\n\tCJoinProbeScope JoinScope("{label}");')
    edit(client, 'void CClient::PumpNetwork()\n{', 'void CClient::PumpNetwork()\n{\n\tJoinProbePump();')
    edit(client, '\t\t\t\tif(g_JoinProbeRendered) JoinProbeOnce(3, "swap_submitted");', '\t\t\t\tif(g_JoinProbeRendered) JoinProbeOnce(3, "swap_submitted");\n\t\t\t\tif(g_JoinProbeFull) JoinProbeOnce(4, "full_scene_submitted");')
    ready = 'true'
    if 'IsPreparing' in (ROOT / 'src/game/client/components/maplayers.h').read_text():
        ready = '!m_MapImages.IsPreparing() && !m_MapLayersBackground.IsPreparing() && !m_MapLayersForeground.IsPreparing()'
    edit(game, '\tCLineInput::RenderCandidates();', '\tCLineInput::RenderCandidates();\n\tif(g_JoinProbeCriticalReady && (' + ready + ')) JoinProbeOnce(5, "decorative_ready");')
    edit(game, '\t\tg_JoinProbeRendered = true;', '\t\tg_JoinProbeRendered = true;\n\t\tif(' + ready + ') g_JoinProbeFull = true;')
    edit('src/game/client/components/controls.cpp',
        '\t\tConsole()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Move right");',
        '\t\tConsole()->Register("+right", "", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Move right");\n\t\tConsole()->Register("joinprobe_right", "i", CFGFLAG_CLIENT, ConKeyInputState, &s_State, "Local benchmark held input");')
    edit(game, '\t\tg_JoinProbeRendered = true;', '\t\tg_JoinProbeRendered = true;\n\t\tif(m_Controls.m_aInputDirectionRight[0])\n\t\t{\n\t\t\tif(!g_JoinProbeMotionStarted) { g_JoinProbeMotionStarted = true; g_JoinProbeMotionX = m_Snap.m_pLocalCharacter->m_X; }\n\t\t\tif(m_Snap.m_pLocalCharacter->m_X > g_JoinProbeMotionX + 16) JoinProbeOnce(6, "authoritative_motion");\n\t\t}')
    header = '''#ifndef BASE_INSTANT_JOIN_PROBE_H
#define BASE_INSTANT_JOIN_PROBE_H
#include <base/time.h>
#include <base/log.h>
#include <base/str.h>
inline bool g_JoinProbeActive = false;
inline bool g_JoinProbeMotionStarted = false;
inline int g_JoinProbeMotionX = 0;
inline bool g_JoinProbeRendered = false;
inline bool g_JoinProbeFull = false;
inline bool g_JoinProbeCriticalReady = false;
inline bool g_JoinProbeOnline = false;
inline bool g_JoinProbeLastCriticalReady = false;
inline bool g_JoinProbeLastOnline = false;
inline long long g_JoinProbeLastPump = 0;
inline long long g_JoinProbeMaxPumpGap = 0;
inline unsigned g_JoinProbeMask = 0;
inline int g_JoinProbeEpoch = 0;
inline void JoinProbeEvent(const char *pName)
{
    if(str_comp(pName, "resources_done") == 0) g_JoinProbeCriticalReady = true;
    if(str_comp(pName, "snap2") == 0) g_JoinProbeOnline = true;
    if(g_JoinProbeActive) log_info("joinprobe", "epoch=%d event=%s ns=%lld", g_JoinProbeEpoch, pName, (long long)time_get_nanoseconds().count());
}
inline void JoinProbeBegin(const char *pName)
{
    g_JoinProbeActive = true; g_JoinProbeMotionStarted = false; g_JoinProbeRendered = false; g_JoinProbeFull = false; g_JoinProbeCriticalReady = false; g_JoinProbeOnline = false; g_JoinProbeLastCriticalReady = false; g_JoinProbeLastOnline = false; g_JoinProbeLastPump = 0; g_JoinProbeMaxPumpGap = 0; g_JoinProbeMask = 0; ++g_JoinProbeEpoch; JoinProbeEvent(pName);
}
inline void JoinProbeOnce(unsigned Bit, const char *pName)
{
    if(!(g_JoinProbeMask & (1u << Bit))) { g_JoinProbeMask |= 1u << Bit; JoinProbeEvent(pName); }
}
inline void JoinProbePump()
{
    const auto Now = time_get_nanoseconds().count();
    if(g_JoinProbeActive && !(g_JoinProbeMask & (1u << 4)))
    {
        if(g_JoinProbeLastPump && Now - g_JoinProbeLastPump > g_JoinProbeMaxPumpGap)
        {
            g_JoinProbeMaxPumpGap = Now - g_JoinProbeLastPump;
            log_info("joinscope", "epoch=%d scope=max_network_gap ms=%.6f", g_JoinProbeEpoch, g_JoinProbeMaxPumpGap / 1e6);
        }
        if(g_JoinProbeLastPump && g_JoinProbeLastCriticalReady)
            log_info("joinscope", "epoch=%d scope=%s ms=%.6f", g_JoinProbeEpoch, g_JoinProbeLastOnline ? "online_network_gap" : "deferred_network_gap", (Now - g_JoinProbeLastPump) / 1e6);
        g_JoinProbeLastCriticalReady = g_JoinProbeCriticalReady;
        g_JoinProbeLastOnline = g_JoinProbeOnline;
        g_JoinProbeLastPump = Now;
    }
}
class CJoinProbeScope
{
    const char *m_pName;
    long long m_Start;
public:
    CJoinProbeScope(const char *pName) : m_pName(pName), m_Start(time_get_nanoseconds().count()) {}
    ~CJoinProbeScope() { if(g_JoinProbeActive) log_info("joinscope", "epoch=%d scope=%s ms=%.6f", g_JoinProbeEpoch, m_pName, (time_get_nanoseconds().count() - m_Start) / 1e6); }
};
#endif
'''
    # All probes above run on the main thread. Do not include in workers/backends.
    manifest = {}
    for name, content in edits.items():
        backup = STATE / name
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_bytes((ROOT / name).read_bytes())
        (ROOT / name).write_text('#include <base/instant_join_probe.h>\n' + content, encoding='utf-8', newline='\n')
        manifest[name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
    HEADER.write_text(header, encoding='utf-8', newline='\n')
    manifest[str(HEADER.relative_to(ROOT)).replace('\\', '/')] = hashlib.sha256(HEADER.read_bytes()).hexdigest()
    (STATE / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')

if __name__ == '__main__':
    main()
