#!/usr/bin/env python
"""
Server:
$ sudo ./ix -d 0000:42:00.1 -c 1 /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 ../apps/tcp/ix_pingpongs 8
Client:
$ sudo iptables -t raw -A OUTPUT -p tcp --dport 8000 -j DROP
$ sudo ./tcp_ooseq.py 192.168.21.1 8000 8
$ sudo iptables -t raw -F
"""

import random
import sys
import time
from scapy.all import *

dst = sys.argv[1]
tcp_port = int(sys.argv[2])
msg_size = int(sys.argv[3])

sport = random.randint(1024,60000)

synack = sr1(IP(dst=dst)/TCP(sport=sport,dport=tcp_port,flags='S'))
send(IP(dst=dst)/TCP(sport=sport,dport=tcp_port,flags='A',seq=1,ack=synack.seq+1))

data1 = '0' * int(msg_size / 2)
data2 = '1' * (msg_size - int(msg_size / 2))
send(IP(dst=dst)/TCP(sport=sport,dport=tcp_port,flags='A',seq=1+len(data1),ack=synack.seq+1)/data2)
time.sleep(1)
send(IP(dst=dst)/TCP(sport=sport,dport=tcp_port,flags='A',seq=1,ack=synack.seq+1)/data1)
response = sniff(filter='tcp and host %s and port %d' % (dst, tcp_port), count=1)
response = response[0]
send(IP(dst=dst)/TCP(sport=sport,dport=tcp_port,flags='R',seq=1+len(data1)))
assert str(response.payload.payload.payload) == data1+data2, '%r' % response
print 'OK'
