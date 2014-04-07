#!/usr/bin/env python

import struct
import sys
import time

def rdmsr(cpu, address):
  try:
    f = open('/dev/cpu/%d/msr' % cpu, 'rb')
    f.seek(address)
    data = f.read(8)
    value = struct.unpack('Q', data)[0]
    f.close()
    return value
  except IOError, e:
    print >>sys.stderr, 'Warning: %s' % e
    return 0

class Power:
  def __init__(self, cpu):
    self.cpu = cpu
    self.energy_unit = self.get_energy_unit(cpu)
    self.ofs = 0
    self.prv = 0

  def measure(self):
    MSR_PKG_ENERY_STATUS = 1553
    return self.read_energy(MSR_PKG_ENERY_STATUS)

  def read_energy(self, address):
    val = rdmsr(self.cpu, address)
    if val < self.prv:
      self.ofs += 2**32
    self.prv = val
    return (val+self.ofs) * self.energy_unit

  def get_energy_unit(self, cpu):
    MSR_RAPL_POWER_UNIT = 1542
    ENERGY_UNIT_MASK = 0x1F00
    ENERGY_UNIT_OFFSET = 0x08

    val = rdmsr(0, MSR_RAPL_POWER_UNIT)
    energy = 1.0 / (1 << ((val & ENERGY_UNIT_MASK) >> ENERGY_UNIT_OFFSET))
    return energy

def main():
  power = [Power(i) for i in xrange(0, 2)]
  while True:
    print ' '.join(['%f' % x.measure() for x in power])
    sys.stdout.flush()
    time.sleep(1)

if __name__ == '__main__':
  main()
