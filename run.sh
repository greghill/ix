aklog
sudo aklog
sudo insmod ./dune/dune.ko
sudo sh -c "echo '2' > /sys/devices/pci0000\:00/0000\:00\:01.0/0000\:01\:00.0/sriov_numvfs"
sudo ip link set p1p1 vf 0 mac e6:14:c6:39:a5:d7
sudo ip link set p1p1 vf 0 spoofchk off
sudo rmmod ixgbevf
#sudo insmod ./igb_stub/igb_stub.ko
sudo sh -c "echo '5000' > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages"
