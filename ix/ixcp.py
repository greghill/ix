#!/usr/bin/env python

import argparse
import ctypes
import mmap
import os
import os.path
import posix_ipc
import sys
import time

BITS_PER_LONG = 64
NCPU = 128
ETH_MAX_NUM_FG = 128
NETHDEV = 16
ETH_MAX_TOTAL_FG = ETH_MAX_NUM_FG * NETHDEV
IDLE_FIFO_SIZE = 256

class CpuMetrics(ctypes.Structure):
  _fields_ = [
    ('queuing_delay', ctypes.c_double),
    ('batch_size', ctypes.c_double),
    ('padding', ctypes.c_byte * 48),
  ]

class FlowGroupMetrics(ctypes.Structure):
  _fields_ = [
    ('cpu', ctypes.c_uint),
    ('padding', ctypes.c_byte * 60),
  ]

class CmdParamsMigrate(ctypes.Structure):
  _fields_ = [
    ('fg_bitmap', ctypes.c_ulong * (ETH_MAX_TOTAL_FG / BITS_PER_LONG)),
    ('cpu', ctypes.c_uint),
  ]

class CmdParamsIdle(ctypes.Structure):
  _fields_ = [
    ('fifo', ctypes.c_char * IDLE_FIFO_SIZE),
  ]

class CommandParameters(ctypes.Union):
  _fields_ = [
    ('migrate', CmdParamsMigrate),
    ('idle', CmdParamsIdle),
  ]

class Command(ctypes.Structure):
  CP_CMD_NOP = 0
  CP_CMD_MIGRATE = 1
  CP_CMD_IDLE = 2

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
    ('nr_cpus', ctypes.c_uint),
    ('padding', ctypes.c_byte * 56),
    ('cpu_metrics', CpuMetrics * NCPU),
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
  cmd.cmd_params.migrate.fg_bitmap = (ctypes.c_ulong * len(bitmap))(*bitmap)
  cmd.cmd_params.migrate.cpu = target_cpu
  cmd.status = Command.CP_STATUS_RUNNING
  cmd.cmd_id = Command.CP_CMD_MIGRATE
  while cmd.status != Command.CP_STATUS_READY:
    pass

def get_fifo(cpu):
  return os.path.abspath('block-%d.fifo' % cpu)

def is_idle(cpu):
  return os.path.exists(get_fifo(cpu))

def idle(shmem, cpu):
  if is_idle(cpu):
    return
  fifo = get_fifo(cpu)
  os.mkfifo(fifo)

  cmd = shmem.command[cpu]
  assert len(fifo) + 1 < IDLE_FIFO_SIZE, fifo
  cmd.cmd_params.idle.fifo = fifo
  cmd.status = Command.CP_STATUS_RUNNING
  cmd.cmd_id = Command.CP_CMD_IDLE
  while cmd.status != Command.CP_STATUS_READY:
    pass

def wake_up(cpu):
  if not is_idle(cpu):
    return
  fifo = get_fifo(cpu)
  fd = os.open(fifo, os.O_WRONLY)
  os.write(fd, '1')
  os.close(fd)
  os.remove(fifo)

def set_nr_cpus(shmem, fg_per_cpu, cpus, verbose = False):
  fgs_per_cpu = int(shmem.nr_flow_groups / cpus)
  one_more_fg = shmem.nr_flow_groups % cpus

  times = []
  start = 0
  for target_cpu in xrange(cpus):
    wake_up(target_cpu)
    count = fgs_per_cpu
    if target_cpu < one_more_fg:
      count += 1
    target_fgs = range(start, start + count)
    start += count

    for source_cpu in xrange(NCPU):
      if source_cpu == target_cpu:
        continue

      intersection = set(target_fgs).intersection(set(fg_per_cpu[source_cpu]))
      if len(intersection) == 0:
        continue
      #print 'migrate from %d to %d fgs %r' % (source_cpu, target_cpu, list(intersection))
      start_time = time.time()
      migrate(shmem, source_cpu, target_cpu, list(intersection))
      stop_time = time.time()
      if verbose:
        sys.stdout.write('.')
        sys.stdout.flush()
      fg_per_cpu[source_cpu] = list(set(fg_per_cpu[source_cpu]) - intersection)
      fg_per_cpu[target_cpu] = list(set(fg_per_cpu[target_cpu]) | intersection)
      times.append((stop_time - start_time) * 1000)
  if verbose:
    print
    if len(times) > 0:
      print 'migration duration min/avg/max = %f/%f/%f ms' % (min(times), sum(times)/len(times), max(times))
  for cpu in xrange(shmem.nr_cpus):
    if len(fg_per_cpu[cpu]) == 0:
      idle(shmem, cpu)

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

  """
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
  """

  parser = argparse.ArgumentParser()
  parser.add_argument('--single-cpu', action='store_true')
  parser.add_argument('--cpus', type=int)
  parser.add_argument('--idle', type=int)
  parser.add_argument('--wake-up', type=int)
  parser.add_argument('--show-metrics', action='store_true')
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
  elif args.cpus is not None:
    set_nr_cpus(shmem, fg_per_cpu, args.cpus, verbose = True)
  elif args.idle is not None:
    idle(shmem, args.idle)
  elif args.wake_up is not None:
    wake_up(args.wake_up)
  elif args.show_metrics:
    for cpu in xrange(shmem.nr_cpus):
      print 'CPU %d: queuing delay: %d us, batch size: %d pkts' % (cpu, shmem.cpu_metrics[cpu].queuing_delay, shmem.cpu_metrics[cpu].batch_size)

if __name__ == '__main__':
  main()
