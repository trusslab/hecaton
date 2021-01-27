# Undo Workarounds for Kernel Bugs!

This document is a step-by-step guide on deploying Hecaton on a system, and test Undo Workarounds for Linux kernel bugs.
In this document, we show how to deploy Hecaton to insert bowknots for x86 Linux kernel bugs. 
To illustrate the usage of Hecaton on real bugs, we included three real bugs found by the Syzbot system.  Bellow, we show how one can use Hecaton to insert undo-workarounds to these bugs.(*) 

We tested these instructions on a machine running Ubuntu 16.04.3 LTS running on a 36 cores Intel Xeon processor with 131GB of RAM.   To run Hecaton on a system, you need at least 100GB of free disk space. In addition, you need to have sudo access to the machine. 
Please note that  Hecaton's workflow includes building Linux kernels several times. So, we suggest running it on a powerful machine. Otherwise, it might take several hours. 

We test this instruction while we used following versions of third-party programs:
QEMU emulator version 2.5.0, 
GNU objdump (GNU Binutils for Debian) 2.28

(*) To reproduce each Syzbot bug, you need to use the exact same kernel distribution, kernel config, kernel commit, compiler version, and userspace image as the ones used by the Syzbot instance which found the bug.
 We have already injected bowknots to 30 real bugs and reported the result in our paper.  To do so, we used several different kernel branches, compilers, and userspace images. Including all of them would have made our repository unnecessarily large.  As a result, without losing the generality, we focused on three of the bugs that are reproducible by the same configurations in this document. However, our instruction works for any arbitrary bug.
 
