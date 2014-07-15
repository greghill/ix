#!/usr/bin/env python

import ctypes
import mmap
import posix_ipc

class QueueMetrics(ctypes.Structure):
  _fields_ = [
    ('depth', ctypes.c_uint),
    ('padding', ctypes.c_byte * 60),
  ]

class FlowGroupMetrics(ctypes.Structure):
  _fields_ = [
    ('depth', ctypes.c_uint),
    ('padding', ctypes.c_byte * 60),
  ]

class ShMem(ctypes.Structure):
  _fields_ = [
    ('queue', QueueMetrics * 64),
    ('flow_group', FlowGroupMetrics * 128),
  ]

def main():
  shm = posix_ipc.SharedMemory('/ix', 0)
  buffer = mmap.mmap(shm.fd, ctypes.sizeof(ShMem), mmap.MAP_SHARED, mmap.PROT_WRITE)
  shmem = ShMem.from_buffer(buffer)
  for i in xrange(128):
    print shmem.flow_group[i].depth,
  print

if __name__ == '__main__':
  main()
