OBJDUMP=/usr/bin/objdump
VMLINUX=modified_kernels/hecaton_kernel/vmlinux
$OBJDUMP -d $VMLINUX > scripts/vmlinux.txt

python3 scripts/parse_vmlinux.py scripts/vmlinux.txt bugs/$1/functions.txt scripts/hecaton_data.h
cp scripts/hecaton_data.h modified_kernels/hecaton_kernel/kernel/hecaton_data.h