## Downloading the source
First, you need to clone our repository (which includes Hecaton's source code and place-holders for third party components) to your machine.
```bash
WD = "working directory to install Hecaton"
cd $WD 
git clone https://github.com/trusslab/hecaton.git Hecaton
```
Hecaton's repo consists of several sub-folders naming:  `bugs`,  `compilers`,  `database`,  `Hecaton_source`  `images`,  `modified_kernels` , `original_kernels`,   `scripts`.

In `bugs`, we prepared three Syzbot bugs to be processed by Hecaton. For each bug, we have a sub-folder such as `bugs/bug1`. In each bug's directory, we provide several files. `syzbot_link.txt` contains a link to the Syzbot page of the bug. `call_trace.txt` is the stack trace of the bug's crash report. `functions.txt` contains the list of functions that need to be instrumented, and `directories.txt` lists these functions' locations in the kernel. Finally, poc.c is a source code for proof-of-concept code that triggers the bug. 
We keep the function-pair database of the kernel in the `database` folder.
`Hecaton_source` contains the source code of our static analyzer.
`scripts` contains several helper scripts, which helps in preparing, building, and testing Hecaton.

We populate other folders in the following steps:

 We need to clone the Linux kernel instrumented with Hecaton's exception handler into the `original_kernels`.
```bash
cd $WD/Hecaton/original_kernels 
git clone https://gitlab.com/mohammadjavad/linux_next.git linux-next
```

Next, you need to download the exact GCC compiler that Syzbot used on the kernel that triggered the bugs (otherwise, the bugs are not going to reproduced)
```bash
cd $WD/Hecaton/compilers
wget https://storage.googleapis.com/syzkaller/gcc-10.1.0-syz.tar.xz
tar xvf gcc-10.1.0-syz.tar.xz
rm  gcc-10.1.0-syz.tar.xz
```

Then you need to download a clang compiler for our static analysis tool
```bash
cd $WD/Hecaton/compilers
wget https://storage.googleapis.com/syzkaller/clang-11-prerelease-ca2dcbd030e.tar.xz
tar xvf clang-11-prerelease-ca2dcbd030e.tar.xz
rm clang-11-prerelease-ca2dcbd030e.tar.xz
```

Then you need to download the exact same QEMU-suitable Debian Stretch image that Syzbot used  from here:
```bash
cd $WD/Hecaton/images
wget https://storage.googleapis.com/syzkaller/stretch.img
wget https://storage.googleapis.com/syzkaller/stretch.img.key
sudo chmod 0600 stretch.img.key
```
## Preparing Hecaton

### General Preparation
First, we prepare the kernels we want to work on. We need to make three copies of the target kernel. In this document, we work on the linux-next kernel. The following script makes three copies of the original kernel in the `modified_kernels` sub-directory.
```bash
cd $WD/Hecaton
source scripts/copy_kernels.sh original_kernels/linux-next
```
(If you want to use Hecaton for another Linux kernel branch later, you need to clone it to the original_kernels, patch it with Hecaton's exception handler and replace linux-next with its name in the above command.)



### Per-bug Preparation
```bash
cd $WD/Hecaton
source scripts/reset_environment.sh
```
To prepare Hecaton for inserting bowknots to functions related to a bug, we use the following script:
```bash
cd $WD/Hecaton
source scripts/prepare.sh bug1
```
(If you want to test another bug, you need to replace bug1 name with its name, for example, bug2).
The above command serves several functionalities. It lets the static analyzer know which functions to modify, It compiles the PoC of the bug1, and it compiles three clang plug-ins,` hecaton_pass1.so`, `hecaton_pass2.so`, and `hecaton_database.so`.
The first plug-in `hecaton_pass1.so`, preprocesses the functions and makes them ready for bowknot generation. The second plug-in, `hecaton_pass2.so`, instruments the functions with bowknots. We use the third plug-in `hecaton_database.so` for function pair database generation (**). 


(**) As we mentioned in our paper, function pair database generation requires a manual phase. As a result, we included the final database of function pairs for the linux-next kernel in this repo in `database/database_header.h`

## Running Hecaton
As we mentioned, Hecaton has two passes of static analysis. The first pass preprocesses the functions and makes them ready for bowknot generation. This pass processes the kernel resides in `modified_kernels/baseline_kernel` and write its output to `modified_kernels/pass1_kernel`. To run Hecaton's first pass, please use the following commands.

```bash
cd $WD/Hecaton
source scripts/hecaton_pass1.sh
```

After the first pass is done, you need to run hecaton's second pass, which inserts bowknots to the functions. This pass processes the kernel resides in `modified_kernels/pass1_kernel` and write its output to `modified_kernels/hecaton_kernel`. To run Hecaton's second pass, please use the following commands.
```bash
cd $WD/Hecaton
source scripts/hecaton_pass2.sh
```
As stated in the paper, Hecaton also provides confidence scores for the instrumented functions. Hecaton stores the confident scores in the `bugs/bug1/score.txt` 

Now the modified kernel is ready, and you need to build it with a normal `GCC` compiler. To do so, please run the following commands: 
```bash
cd $WD/Hecaton
source scripts/build_hecaton_kernel.sh
```
Now that you have built the modified kernel, you need to update the `hecaton_data.h` with correct addresses for modified functions. To do so, please run:
```bash
cd $WD/Hecaton
source scripts/update_hecaton_data.sh bug1
```
And then, you need to build the modified kernel again.
```bash
cd $WD/Hecaton
source scripts/build_hecaton_kernel.sh
```
## Testing Hecaton
To test Hecaton,  we first test the PoC of the bug on the unmodified kernel and see that it crashes the kernel. Then we run the PoC on the instrumented kernel to check the inserted bowknot.
Before running the unmodified kernel, you need to build it. You can use the following command to do it.
```bash
cd $WD/Hecaton
source scripts/build_original_kernel.sh linux-next
```
To run the **original** kernel in QEMU (You need to have sudo access because of KVM):
```bash
cd $WD/Hecaton
sudo bash scripts/run_original.sh linux-next
```
After the kernel completely boots and prompt “syzkaller login:” you can open another window in your host machine and use the following command to ssh into it:
```bash
cd $WD/Hecaton
source scripts/ssh_to_qemu.sh
```
To copy the PoC of the bug to the image, use:
```bash
cd $WD/Hecaton
source scripts/cp_poc.sh bug1
```
To run the PoC,  use:
```bash
cd $WD/Hecaton
source scripts/run_poc.sh bug1
```
You can see the PoC crashes the original kernel.

To run the **modified** kernel in QEMU, (You need to have sudo access because of KVM):

```bash
cd $WD/Hecaton
sudo bash scripts/run_hecaton.sh 
```

After the kernel completely boots and prompt “syzkaller login:” you can open another window in your host machine and use the following command to ssh into it:
```bash
cd $WD/Hecaton
source scripts/ssh_to_qemu.sh
```

To copy the PoC of the bug to the image, use:

```bash
cd $WD/Hecaton
source scripts/cp_poc.sh bug1
```

To run the PoC, use:

```bash
cd $WD/Hecaton
source scripts/run_poc.sh bug1
```

You can see that the system does not crash, and Hecaton bowknots undo the half executed functions. Without the Hecaton bowknot cleaning up the effect of half executed syscall, the buggy module (tty in case of bug1) would become nonfunctional. However, you can see Hecaton bowknots not only prevent crashing but also keep the functionality of the system. (in case of bug1 the terminal still works; you can type root and then enter in the QEMU window and see it prompt for a password, you can also still ssh to the system).

## Testing other bugs
To test other bugs, repeat all the steps from  **Per-bug Preparation** this time with another bug name, such as bug2; please make sure to replace bug1 for all the following instructions as well.

 








