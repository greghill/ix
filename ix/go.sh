# invoke as: . go.sh

export DPDK_SDK=/home/prekas/nfs/dpdk
export DUNE_CTL_PATH=/home/prekas/dune/control

# prepare

sudo sh -c 'echo 1000 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'
sudo mkdir -p /mnt/huge
( mount | grep mnt/huge > /dev/null ) || sudo mount -t hugetlbfs nodev /mnt/huge
sudo chown `id -u`:`id -g` /mnt/huge
sudo rmmod ixgbe igb_uio 2>/dev/null
sudo modprobe uio
sudo insmod $DPDK_SDK/build/kmod/igb_uio.ko
sudo chown `id -u`:`id -g` /dev/uio*
mkdir -p $DUNE_CTL_PATH
sudo bash <<EOF
mkdir -p /dev/cpuset
( mount | grep dev/cpuset > /dev/null ) || mount -t cpuset cpuset /dev/cpuset
for i in {0..31}; do mkdir -p /dev/cpuset/\$i; sh -c "echo \$i > /dev/cpuset/\$i/cpuset.cpus; echo 0-1 > /dev/cpuset/\$i/cpuset.mems"; done
for i in \`cat /dev/cpuset/tasks\`; do sh -c "echo \$i > /dev/cpuset/0/tasks || true"; done
EOF
sudo chown `id -u`:`id -g` /dev/cpuset/*/tasks

# run

rm -r $DUNE_CTL_PATH/*
./ix -c 1 -n 1 &
sleep 5
echo "echo 1 > $DUNE_CTL_PATH/planes/$!/cpuset"
