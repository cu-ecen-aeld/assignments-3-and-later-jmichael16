## Faulty kernel module oops message analysis

Assignment 7 consists of using buildroot to build and run several kernel modules from the Linux Device Drivers textbook, based off this [repo](https://github.com/martinezjavier/ldd3). 
The faulty module in misc-modules has a kernel space error. After starting the QEMU instance and running the following command, the oops message below is the result: 

```
# echo "hello_world" > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000046
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
Data abort info:
  ISV = 0, ISS = 0x00000046
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=000000004259a000
[0000000000000000] pgd=00000000425d0003, p4d=00000000425d0003, pud=00000000425d0003, pmd=0000000000000000
Internal error: Oops: 96000046 [#1] SMP
Modules linked in: scull(O) faulty(O) hello(O)
CPU: 0 PID: 152 Comm: sh Tainted: G           O      5.10.7 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc0/0x290
sp : ffffffc010bc3db0
x29: ffffffc010bc3db0 x28: ffffff8002648000
x27: 0000000000000000 x26: 0000000000000000
x25: 0000000000000000 x24: 0000000000000000
x23: 0000000000000000 x22: ffffffc010bc3e30
x21: 00000000004c9940 x20: ffffff8002564900
x19: 000000000000000c x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000
x15: 0000000000000000 x14: 0000000000000000
x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000
x9 : 0000000000000000 x8 : 0000000000000000
x7 : 0000000000000000 x6 : 0000000000000000
x5 : ffffff80025d97b8 x4 : ffffffc008675000
x3 : ffffffc010bc3e30 x2 : 000000000000000c
x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x6c/0x100
 __arm64_sys_write+0x1c/0x30
 el0_svc_common.constprop.0+0x9c/0x1c0
 do_el0_svc+0x70/0x90
 el0_svc+0x14/0x20
 el0_sync_handler+0xb0/0xc0
 el0_sync+0x174/0x180
Code: d2800001 d2800000 d503233f d50323bf (b900003f)
---[ end trace 546afa0e0e5db6a2 ]---
```

We can see by the first line of the oops message that there was likely a page fault. Further, it shows us that the program counter was within the faulty_write function. 
If we take a look at the [code line of interest](https://github.com/cu-ecen-aeld/assignment-7-jmichael16/blob/117fbf20b34fc093e5457008253a505f621f307d/misc-modules/faulty.c#L53),
we can see that there is indeed a NULL pointer dereference. If we needed any more assurance that this was our issue, we could issue the objdump command below to get the assembly of the 
source .ko file:

```
[~/aesd/assignments-buildroot]$ ./buildroot/output/host/bin/aarch64-linux-objdump -S buildroot/output/build/ldd-117fbf20b34fc093e5457008253a505f621f307d/misc-modules/faulty.ko

buildroot/output/build/ldd-117fbf20b34fc093e5457008253a505f621f307d/misc-modules/faulty.ko:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <faulty_write>:
   0:	d2800001 	mov	x1, #0x0                   	// #0
   4:	d2800000 	mov	x0, #0x0                   	// #0
   8:	d503233f 	paciasp
   c:	d50323bf 	autiasp
  10:	b900003f 	str	wzr, [x1]
  14:	d65f03c0 	ret
  18:	d503201f 	nop
  1c:	d503201f 	nop
```

From the objdump command, we can see that the constant value of `0x0` is loaded into `x1` on the first instruction. Then, on line 0x10 of faulty_write 
(which is also shown in the oops message), the contents of the zero register `wzr` are stored to the memory pointed at by `xl`, which was zero. Hence the page fault.  
