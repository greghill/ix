#!/usr/bin/env python

import sys

def main():
  xcol = sys.argv[1]
  ycol = sys.argv[2]
  id = sys.argv[3]
  xcol = int(xcol) - 1
  ycol = int(ycol) - 1
  id = map(lambda x: int(x) - 1, id.split(','))
  data = []
  for line in sys.stdin:
    line = line.strip().split()
    data.append((float(line[xcol]), float(line[ycol]), ','.join(str(line[x]) for x in id)))
  in_pareto = set(range(len(data)))
  for i in xrange(len(data)):
    if i not in in_pareto:
      continue
    ok = True
    for j in in_pareto:
      if i == j:
        continue
      if data[i][0] < data[j][0] and data[i][1] > data[j][1]:
        ok = False
        break
    if not ok:
      in_pareto.remove(i)
  in_pareto = sorted(map(lambda i: data[i], in_pareto))
  for e in in_pareto:
    print '%f %f # %s' % (e[0], e[1], e[2])

if __name__ == '__main__':
  main()
