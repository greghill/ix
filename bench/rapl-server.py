#!/usr/bin/env python

import SocketServer
import sys
import time

sys.path.append('/home/prekas/epfl1/prg/intel_power_measure')
import intel_power_measure

class Power():
  MSR_PKG_ENERGY_STATUS  = 0x00000611
  MSR_DRAM_ENERGY_STATUS = 0x00000619
  MSR_PP0_ENERGY_STATUS  = 0x00000639
  MSR_PP1_ENERGY_STATUS  = 0x00000641
  power_obj0 = intel_power_measure.Power(0)
  power_obj1 = intel_power_measure.Power(1)

  def read(self):
    return self.power_obj0.read_energy(self.MSR_PKG_ENERGY_STATUS), \
           self.power_obj0.read_energy(self.MSR_DRAM_ENERGY_STATUS), \
           self.power_obj0.read_energy(self.MSR_PP0_ENERGY_STATUS), \
           0, \
           2**32 * self.power_obj0.energy_unit, \
           self.power_obj1.read_energy(self.MSR_PKG_ENERGY_STATUS), \
           self.power_obj1.read_energy(self.MSR_DRAM_ENERGY_STATUS), \
           self.power_obj1.read_energy(self.MSR_PP0_ENERGY_STATUS), \
           0, \
           2**32 * self.power_obj1.energy_unit

power = Power()

class ThreadedTCPRequestHandler(SocketServer.BaseRequestHandler):
  def handle(self):
    while True:
      data = self.request.recv(1024)
      if len(data) == 0:
        break
      reading = power.read()
      self.request.sendall('%f %f %f %f %f %f %f %f %f %f %f\n' % tuple([time.time()] + list(reading)))

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
  allow_reuse_address = True
  daemon_threads = True

def main():
  if len(sys.argv) > 1 and sys.argv[1] == '--local':
    while True:
      print power.read()
      time.sleep(1)
  server = ThreadedTCPServer(('0.0.0.0', 4242), ThreadedTCPRequestHandler)
  server.serve_forever()

if __name__ == "__main__":
  main()
