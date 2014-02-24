#!/usr/bin/env python

import ctypes
import fcntl
import mmap
import posix_ipc
import socket
import struct
import sys
import time

MY_MAC = ''.join(chr(x) for x in [0x00, 0x16, 0x3e, 0x1f, 0x4a, 0x6b])
DEVICE = 'eth0'

ETH_P_ALL = 0x0003
BROADCAST_MAC = ''.join(chr(x) for x in [0xff,0xff,0xff,0xff,0xff,0xff])

QUEUE_SIZE = 16

class Descriptor(ctypes.Structure):
  _fields_ = [
    ('done', ctypes.c_uint),
    ('len', ctypes.c_uint),
    ('addr', ctypes.c_ulonglong),
  ]

class ShMem(ctypes.Structure):
  _fields_ = [
    ('rx_head', ctypes.c_uint),
    ('tx_head', ctypes.c_uint),
    ('rx_tail', ctypes.c_uint),
    ('tx_tail', ctypes.c_uint),
    ('rx_ring', Descriptor * QUEUE_SIZE),
    ('tx_ring', Descriptor * QUEUE_SIZE),
    ('mempool_area', ctypes.c_ubyte * (1024 * 1024 * 4 - 1024)),
  ]

def init_shmem():
  global shmem
  size = 4*1<<20
  shm = posix_ipc.SharedMemory('/vnic_shmem', posix_ipc.O_CREAT, size = size)
  buffer = mmap.mmap(shm.fd, size, mmap.MAP_SHARED, mmap.PROT_WRITE)
  shmem = ShMem.from_buffer(buffer)

def init_raw_socket():
  sock = socket.socket(socket.PF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))

  if sock < 1:
    print 'ERROR: Could not open socket, Got #?'
    sys.exit(1)

  sock.settimeout(.5)

  return sock

def is_pkt_for_me(packet):
  return packet[0:6] == BROADCAST_MAC or packet[0:6] == MY_MAC

def main():
  init_shmem()

  def recv_wrapper():
    try:
      data = sock.recv(4096)
    except socket.timeout:
      return None
    if is_pkt_for_me(data):
      return data
    return None

  sock = init_raw_socket()
  while True:
    if shmem.rx_head != shmem.rx_tail:
      data = recv_wrapper()
      if data is not None:
        size = len(data)
        for i in xrange(size):
          shmem.mempool_area[shmem.rx_ring[shmem.rx_head].addr + i] = ord(data[i])
        shmem.rx_ring[shmem.rx_head].len = size
        shmem.rx_ring[shmem.rx_head].done = 1
        shmem.rx_head += 1
        if shmem.rx_head == QUEUE_SIZE:
          shmem.rx_head = 0
    else:
      time.sleep(.5)
    if shmem.tx_head != shmem.tx_tail:
      addr = shmem.tx_ring[shmem.tx_head].addr
      size = shmem.tx_ring[shmem.tx_head].len
      data = ''.join((chr(x) for x in shmem.mempool_area[addr:addr+size]))
      sock.sendto(data, (DEVICE,0))
      shmem.tx_ring[shmem.tx_head].done = 1
      shmem.tx_head += 1
      if shmem.tx_head == QUEUE_SIZE:
        shmem.tx_head = 0

if __name__ == '__main__':
  main()
