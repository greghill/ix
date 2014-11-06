#!/usr/bin/env python

import ctypes
import mmap
import posix_ipc
import sys
import time

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

class CmdParamsMigrateFlowGroup(ctypes.Structure):
  _fields_ = [
    ('flow', ctypes.c_uint),
    ('cpu', ctypes.c_uint),
  ]

class CommandParameters(ctypes.Union):
  _fields_ = [
    ('migrate_flow_group', CmdParamsMigrateFlowGroup)
  ]

class Command(ctypes.Structure):
  CP_CMD_NOP = 0
  CP_CMD_MIGRATE_FLOW_GROUP = 1

  CP_STATUS_NOT_RUNNING = 0
  CP_STATUS_RUNNING = 1

  _fields_ = [
    ('cmd_id', ctypes.c_uint),
    ('status', ctypes.c_uint),
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
     print '%d' % shmem.command[i].status,
  print
  if len(sys.argv) >= 2 and sys.argv[1] == '--single-cpu':
    for i in xrange(128):
      cmd = shmem.command[shmem.flow_group[i].cpu]
      cmd.cmd_params.migrate_flow_group.flow = i
      cmd.cmd_params.migrate_flow_group.cpu = 0
      cmd.cmd_id = Command.CP_CMD_MIGRATE_FLOW_GROUP
      cmd.status = Command.CP_STATUS_RUNNING
      sys.stdout.write('.')
      sys.stdout.flush()
      while cmd.status != Command.CP_STATUS_NOT_RUNNING:
        time.sleep(0.01)
    print

if __name__ == '__main__':
  main()
