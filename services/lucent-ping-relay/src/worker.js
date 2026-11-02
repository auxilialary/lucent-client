import { DurableObject } from "cloudflare:workers";
const SIZE = 32, MAX_MEMBERS = 64;
const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, "0")).join("");
const validHash = s => /^[0-9a-f]{64}$/.test(s || "") && /[1-9a-f]/.test(s);
const frame = (type, sender = 0n, id = 0n) => {
  const data = new ArrayBuffer(SIZE), v = new DataView(data);
  v.setUint32(0, 0x4c505232); v.setUint8(4, type); v.setBigUint64(8, sender); v.setBigUint64(16, id);
  return data;
};
export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === "/health") return new Response("Lucent ping relay v2");
    if (url.pathname === "/v1") return new Response("Lucent ping protocol v2 required; upgrade the client and use /v2", {status: 426});
    if (url.pathname !== "/v2" || request.method !== "GET") return new Response("Not found", {status: 404});
    if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") return new Response("WebSocket required", {status: 426});
    const server = url.searchParams.get("server"), map = url.searchParams.get("map");
    if (!validHash(server) || !validHash(map) || [...url.searchParams.keys()].length !== 2)
      return new Response("Invalid game context", {status: 400});
    const ip = request.headers.get("CF-Connecting-IP") || "local";
    const ipHash = hex(new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(ip))));
    if (!env.CONNECT_RATE || !(await env.CONNECT_RATE.limit({key: ipHash})).success)
      return new Response("Connection rate limit", {status: 429});
    const room = env.PING_ROOMS.getByName(`v2:${server}:${map}`);
    const headers = new Headers(request.headers);
    // Routing claims are not proof of identity. Overwrite internal, untrusted headers.
    headers.set("X-Lucent-IP", ipHash);
    return room.fetch(new Request(request, {headers}));
  }
};
export class PingRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair("lp2", "lp2"));
  }
  members() { return this.ctx.getWebSockets().filter(ws => ws.readyState === 1); }
  counts() {
    const peers = this.members(), message = frame(2, 0n, BigInt(peers.length));
    for (const ws of peers) { try { ws.send(message); } catch { ws.close(1011, "Send failed"); } }
  }
  fetch(request) {
    const peers = this.members(), ip = request.headers.get("X-Lucent-IP");
    if (peers.length >= MAX_MEMBERS || peers.filter(ws => ws.deserializeAttachment()?.ip === ip).length >= 4)
      return new Response("Room or per-IP connection limit", {status: 429});
    const [client, server] = Object.values(new WebSocketPair());
    let id;
    do { id = hex(crypto.getRandomValues(new Uint8Array(8))); }
    while (id === "0000000000000000" || peers.some(ws => ws.deserializeAttachment()?.id === id));
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({id, ip, last: "0000000000000000", tokens: 4, time: Date.now(), window: 0, count: 0, strikes: 0});
    this.counts();
    return new Response(null, {status: 101, webSocket: client});
  }
  webSocketMessage(ws, message) {
    const a = ws.deserializeAttachment();
    if (!a) { ws.close(1008, "Missing session"); return; }
    if (!(message instanceof ArrayBuffer) || message.byteLength !== SIZE) { ws.close(1008, "Invalid frame size/type"); return; }
    const v = new DataView(message), id = v.getBigUint64(16), x = v.getFloat32(24), y = v.getFloat32(28);
    if (v.getUint32(0) !== 0x4c505232 || v.getUint8(4) !== 1 || v.getUint8(5) || v.getUint16(6) < 1 || v.getUint16(6) > 128 || v.getBigUint64(8) !== 0n ||
        id === 0n || !Number.isFinite(x) || !Number.isFinite(y) || Math.abs(x) > 16777216 || Math.abs(y) > 16777216) {
      ws.close(1008, "Invalid ping"); return;
    }
    const now = Date.now(), window = Math.floor(now / 1000), event = id.toString(16).padStart(16, "0");
    a.tokens = Math.min(4, a.tokens + Math.max(0, now - a.time) * .004); a.time = now;
    if (a.tokens < 1) {
      a.strikes++; ws.serializeAttachment(a);
      if (a.strikes >= 8) ws.close(1008, "Ping rate limit"); return;
    }
    a.tokens--;
    if (event <= a.last) { ws.serializeAttachment(a); return; }
    a.last = event;
    if (a.window !== window) { a.window = window; a.count = 0; }
    const peers = this.members();
    const total = peers.reduce((n, peer) => { const p = peer.deserializeAttachment(); return n + (p?.window === window ? p.count : 0); }, 0);
    if (total >= 32) { ws.serializeAttachment(a); return; }
    a.count++; a.strikes = Math.max(0, a.strikes - 1); ws.serializeAttachment(a);
    const output = message.slice(0); new DataView(output).setBigUint64(8, BigInt(`0x${a.id}`));
    for (const peer of peers) if (peer !== ws) { try { peer.send(output); } catch { peer.close(1011, "Send failed"); } }
  }
  webSocketClose(ws, code) {
    ws.close(code === 1005 || code === 1006 ? 1000 : code, "Closed"); this.counts();
  }
  webSocketError(ws) { ws.close(1011, "Socket error"); this.counts(); }
}
