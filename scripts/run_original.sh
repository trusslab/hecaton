qemu-system-x86_64 -smp 2 -m 4G -enable-kvm -cpu host \
    -net nic -net user,hostfwd=tcp::10022-:22 \
    -kernel original_kernels/$1/arch/x86/boot/bzImage -nographic \
    -device virtio-scsi-pci,id=scsi \
    -device scsi-hd,bus=scsi.0,drive=d0 \
    -drive file=images/stretch.img,format=raw,if=none,id=d0 \
    -append "root=/dev/sda console=ttyS0 earlyprintk=serial \
      oops=panic panic_on_warn=1 panic=86400 kvm-intel.nested=1 \
      security=apparmor ima_policy=tcb workqueue.watchdog_thresh=140 \
      nf-conntrack-ftp.ports=20000 nf-conntrack-tftp.ports=20000 \
      nf-conntrack-sip.ports=20000 nf-conntrack-irc.ports=20000 \
      nf-conntrack-sane.ports=20000 vivid.n_devs=16 \
      vivid.multiplanar=1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2 \
      spec_store_bypass_disable=prctl nopcid"
