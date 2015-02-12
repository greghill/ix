aklog
sudo aklog
sudo insmod ./dune/dune.ko
sudo sh -c "echo '6' > /sys/devices/pci0000\:00/0000\:00\:04.0/subsystem/devices/0000\:04\:00.0/sriov_numvfs"
sudo ip link set p3p1 vf 0 mac e6:14:c6:39:a5:d9
sudo ip link set p3p1 vf 0 spoofchk off
sudo ip link set p3p1 vf 1 mac e6:14:c6:39:a5:da
sudo ip link set p3p1 vf 1 spoofchk off
sudo ip link set p3p1 vf 2 mac e6:14:c6:39:a5:db
sudo ip link set p3p1 vf 2 spoofchk off
sudo ip link set p3p1 vf 3 mac e6:14:c6:39:a5:dc
sudo ip link set p3p1 vf 3 spoofchk off
sudo ip link set p3p1 vf 4 mac e6:14:c6:39:a5:dd
sudo ip link set p3p1 vf 4 spoofchk off
sudo ip link set p3p1 vf 5 mac e6:14:c6:39:a5:df
sudo ip link set p3p1 vf 5 spoofchk off
#sudo rmmod ixgbevf
#sudo rmmod ixgbe
#sudo insmod ./igb_stub/igb_stub.ko
sudo sh -c "echo '50000' > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages"
