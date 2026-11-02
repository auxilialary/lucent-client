import { test } from "node:test";
import assert from "node:assert/strict";
import { Miniflare } from "miniflare";
import { spawn } from "node:child_process";
import { readFile } from "node:fs/promises";

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(predicate, timeout = 5000) {
  const end = Date.now() + timeout;
  while (!predicate()) { if (Date.now() > end) throw new Error("Timed out"); await sleep(10); }
}
const hash = n => n.toString(16).padStart(2, "0") + "00".repeat(31);
const ping = (id, x = 42.5, y = -13.125, clientId = 3) => {
  const b = Buffer.alloc(32); b.write("LPR2"); b[4] = 1; b.writeUInt16BE(clientId+1,6);
  b.writeBigUInt64BE(BigInt(id), 16); b.writeFloatBE(x, 24); b.writeFloatBE(y, 28); return b;
};
async function open(base, server = 1, map = 2) {
  const ws = new WebSocket(`${base}/v2?server=${hash(server)}&map=${hash(map)}`);
  ws.binaryType = "arraybuffer";
  const client = {ws, members: 0, pings: [], text: [], closed: false};
  ws.addEventListener("message", e => {
    if (typeof e.data === "string") { client.text.push(e.data); return; }
    const b = Buffer.from(e.data);
    if (b[4] === 2) client.members = Number(b.readBigUInt64BE(16));
    else client.pings.push(b);
  });
  ws.addEventListener("close", () => { client.closed = true; });
  await new Promise((resolve, reject) => { ws.addEventListener("open", resolve, {once: true}); ws.addEventListener("error", reject, {once: true}); });
  return client;
}
async function close(c) { c.ws.close(); await until(() => c.closed); }
const options = {ratelimits: {CONNECT_RATE: {namespace_id: "1001", simple: {limit: 60, period: 60}}}, modules: true, scriptPath: "src/worker.js", compatibilityDate: "2026-07-30", durableObjects: {PING_ROOMS: {className: "PingRoom", useSQLite: true}}};

test("Durable Object relay: isolation, bidirectional events, duplicates, flood, cleanup and reconnect", {timeout: 60000}, async () => {
  let mf = new Miniflare(options);
  const clients = [];
  try {
    const ready = await mf.ready, base = `ws://127.0.0.1:${ready.port}`;
    const A = await open(base), B = await open(base), C = await open(base, 1, 3), D = await open(base, 3, 2); clients.push(A, B, C, D);
    await until(() => A.members === 2 && B.members === 2 && C.members === 1 && D.members === 1);
    A.ws.send(ping(1)); B.ws.send(ping(1, -.03125, 12345.125, 7));
    await until(() => A.pings.length === 1 && B.pings.length === 1);
    assert.equal(B.pings[0].readUInt16BE(6)-1,3); assert.equal(A.pings[0].readUInt16BE(6)-1,7);
    assert.equal(B.pings[0].readFloatBE(24), 42.5); assert.equal(A.pings[0].readFloatBE(28), 12345.125);
    assert.notEqual(A.pings[0].readBigUInt64BE(8), 0n);
    assert.notEqual(A.pings[0].readBigUInt64BE(8), B.pings[0].readBigUInt64BE(8));
    A.ws.send(ping(1)); await sleep(100);
    assert.equal(B.pings.length, 1); assert.equal(A.pings.length, 1); // duplicate and no self echo
    assert.equal(C.pings.length, 0); assert.equal(D.pings.length, 0);
    A.ws.send("lp2"); await until(() => A.text.includes("lp2")); // hibernating auto-response
    for (const bad of [Buffer.alloc(31), Buffer.alloc(1000), ping(2, NaN), ping(2, Infinity), ping(2, 16777218), ping(2,0,0,-1), ping(2,0,0,128)]) {
      const E = await open(base); clients.push(E); E.ws.send(bad); await until(() => E.closed);
      await until(() => A.members === 2);
    }
    const E = await open(base); clients.push(E);
    const spoof = ping(2); spoof.writeBigUInt64BE(42n, 8); E.ws.send(spoof); await until(() => E.closed);
    await until(() => A.members === 2);
    const before = B.pings.length, Flood = await open(base); clients.push(Flood);
    for (let id = 1; id <= 100; id++) Flood.ws.send(ping(id));
    await until(() => Flood.closed); await sleep(100);
    assert.ok(B.pings.length - before <= 4); await until(() => A.members === 2);
    const X = await open(base), Y = await open(base); clients.push(X, Y);
    const denied = await mf.dispatchFetch(`http://localhost/v2?server=${hash(1)}&map=${hash(2)}`, {headers: {Upgrade: "websocket"}});
    assert.equal(denied.status, 429); // maximum four sockets per IP per room
    await close(X); await close(Y); await until(() => A.members === 2);
    await close(B); await until(() => A.members === 1);
    const Again = await open(base); clients.push(Again); Again.ws.send(ping(1));
    const prior = A.pings.length; await until(() => A.pings.length > prior);
    await close(Again); await until(() => A.members === 1);
    for (const client of clients) if (!client.closed) await close(client);
    const Empty = await open(base); clients.push(Empty); await until(() => Empty.members === 1);
    assert.equal(Empty.pings.length, 0); await close(Empty); // no event history on reconnect

    if (process.env.LUCENT_PING_NATIVE_TEST) {
      const tlsMf = new Miniflare({...options, https: true});
      const tlsReady = await tlsMf.ready;
      try {
      await new Promise((resolve, reject) => {
        const child = spawn(process.env.LUCENT_PING_NATIVE_TEST, ["--gtest_filter=LucentPingRelay.*", "--no-capture"], {
          env: {...process.env, LUCENT_PING_TEST_RELAY_URL: `${base}/v2`, LUCENT_PING_TEST_RESTART: "1", LUCENT_PING_TEST_UNTRUSTED_WSS_URL: `wss://localhost:${tlsReady.port}/v2`}, stdio: ["ignore", "pipe", "pipe"]
        });
        const timer = setTimeout(() => { child.kill(); reject(new Error("Native relay test timed out")); }, 25000);
        let output = "", restarted = false;
        child.stdout.on("data", data => {
          process.stdout.write(data); output += data;
          if (!restarted && output.includes("NATIVE_RELAY_RECONNECT_READY")) {
            restarted = true;
            (async () => { await mf.dispose(); mf = new Miniflare({...options, port: Number(ready.port)}); await mf.ready; })().catch(reject);
          }
        });
        child.stderr.on("data", data => process.stderr.write(data));
        child.on("error", reject); child.on("exit", code => { clearTimeout(timer); code === 0 ? resolve() : reject(new Error(`Native relay tests exited ${code}`)); });
      });
      } finally { await tlsMf.dispose(); }
      // Separate OS processes exercise production relay code with independent config sessions.
      await Promise.all(["A", "B"].map(role => new Promise((resolve, reject) => {
        const child = spawn(process.env.LUCENT_PING_NATIVE_TEST, ["--gtest_filter=LucentPingRelay.PersistentAutoClientProcess", "--no-capture"], {
          env: {...process.env, LUCENT_PING_TEST_RELAY_URL: `${base}/v2`, LUCENT_PING_AUTO_PEER: role}, stdio: ["ignore", "pipe", "pipe"]
        });
        const timer = setTimeout(() => { child.kill(); reject(new Error(`Auto peer ${role} timed out`)); }, 30000);
        child.stdout.on("data", data => process.stdout.write(data)); child.stderr.on("data", data => process.stderr.write(data));
        child.on("error", reject); child.on("exit", code => { clearTimeout(timer); code === 0 ? resolve() : reject(new Error(`Auto peer ${role} exited ${code}`)); });
      })));
    }
  } finally {
    for (const c of clients) if (!c.closed) c.ws.close();
    await mf.dispose();
  }
});

