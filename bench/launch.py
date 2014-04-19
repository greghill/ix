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
    self.stdin = self.proc.stdin
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

  def check_and_cat(title, fd, r):
    if fd in r:
      sys.stdout.write('\033[1m%s: \033[0m' % title)
      sys.stdout.flush()
      if cat(fd, sys.stdout):
        sys.stdout.write('-closed-\n')
        sys.stdout.flush()
        fds.remove(fd)
      return 1
    return 0

  def get_stdout_from_all():
    ret = {}
    while len(ret) < len(clients):
      r, _, _ = select.select(fds, [], [])

      for i in clients:
        if clients[i].stdout in r:
          line = clients[i].stdout.readline()
          if len(line) == 0:
            print '%s-stdout: -closed-' % i
            sys.exit(1)
          ret[i] = line
        if check_and_cat('%s-stderr' % i, clients[i].stderr, r):
          sys.exit(1)
    return ret

  def get_data():
    now = time.time()
    for i in clients:
      clients[i].stdin.write('\n')
      clients[i].stdin.flush()
    now2 = time.time()

    ret = get_stdout_from_all()
    for i in ret:
      try:
        ret[i] = [int(x) for x in ret[i].split()]
      except ValueError:
        logging.error('%s: Malformed output: %s', i, ret[i])
        sys.exit(1)

    return now, ret

  ret = get_stdout_from_all()
  for i in ret:
    if ret[i] != 'ok\n':
      print '%s-stdout: %s' % (i, ret[i])
      sys.exit(1)

  time.sleep(1)

  failed = False
  try:
    start_time, start_data = get_data()
  except IOError, e:
    if e.errno == 32: # EPIPE
      failed = True
    else:
      raise

  while True:
    if failed:
      timeout = 1
    else:
      timeout = start_time - time.time() + args.time
    r, _, _ = select.select(fds, [], [], timeout)

    if len(r) == 0:
      break

    for i in clients:
      check_and_cat('%s-stdout' % i, clients[i].stdout, r)
      check_and_cat('%s-stderr' % i, clients[i].stderr, r)

    sys.exit(1)

  logging.debug('loop done')

  if failed:
    logging.error('A client has failed and select did not catch it.')
    sys.exit(1)

  stop_time, stop_data = get_data()

  duration = stop_time - start_time

  def calc(column):
    sum = 0
    for i in clients:
      sum += stop_data[i][column] - start_data[i][column]
    return sum / duration

  msg_per_sec = calc(1)
  rx_bytes = calc(5)
  rx_packets = calc(6)
  tx_bytes = calc(7)
  tx_packets = calc(8)
  latency_99 = max(data[9] for data in stop_data.itervalues())

  print '%f %d %d %d %d ' % (msg_per_sec, rx_bytes, rx_packets, tx_bytes, tx_packets),
  print '%d ' % latency_99,

  for i in clients:
    clients[i].proc.terminate()

  for i in clients:
    clients[i].proc.wait()

if __name__ == '__main__':
  main()
