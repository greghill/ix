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
    ('cpu', ctypes.c_uint),
    ('padding', ctypes.c_byte * 56),
  ]

class CommandParameters(ctypes.Union):
  _fields_ = [
  ]

class Command(ctypes.Structure):
  CP_CMD_NOP = 0

  _fields_ = [
    ('cmd_id', ctypes.c_uint),
    ('cmd_params', CommandParameters),
  ]

class ShMem(ctypes.Structure):
  _fields_ = [
    ('queue', QueueMetrics * 64),
    ('flow_group', FlowGroupMetrics * (16 * 128)),
    ('command', Command * 128),
  ]

def main():
  shm = posix_ipc.SharedMemory('/ix', 0)
  buffer = mmap.mmap(shm.fd, ctypes.sizeof(ShMem), mmap.MAP_SHARED, mmap.PROT_WRITE)
  shmem = ShMem.from_buffer(buffer)
  print 'flow group assignments = ',
  for i in xrange(128):
    print shmem.flow_group[i].cpu,
  print
  print 'commands running = ',
  for i in xrange(16):
     print '%d' % shmem.command[i].cmd_id,
  print

if __name__ == '__main__':
  main()