test("Admission rate limit and invalid context fail closed", {timeout: 15000}, async () => {
  const mf = new Miniflare({...options, ratelimits: {CONNECT_RATE: {namespace_id: "1001", simple: {limit: 2, period: 60}}}});
  try {
    const legacy = await mf.dispatchFetch("http://localhost/v1", {headers: {Upgrade: "websocket"}}); assert.equal(legacy.status,426);
    let response = await mf.dispatchFetch("http://localhost/v2?server=invalid&map=invalid", {headers: {Upgrade: "websocket"}});
    assert.equal(response.status, 400);
    for (let i=0; i<3; ++i) {
      response = await mf.dispatchFetch(`http://localhost/v2?server=${hash(1)}&map=${hash(i+1)}`, {headers: {Upgrade: "websocket"}});
      assert.equal(response.status, i < 2 ? 101 : 429);
      if (response.webSocket) { response.webSocket.accept(); response.webSocket.close(); }
    }
  } finally { await mf.dispose(); }
});

test("lp2 keepalive uses Hibernation auto-response without invoking DO message handler", {timeout: 15000}, async () => {
  const source = await readFile("src/worker.js", "utf8");
  assert.ok(source.includes("webSocketMessage(ws, message) {"));
  // A test-only trap: any JS message callback would close/fail this connection.
  const script = source.replace("webSocketMessage(ws, message) {", "webSocketMessage(ws, message) { throw new Error('Keepalive woke JavaScript handler');");
  const {scriptPath, ...baseOptions} = options;
  const mf = new Miniflare({...baseOptions, script});
  let A, B;
  try {
    const ready = await mf.ready, base = `ws://127.0.0.1:${ready.port}`;
    A = await open(base); B = await open(base);
    await until(() => A.members === 2 && B.members === 2);
    for (let i=0; i<5; ++i) A.ws.send("lp2");
    await until(() => A.text.length === 5);
    assert.ok(A.text.every(value => value === "lp2"));
    assert.equal(B.text.length,0); assert.equal(A.closed,false); assert.equal(B.closed,false);
    assert.equal(A.pings.length,0); assert.equal(B.pings.length,0);
  } finally {
    if(A && !A.closed) await close(A); if(B && !B.closed) await close(B); await mf.dispose();
  }
});
