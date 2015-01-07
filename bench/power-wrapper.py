#!/usr/bin/env python

import socket
import sys
import subprocess

def sub_mod(a, b, mod):
  res = a - b
  if res < 0:
    res += mod
  return res

class Power():
  def __init__(self, host_port):
    host, port = host_port.split(':')
    port = int(port)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    self.file = s.makefile()
    self.prv = [0] * 11

  def get_power(self):
    self.file.write('\n')
    self.file.flush()
    line = map(float, self.file.readline().split())
    dt = line[0] - self.prv[0]
    res = sub_mod(line[1], self.prv[1], line[5]) / dt, \
          sub_mod(line[2], self.prv[2], line[5]) / dt, \
          sub_mod(line[3], self.prv[3], line[5]) / dt, \
          sub_mod(line[4], self.prv[4], line[5]) / dt, \
          sub_mod(line[6], self.prv[6], line[10]) / dt, \
          sub_mod(line[7], self.prv[7], line[10]) / dt, \
          sub_mod(line[8], self.prv[8], line[10]) / dt, \
          sub_mod(line[9], self.prv[9], line[10]) / dt
    self.prv = line
    return res

def line(power, args):
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
  while True:
    proc_line = proc.stdout.readline()
    if len(proc_line) == 0:
      break
    sys.stdout.write(proc_line[:-1])
    print ' %f %f %f %f %f %f %f %f' % power.get_power()
    sys.stdout.flush()

def one(power, args):
  power.get_power()
  subprocess.call(args)
  print 'Avg. power (W) = %f %f %f %f %f %f %f %f' % power.get_power()
  sys.stdout.flush()

def main():
  mode = sys.argv[1]
  power = Power(sys.argv[2])
  args = sys.argv[3:]
  if mode == '--line':
    line(power, args)
  elif mode == '--one':
    one(power, args)
  else:
    print >>sys.stderr, '%s: error' % sys.argv[0]

if __name__ == "__main__":
  main()
