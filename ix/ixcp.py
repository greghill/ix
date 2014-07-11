#!/usr/bin/env python

import ctypes
import mmap
import posix_ipc

class QueueMetrics(ctypes.Structure):
  _fields_ = [
    ('depth', ctypes.c_uint),
    ('padding', ctypes.c_byte * 60),
  ]

class ShMem(ctypes.Structure):
  _fields_ = [
    ('queue', QueueMetrics * 64),
  ]

def main():
  shm = posix_ipc.SharedMemory('/ix', 0)
  buffer = mmap.mmap(shm.fd, ctypes.sizeof(ShMem), mmap.MAP_SHARED, mmap.PROT_WRITE)
  shmem = ShMem.from_buffer(buffer)
  for i in xrange(32):
    print shmem.queue[i].depth,
  print

if __name__ == '__main__':
  main()
