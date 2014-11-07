#!/usr/bin/env python

import argparse
import ctypes
import mmap
import posix_ipc
import sys
import time

BITS_PER_LONG = 64
NCPU = 128
ETH_MAX_NUM_FG = 128
NETHDEV = 16
ETH_MAX_TOTAL_FG = ETH_MAX_NUM_FG * NETHDEV

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
    ('fg_bitmap', ctypes.c_ulong * (ETH_MAX_TOTAL_FG / BITS_PER_LONG)),
    ('cpu', ctypes.c_uint),
  ]

class CommandParameters(ctypes.Union):
  _fields_ = [
    ('migrate_flow_group', CmdParamsMigrateFlowGroup)
  ]

class Command(ctypes.Structure):
  CP_CMD_NOP = 0
  CP_CMD_MIGRATE = 1

  CP_STATUS_READY = 0
  CP_STATUS_RUNNING = 1

  _fields_ = [
    ('cmd_id', ctypes.c_uint),
    ('status', ctypes.c_uint),
    ('cmd_params', CommandParameters),
  ]

class ShMem(ctypes.Structure):
  _fields_ = [
    ('nr_flow_groups', ctypes.c_uint),
    ('padding', ctypes.c_byte * 60),
    ('queue', QueueMetrics * 64),
    ('flow_group', FlowGroupMetrics * ETH_MAX_TOTAL_FG),
    ('command', Command * NCPU),
  ]

def bitmap_create(size, on):
  bitmap = [0] * (size / BITS_PER_LONG)

  for pos in on:
    bitmap[pos / BITS_PER_LONG] |= 1 << (pos % BITS_PER_LONG)

  return bitmap

def migrate(shmem, source_cpu, target_cpu, flow_groups):
  cmd = shmem.command[source_cpu]
  bitmap = bitmap_create(ETH_MAX_TOTAL_FG, flow_groups)
  cmd.cmd_params.migrate_flow_group.fg_bitmap = (ctypes.c_ulong * len(bitmap))(*bitmap)
  cmd.cmd_params.migrate_flow_group.cpu = target_cpu
  cmd.cmd_id = Command.CP_CMD_MIGRATE
  cmd.status = Command.CP_STATUS_RUNNING
  while cmd.status != Command.CP_STATUS_READY:
    time.sleep(0.01)

def main():
  shm = posix_ipc.SharedMemory('/ix', 0)
  buffer = mmap.mmap(shm.fd, ctypes.sizeof(ShMem), mmap.MAP_SHARED, mmap.PROT_WRITE)
  shmem = ShMem.from_buffer(buffer)
  fg_per_cpu = {}
  for i in xrange(NCPU):
    fg_per_cpu[i] = []
  for i in xrange(shmem.nr_flow_groups):
    cpu = shmem.flow_group[i].cpu
    fg_per_cpu[cpu].append(i)
  print 'flow group assignments:'
  for cpu in xrange(NCPU):
    if len(fg_per_cpu[cpu]) == 0:
      continue
    print '  CPU %02d: flow groups %r' % (cpu, fg_per_cpu[cpu])
  print 'commands running at:',
  empty = True
  for i in xrange(NCPU):
    if shmem.command[i].status != Command.CP_STATUS_READY:
      print 'CPU %02d,' % i,
      empty = False
  if empty:
    print 'none',
  print

  parser = argparse.ArgumentParser()
  parser.add_argument('--single-cpu', action='store_true')
  args = parser.parse_args()

  if args.single_cpu:
    target_cpu = 0
    for cpu in xrange(NCPU):
      if cpu == target_cpu:
        continue
      migrate(shmem, cpu, target_cpu, fg_per_cpu[cpu])
      sys.stdout.write('.')
      sys.stdout.flush()
    print

if __name__ == '__main__':
  main()
