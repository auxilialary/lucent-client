"""Extract production map handler and repacker for the isolated C++ regression fixture.

No copy of the request condition lives in the test. Fail closed if source markers change.
"""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
start = source.index("static inline bool RepackMsg(")
end = source.index("\nint CClient::SendMsg(", start)
repack = source[start:end]
start = source.index("void CClient::SendMapRequest()")
end = source.index("\nvoid CClient::RconAuth(", start)
request = source[start:end].replace("CClient::", "CMapDownloadHarness::", 1)
start = source.index("\t\telse if(Conn == CONN_MAIN && Msg == NETMSG_MAP_DATA)")
start = source.index("{", start)
end = source.index("\n\t\telse if(", start)
handler = source[start:end].strip()
assert handler.startswith("{") and handler.endswith("}")
pathlib.Path(sys.argv[2]).write_text(
    "// Generated from client.cpp; do not edit.\n" + repack
    + "\n" + request + "\nvoid CMapDownloadHarness::Receive(CUnpacker &Unpacker)\n" + handler + "\n",
    encoding="utf-8",
)
