//
// Licensed under the Academic Free License version 3.0
//
// Emscripten JS library for the Sedona VM compiled to WebAssembly
// (linked with --js-library). It provides
//
// - the clock: Module.svmNowNs() if the host sets it (e.g. a virtual
//   clock), else performance.now(); Sys.sleep moves it forward;
// - a small network: UDP and TCP sockets of this VM reach each other over
//   loopback (127.x.x.x, ::1, 0.0.0.0). UDP datagrams for any other
//   address go to Module.svmNet.onUdpSend(fromPort, toAddr, toPort, data);
//   the host injects datagrams with Module.svmNet.deliverUdp(toPort,
//   fromAddr, fromPort, data). TCP to other addresses fails.
//
// Addresses are 16-byte Uint8Arrays as in inet::IpAddr (IPv4 mapped into
// IPv6, network order).
//

addToLibrary({
  $SvmNet__postset: 'Module["svmNet"] = SvmNet;',
  $SvmNet: {
    next: 0,
    socks: {},
    /** Host hook for datagrams that leave the VM. */
    onUdpSend: null,

    // A plain array: Emscripten writes library objects out as source, and a
    // typed array would not survive that.
    LOOPBACK: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 127, 0, 0, 1],

    isLocal: function (a) {
      var mapped = true;
      for (var i = 0; i < 10; i++) if (a[i] !== 0) mapped = false;
      if (mapped && a[10] === 0xff && a[11] === 0xff) {
        if (a[12] === 127) return true;
        if (a[12] === 0 && a[13] === 0 && a[14] === 0 && a[15] === 0) return true;
        return false;
      }
      for (var j = 0; j < 15; j++) if (a[j] !== 0) return false;
      return a[15] === 1 || a[15] === 0;
    },

    add: function (s) {
      var h = SvmNet.next++;
      SvmNet.socks[h] = s;
      return h;
    },

    udpByPort: function (port) {
      for (var h in SvmNet.socks) {
        var s = SvmNet.socks[h];
        if (s.kind === 'udp' && s.port === port) return s;
      }
      return null;
    },

    /** Queue a datagram for the VM socket bound to toPort; false if there is none. */
    deliverUdp: function (toPort, fromAddr, fromPort, data) {
      var s = SvmNet.udpByPort(toPort);
      if (!s) return false;
      s.queue.push({ addr: new Uint8Array(fromAddr), port: fromPort, data: new Uint8Array(data) });
      return true;
    },

    /** The UDP ports VM sockets are bound to. */
    udpPorts: function () {
      var out = [];
      for (var h in SvmNet.socks) if (SvmNet.socks[h].kind === 'udp' && SvmNet.socks[h].port >= 0) out.push(SvmNet.socks[h].port);
      return out;
    },

    /** Drop all sockets (before the VM restarts). */
    reset: function () {
      SvmNet.socks = {};
    },
  },

  // The VM clock: the host's (Module.svmNowNs, e.g. a virtual clock) or
  // performance.now(), plus the time the VM slept. Sys.sleep cannot block in
  // JavaScript, so it moves the VM clock forward instead; a running app never
  // sleeps (WasmPlatformService yields), but tests and Sys.main code do.
  $SvmClock__postset: 'Module["svmClock"] = SvmClock;',
  $SvmClock: {
    slept: 0,
  },

  svm_now_ns__deps: ['$SvmClock'],
  svm_now_ns: function () {
    var now = Module['svmNowNs'] ? Module['svmNowNs']() : performance.now() * 1e6;
    return now + SvmClock.slept;
  },

  svm_sleep_ns__deps: ['$SvmClock'],
  svm_sleep_ns: function (ns) {
    if (ns > 0) SvmClock.slept += ns;
  },

  // ---------------------------------------------------------------- UDP

  net_udp_open__deps: ['$SvmNet'],
  net_udp_open: function () {
    return SvmNet.add({ kind: 'udp', port: -1, queue: [] });
  },

  net_udp_bind__deps: ['$SvmNet'],
  net_udp_bind: function (h, port) {
    var s = SvmNet.socks[h];
    if (!s || s.kind !== 'udp' || s.port >= 0 || SvmNet.udpByPort(port)) return 0;
    s.port = port;
    return 1;
  },

  net_udp_send__deps: ['$SvmNet'],
  net_udp_send: function (h, addrPtr, port, bufPtr, len) {
    var s = SvmNet.socks[h];
    if (!s || s.kind !== 'udp' || len < 0) return 0;
    var addr = HEAPU8.slice(addrPtr, addrPtr + 16);
    var data = HEAPU8.slice(bufPtr, bufPtr + len);
    if (SvmNet.isLocal(addr)) {
      // Loopback to a port nobody listens on: the datagram is lost, like UDP.
      SvmNet.deliverUdp(port, SvmNet.LOOPBACK, s.port, data);
      return 1;
    }
    if (SvmNet.onUdpSend) SvmNet.onUdpSend(s.port, addr, port, data);
    return 1;
  },

  net_udp_receive__deps: ['$SvmNet'],
  net_udp_receive: function (h, bufPtr, len, addrOutPtr, portOutPtr) {
    var s = SvmNet.socks[h];
    if (!s || s.kind !== 'udp' || s.queue.length === 0) return -1;
    var d = s.queue.shift();
    var n = Math.min(len, d.data.length);
    HEAPU8.set(d.data.subarray(0, n), bufPtr);
    HEAPU8.set(d.addr, addrOutPtr);
    HEAP32[portOutPtr >> 2] = d.port;
    return n;
  },

  net_close__deps: ['$SvmNet'],
  net_close: function (h) {
    var s = SvmNet.socks[h];
    if (!s) return;
    if (s.peer) s.peer.peerClosed = true;
    delete SvmNet.socks[h];
  },

  // ---------------------------------------------------------------- TCP (loopback only)

  net_tcp_listen__deps: ['$SvmNet'],
  net_tcp_listen: function (port) {
    for (var h in SvmNet.socks) {
      var s = SvmNet.socks[h];
      if (s.kind === 'listen' && s.port === port) return -1;
    }
    return SvmNet.add({ kind: 'listen', port: port, pending: [] });
  },

  net_tcp_accept__deps: ['$SvmNet'],
  net_tcp_accept: function (h) {
    var s = SvmNet.socks[h];
    if (!s || s.kind !== 'listen' || s.pending.length === 0) return -1;
    return SvmNet.add(s.pending.shift());
  },

  net_tcp_connect__deps: ['$SvmNet'],
  net_tcp_connect: function (addrPtr, port) {
    var addr = HEAPU8.slice(addrPtr, addrPtr + 16);
    var listener = null;
    if (SvmNet.isLocal(addr)) {
      for (var h in SvmNet.socks) {
        var s = SvmNet.socks[h];
        if (s.kind === 'listen' && s.port === port) listener = s;
      }
    }
    // Like a non-blocking connect: it fails later, in finishConnect.
    if (!listener) return SvmNet.add({ kind: 'tcp', state: -1, inbox: [], peer: null });
    var client = { kind: 'tcp', state: 1, inbox: [], peer: null, peerClosed: false };
    var server = { kind: 'tcp', state: 1, inbox: [], peer: client, peerClosed: false };
    client.peer = server;
    listener.pending.push(server);
    return SvmNet.add(client);
  },

  net_tcp_finish_connect__deps: ['$SvmNet'],
  net_tcp_finish_connect: function (h) {
    var s = SvmNet.socks[h];
    return s && s.kind === 'tcp' ? s.state : -1;
  },

  net_tcp_write__deps: ['$SvmNet'],
  net_tcp_write: function (h, bufPtr, len) {
    var s = SvmNet.socks[h];
    if (!s || s.kind !== 'tcp' || s.state !== 1 || s.peerClosed || !s.peer) return -1;
    var data = HEAPU8.slice(bufPtr, bufPtr + len);
    for (var i = 0; i < data.length; i++) s.peer.inbox.push(data[i]);
    return len;
  },

  net_tcp_read__deps: ['$SvmNet'],
  net_tcp_read: function (h, bufPtr, len) {
    var s = SvmNet.socks[h];
    if (!s || s.kind !== 'tcp' || s.state !== 1) return -1;
    if (s.inbox.length === 0) return s.peerClosed ? -1 : 0;
    var n = Math.min(len, s.inbox.length);
    HEAPU8.set(s.inbox.splice(0, n), bufPtr);
    return n;
  },
});
