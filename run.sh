sudo insmod ./dune/dune.ko
sudo sh -c "echo '2' > /sys/devices/pci0000\:00/0000\:00\:01.0/0000\:01\:00.0/sriov_numvfs"
#sudo rmmod ixgbe
#sudo insmod/igb_stub/igb_stub.ko
sudo sh -c "echo '5000' > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages"
