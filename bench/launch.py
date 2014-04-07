#!/usr/bin/env python

import argparse
import os
import select
import subprocess
import sys
import time

class RemoteExec():
  def __init__(self, user, host, files):
    self.user = user
    self.host = host
    self.files = files
    self.directory = 'rexec/%s' % self.host
    self.proc = None

  def deploy(self):
    proc = subprocess.Popen(['ssh', '%s@%s' % (self.user,self.host), 'mkdir -p %s' % self.directory], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    assert stdout == '', stdout
    assert stderr == '', stderr
    args = ['scp'] + self.files + ['%s@%s:%s' % (self.user,self.host, self.directory)]
    proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    assert stdout == '', stdout
    assert stderr == '', stderr

  def execute(self, cmdline):
    self.proc = subprocess.Popen(['ssh', '%s@%s' % (self.user,self.host), 'cd %s && exec %s' % (self.directory, cmdline)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    self.stderr = self.proc.stderr
    self.stdout = self.proc.stdout

class ExecSpec():
  def __init__(self, user, host, files, cmdline):
    self.user = user
    self.host = host
    self.files = files
    self.cmdline = cmdline

def cat(src, dst):
  data = os.read(src.fileno(), 4096)
  if len(data) == 0:
    return True
  os.write(dst.fileno(), data)
  dst.flush()
  return False

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--conf', required=True)
  parser.add_argument('--clients', type=int, required=True)
  parser.add_argument('--with-server', action='store_true')
  parser.add_argument('--deploy', action='store_true')
  parser.add_argument('--server-params', default='')
  parser.add_argument('--client-params', default='')
  args = parser.parse_args()

  CONF = {'ExecSpec': ExecSpec}
  execfile(args.conf, CONF)
  from collections import namedtuple
  del CONF['__builtins__']
  CONF = namedtuple('Struct', CONF.keys())(*CONF.values())

  if args.with_server == False and args.server_params != '':
    print >>sys.stderr, 'Error: --server-params specified but not --with-server.'
    return 1

  server = RemoteExec(CONF.SERVER.user, CONF.SERVER.host, CONF.SERVER.files)
  server_energy = RemoteExec(CONF.ENERGY.user, CONF.ENERGY.host, CONF.ENERGY.files)
  latency = RemoteExec(CONF.LATENCY.user, CONF.LATENCY.host, CONF.LATENCY.files)

  clients = {}
  for i in xrange(args.clients):
    c = CONF.CLIENTS[i]
    clients[c.host] = RemoteExec(c.user, c.host, c.files)

  if args.deploy:
    if args.with_server:
      server.deploy()
      server_energy.deploy()
    latency.deploy()
    for id in clients:
      clients[id].deploy()
    return

  fds = []

  if args.with_server:
    server.execute(CONF.SERVER.cmdline % {'params': args.server_params})
    fds.append(server.stderr)
    server_energy.execute(CONF.ENERGY.cmdline)
    fds.append(server_energy.stdout)
    fds.append(server_energy.stderr)
  else:
    server = None
    server_energy = None

  def execute_latency():
    latency.execute(CONF.LATENCY.cmdline)
    fds.append(latency.stdout)
    fds.append(latency.stderr)
    start_energy = None

  if len(clients) == 0:
    execute_latency()

  for i,id in enumerate(clients):
    clients[id].execute(CONF.CLIENTS[i].cmdline % {'params': args.client_params})
    fds.append(clients[id].stdout)
    fds.append(clients[id].stderr)

  stats = {}
  latencies_count = 0
  latencies = []
  energy = [0, 0]

  def check_and_cat(title, fd):
    if fd in r:
      sys.stdout.write('\033[1m%s: \033[0m' % title)
      sys.stdout.flush()
      if cat(fd, sys.stdout):
        sys.stdout.write('-closed-\n')
        sys.stdout.flush()
        fds.remove(fd)

  print '# remaining_time connections conn_sec msg_sec timeouts_connect timeouts_recv max_active_conn %s latency_count energy0 energy1 errors' % ' '.join(('latency_%d' % x for x in CONF.LATENCIES))

  connections = args.clients * int(args.client_params.split()[1])
  max_active_conn = 0
  start_energy = None
  start = time.time()
  while time.time() - start < 60:
    r, _, _ = select.select(fds, [], [])

    if server is not None:
      check_and_cat('server-stdout', server.stdout)
      check_and_cat('server-stderr', server.stderr)

    if server_energy is not None:
      if server_energy.stdout in r:
        line = server_energy.stdout.readline()
        if len(line) == 0:
          fds.remove(server_energy.stdout)
        else:
          line = line.split()
          energy = [float(x) for x in line]
          if start_energy is None:
            start_energy = energy
          energy = [energy[i] - start_energy[i] for i in xrange(len(energy))]
      check_and_cat('energy-stderr', server_energy.stderr)

    if latency.proc is not None:
      if latency.stdout in r:
        line = latency.stdout.readline()
        if len(line) == 0:
          fds.remove(latency.stdout)
        else:
          line = line.split()
          latencies_count = int(line[0])
          latencies = [float(x) for x in line[1:]]

      check_and_cat('latency-stderr', latency.stderr)

    for i in clients:
      if clients[i].stdout in r:
        line = clients[i].stdout.readline()
        if len(line) == 0:
          fds.remove(clients[i].stdout)
        else:
          data = [int(x) for x in line.split()]
          now = time.time()
          if i not in stats:
            start = now
            stats[i] = {'start': now}
            if len(stats) == len(clients):
              execute_latency()

          stats[i]['stop'] = now
          stats[i]['data'] = data
      check_and_cat('%s-stderr' % i, clients[i].stderr)

    conn_per_sec = 0
    msg_per_sec = 0
    timeouts_connect = 0
    timeouts_recv = 0
    active_conn = 0
    errors = {}
    for j in sorted(stats):
      duration = stats[j]['stop'] - stats[j]['start']
      data = stats[j]['data']
      if duration > 0:
        conn_per_sec += 1.0 * data[0] / duration
        msg_per_sec += 1.0 * data[1] / duration
      active_conn += data[2]
      timeouts_connect += data[3]
      timeouts_recv += data[4]
      for k in xrange(5, len(data), 3):
        key = (data[k], data[k+1])
        if key not in errors:
          errors[key] = 0
        errors[key] += data[k+2]
    latencies_str = ' '.join(('%d' % x for x in latencies))
    max_active_conn = max(max_active_conn, active_conn)
    print '%3d %4d %6d %8d %5d %5d %6d %s %d %d %d %r %s\r' % (60 - time.time() + start, connections, conn_per_sec, msg_per_sec, timeouts_connect, timeouts_recv, max_active_conn, latencies_str, latencies_count, energy[0], energy[1], errors, ' '*10),

  print

  if server is not None:
    server.proc.terminate()
    server.proc.wait()

  for i in clients:
    clients[i].proc.terminate()
    clients[i].proc.wait()

if __name__ == '__main__':
  main()
