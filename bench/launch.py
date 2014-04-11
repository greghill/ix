#!/usr/bin/env python

import argparse
import getpass
import logging
import os
import select
import subprocess
import sys
import time

class RemoteExec():
  def __init__(self, user, host, exec_cmdline):
    self.user = user
    self.host = host
    self.exec_cmdline = exec_cmdline
    self.proc = None

  def execute(self):
    self.proc = subprocess.Popen(['ssh', '%s@%s' % (self.user,self.host), self.exec_cmdline], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.stderr = self.proc.stderr
    self.stdout = self.proc.stdout

def cat(src, dst):
  data = os.read(src.fileno(), 4096)
  if len(data) == 0:
    return True
  os.write(dst.fileno(), data)
  dst.flush()
  return False

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--clients', required=True, help='comma-separated list of hostnames')
  parser.add_argument('--client-cmdline', required=True, help='the command line to execute at each client')
  parser.add_argument('--debug', action='store_true', help='enable debug log')
  parser.add_argument('--time', type=int, default=60, help='time of benchmark execution in seconds')
  parser.add_argument('--user', default=getpass.getuser(), help='SSH username for connecting to the clients')
  args = parser.parse_args()
  args.clients = args.clients.split(',')

  if args.debug:
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s:%(levelname)s:%(message)s')

  clients = {}
  for client_host in args.clients:
    clients[client_host] = RemoteExec(args.user, client_host, args.client_cmdline)

  fds = []

  logging.debug('before execute')
  for id in args.clients:
    clients[id].execute()
    fds.append(clients[id].stdout)
    fds.append(clients[id].stderr)
  logging.debug('after execute')

  def check_and_cat(title, fd):
    if fd in r:
      sys.stdout.write('\033[1m%s: \033[0m' % title)
      sys.stdout.flush()
      if cat(fd, sys.stdout):
        sys.stdout.write('-closed-\n')
        sys.stdout.flush()
        fds.remove(fd)

  stats = {}
  timestamps = {}
  record = 'start'
  timestamps['start'] = time.time()
  while time.time() - timestamps['start'] < args.time:
    r, _, _ = select.select(fds, [], [], 1)

    for i in clients:
      if clients[i].stdout in r:
        line = clients[i].stdout.readline()
        if len(line) == 0:
          fds.remove(clients[i].stdout)
        else:
          data = [int(x) for x in line.split()]
          now = time.time()
          if i not in stats:
            stats[i] = {'start': None, 'stop': None}
          stats[i][record] = data
          timestamps[record] = time.time()
          if record == 'start' and len(stats) == len(clients):
            logging.debug('all clients responded')
            record = 'stop'
      check_and_cat('%s-stderr' % i, clients[i].stderr)
  logging.debug('loop done')

  duration = timestamps['stop'] - timestamps['start']

  msg_per_sec = 0
  for i in sorted(stats):
    data = stats[i]
    msg_per_sec += data['stop'][1] - data['start'][1]
  msg_per_sec /= duration

  print '%d' % (msg_per_sec,)

  for i in clients:
    clients[i].proc.terminate()

  for i in clients:
    clients[i].proc.wait()

if __name__ == '__main__':
  main()
