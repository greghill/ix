#!/usr/bin/env python

import ctypes
import fcntl
import mmap
import posix_ipc
import socket
import struct
import sys
import time

MY_MAC = [0xB8,0xCA,0x3A,0xF7,0x4D,0x87]
DEVICE = 'eth0'

ETH_P_ALL = 0x0003
SIOCGIFINDEX = 0x8933
SIOCGIFFLAGS = 0x8913
IFF_PROMISC = 0x100
SIOCSIFFLAGS = 0x8914
BROADCAST_MAC = [0xff,0xff,0xff,0xff,0xff,0xff]

QUEUE_SIZE = 16

class Packet(ctypes.Structure):
  _fields_ = [
    ('size', ctypes.c_uint),
    ('data', ctypes.c_ubyte * 4096),
  ]

class QueueItem(ctypes.Structure):
  _fields_ = [
    ('done', ctypes.c_uint),
    ('data_offset', ctypes.c_ulonglong),
  ]

class Queue(ctypes.Structure):
  _fields_ = [
    ('enqueuep', ctypes.c_uint),
    ('dequeuep', ctypes.c_uint),
    ('queue_item_alloc', ctypes.c_ulonglong),
    ('item', QueueItem * QUEUE_SIZE),
  ]

  def dequeue(self):
    next = (self.dequeuep + 1) % QUEUE_SIZE
    if self.enqueuep == next:
      return None, None
    self.dequeuep = next
    return self.item[next], Packet.from_buffer(shmem.mempool_area, self.item[next].data_offset)

  def enqueue(self, f):
    if self.enqueuep == self.dequeuep:
      return 0
    packet = Packet.from_buffer(shmem.mempool_area, self.item[self.enqueuep].data_offset)
    if not f(packet):
      return 0
    self.enqueuep = (self.enqueuep + 1) % QUEUE_SIZE
    return 1

  def __str__(self):
    str = 'enqueuep = %d, dequeuep = %d, items = ' % (self.enqueuep, self.dequeuep)
    str += ''.join(('%x ' % i.data_offset for i in self.item))
    return str

class ShMem(ctypes.Structure):
  _fields_ = [
    ('rx_queue', Queue),
    ('tx_queue', Queue),
    ('mempool_area', ctypes.c_ubyte * (1024 * 1024 * 4 - 1024)),
  ]

def init_shmem():
  global shmem
  size = 4*1<<20
  shm = posix_ipc.SharedMemory('/vnic_shmem', posix_ipc.O_CREAT, size = size)
  buffer = mmap.mmap(shm.fd, size, mmap.MAP_SHARED, mmap.PROT_WRITE)
  shmem = ShMem.from_buffer(buffer)

def init_raw_socket(device):
  sock = socket.socket(socket.PF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))

  if sock < 1:
    print 'ERROR: Could not open socket, Got #?'
    sys.exit(1)

  ifr = struct.pack('16sh', device, 0)
  ifr = fcntl.ioctl(sock, SIOCGIFFLAGS, ifr)
  ifr_flags = struct.unpack('16sh', ifr)[1]
  ifr_flags |= IFF_PROMISC
  fcntl.ioctl(sock, SIOCSIFFLAGS, ifr)

  sock.settimeout(.5)

  return sock

def is_pkt_for_me(packet):
  return packet.data[0:6] == MY_MAC
  return packet.data[0:6] == BROADCAST_MAC or packet.data[0:6] == MY_MAC

def main():
  init_shmem()

  def recv_into_packet(packet):
    try:
      packet.size = sock.recv_into(packet.data, 4096)
    except socket.timeout:
      return 0
    return is_pkt_for_me(packet)

  sock = init_raw_socket(DEVICE)
  while True:
    shmem.rx_queue.enqueue(recv_into_packet)
    item, packet = shmem.tx_queue.dequeue()
    if packet is not None:
      data = ''.join((chr(x) for x in packet.data[0:packet.size]))
      sock.sendto(data, (DEVICE,0))
      item.done = 1

if __name__ == '__main__':
  main()
