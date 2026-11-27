//
// Licensed under the Academic Free License version 3.0
//
// inet::UdpSocket, TcpSocket and TcpServerSocket for the Sedona VM
// compiled to WebAssembly. Sockets are handles into the network of the
// JavaScript library (svm_lib.js): it connects sockets of this VM with
// each other (loopback) and hands datagrams for other addresses to the
// host. Addresses are the 16 bytes of an inet::IpAddr (IPv4 mapped into
// IPv6), in network order.
//

#include "sedona.h"
#include "inet_util.h"

// Provided by svm_lib.js. Handles are >= 0; -1 means failure.
extern int net_udp_open(void);
extern int net_udp_bind(int h, int port);
extern int net_udp_send(int h, const uint8_t* addr, int port, const uint8_t* buf, int len);
extern int net_udp_receive(int h, uint8_t* buf, int len, uint8_t* addrOut, int* portOut);
extern void net_close(int h);
extern int net_tcp_listen(int port);
extern int net_tcp_accept(int h);
extern int net_tcp_connect(const uint8_t* addr, int port);
extern int net_tcp_finish_connect(int h);
extern int net_tcp_write(int h, const uint8_t* buf, int len);
extern int net_tcp_read(int h, uint8_t* buf, int len);

// UdpDatagram field offsets (32-bit references)
#define DG_ADDR   0
#define DG_PORT   4
#define DG_BUF    8
#define DG_OFF    12
#define DG_LEN    16

////////////////////////////////////////////////////////////////
// UdpSocket
////////////////////////////////////////////////////////////////

Cell inet_UdpSocket_maxPacketSize(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.ival = 512;
  return r;
}

Cell inet_UdpSocket_idealPacketSize(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.ival = 512;
  return r;
}

Cell inet_UdpSocket_open(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  int h;
  if (!getClosed(self) || getSocket(self) != -1) return falseCell;
  h = net_udp_open();
  if (h < 0) return falseCell;
  setClosed(self, 0);
  setSocket(self, h);
  return trueCell;
}

Cell inet_UdpSocket_bind(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  if (getClosed(self)) return falseCell;
  return net_udp_bind(getSocket(self), params[1].ival) ? trueCell : falseCell;
}

Cell inet_UdpSocket_join(SedonaVM* vm, Cell* params)
{
  // No multicast: DASP discovery does not reach the simulated VM.
  return getClosed(params[0].aval) ? falseCell : trueCell;
}

Cell inet_UdpSocket_send(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  uint8_t* dg = (uint8_t*)params[1].aval;
  uint8_t* addr;
  uint8_t* buf;
  if (getClosed(self) || dg == NULL) return falseCell;
  addr = (uint8_t*)getRef(dg, DG_ADDR);
  buf  = (uint8_t*)getRef(dg, DG_BUF);
  if (addr == NULL || buf == NULL) return falseCell;
  return net_udp_send(getSocket(self), addr, getInt(dg, DG_PORT),
                      buf + getInt(dg, DG_OFF), getInt(dg, DG_LEN)) ? trueCell : falseCell;
}

Cell inet_UdpSocket_receive(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  uint8_t* dg = (uint8_t*)params[1].aval;
  uint8_t* buf;
  uint8_t* fromAddr = (uint8_t*)getInline(self, 8);   // the socket's receiveIpAddr
  int port = -1;
  int n;
  if (getClosed(self) || dg == NULL) return falseCell;
  buf = (uint8_t*)getRef(dg, DG_BUF);
  if (buf == NULL) return falseCell;
  n = net_udp_receive(getSocket(self), buf + getInt(dg, DG_OFF), getInt(dg, DG_LEN), fromAddr, &port);
  if (n < 0)
  {
    setInt(dg, DG_LEN, 0);
    setRef(dg, DG_ADDR, NULL);
    setInt(dg, DG_PORT, -1);
    return falseCell;
  }
  setInt(dg, DG_LEN, n);
  setRef(dg, DG_ADDR, fromAddr);
  setInt(dg, DG_PORT, port);
  return trueCell;
}

Cell inet_UdpSocket_close(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  if (!getClosed(self))
  {
    net_close(getSocket(self));
    setClosed(self, 1);
    setSocket(self, -1);
  }
  return nullCell;
}

////////////////////////////////////////////////////////////////
// TcpSocket
////////////////////////////////////////////////////////////////

Cell inet_TcpSocket_connect(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  int h = net_tcp_connect((const uint8_t*)params[1].aval, params[2].ival);
  if (h < 0) return falseCell;
  setSocket(self, h);
  return trueCell;
}

Cell inet_TcpSocket_finishConnect(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  int r = net_tcp_finish_connect(getSocket(self));
  if (r == 0) return falseCell;   // still connecting
  if (r > 0) setClosed(self, 0);  // connected; on failure it stays closed
  return trueCell;
}

Cell inet_TcpSocket_write(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  uint8_t* buf = (uint8_t*)params[1].aval;
  Cell r;
  r.ival = net_tcp_write(getSocket(self), buf + params[2].ival, params[3].ival);
  if (r.ival < 0)
  {
    net_close(getSocket(self));
    setClosed(self, 1);
    setSocket(self, -1);
  }
  return r;
}

Cell inet_TcpSocket_read(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  uint8_t* buf = (uint8_t*)params[1].aval;
  Cell r;
  r.ival = net_tcp_read(getSocket(self), buf + params[2].ival, params[3].ival);
  if (r.ival < 0)
  {
    net_close(getSocket(self));
    setClosed(self, 1);
    setSocket(self, -1);
  }
  return r;
}

Cell inet_TcpSocket_close(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  net_close(getSocket(self));
  setClosed(self, 1);
  setSocket(self, -1);
  return nullCell;
}

////////////////////////////////////////////////////////////////
// TcpServerSocket
////////////////////////////////////////////////////////////////

Cell inet_TcpServerSocket_bind(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  int h;
  if (!getClosed(self)) return falseCell;
  h = net_tcp_listen(params[1].ival);
  if (h < 0) return falseCell;
  setSocket(self, h);
  setClosed(self, 0);
  return trueCell;
}

Cell inet_TcpServerSocket_accept(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  void* accepted = params[1].aval;
  int h = net_tcp_accept(getSocket(self));
  if (h < 0) return falseCell;
  setSocket(accepted, h);
  setClosed(accepted, 0);
  return trueCell;
}

Cell inet_TcpServerSocket_close(SedonaVM* vm, Cell* params)
{
  void* self = params[0].aval;
  if (!getClosed(self))
  {
    net_close(getSocket(self));
    setClosed(self, 1);
    setSocket(self, -1);
  }
  return nullCell;
}
