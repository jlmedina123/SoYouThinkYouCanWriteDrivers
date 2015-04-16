/* This is a small driver that I wrote to for the NIC on my laptop. 
It only includes the PCI initialization part of the driver. If I manage
to get the functional specs, I will complete this driver.
The following instructions show how to get information about your NIC
(device ID and vendor ID), and how to unload the running driver to
load the one you write.

How to use driver:
 
cli> lspci -v
 
10:00.0 Ethernet controller: Qualcomm Atheros AR8151 v2.0 Gigabit Ethernet (rev c0)
        Subsystem: Hewlett-Packard Company Device 1688
        Flags: bus master, fast devsel, latency 0, IRQ 58
        Memory at c1500000 (64-bit, non-prefetchable) [size=256K]
        I/O ports at 2000 [size=128]
        Capabilities: access denied
        Kernel driver in use: atl1c
 
cli> lspci -n | grep 10:00
10:00.0 0200: 1969:1083 (rev c0) <-- vendor ID and device ID
 
cli> lsmod | grep -i atl1c
atl1c                  40949  0
 
cli> ls
Makefile  pci_nic.c
 
cli> make
make -C /lib/modules/3.13.0-34-generic/build M=/home/jose/Documents/driverspractice/pci modules
make[1]: Entering directory `/usr/src/linux-headers-3.13.0-34-generic'
  CC [M]  /home/jose/Documents/driverspractice/pci/pci_nic.o
  Building modules, stage 2.
  MODPOST 1 modules
  CC      /home/jose/Documents/driverspractice/pci/pci_nic.mod.o
  LD [M]  /home/jose/Documents/driverspractice/pci/pci_nic.ko
make[1]: Leaving directory `/usr/src/linux-headers-3.13.0-34-generic'
 
cli> sudo rmmod atl1c
 
cli> sudo insmod pci_nic.ko
 
 
cli> sudo tail -f /var/log/kern.log
Aug 29 14:17:32 jose kernel: [16553.276170] pci_eth_driver: initializing module
Aug 29 14:17:32 jose kernel: [16553.276208] pci_eth_driver: pci probe called.
Aug 29 14:17:32 jose kernel: [16553.276478] pci_eth_driver: pci driver registered
*/
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/types.h>
 
#define VENDOR_ID 0x1969
#define DEVICE_ID 0x1083
#define DRV_NAME "pci_eth_driver"
 
static int my_pci_probe(struct pci_dev *, const struct pci_device_id *);
static void my_pci_remove(struct pci_dev *);
 
static struct pci_device_id pci_id_table[] = {
    {VENDOR_ID, DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_ANY_ID},
    { 0, }
};
 
static struct pci_driver my_pci_driver= {
    .name = DRV_NAME,
    .id_table = pci_id_table,
    .probe =  my_pci_probe,
    .remove = my_pci_remove,
};
 
/* expose table id to pci hotplug module */
MODULE_DEVICE_TABLE(pci, pci_id_table); 
 
/*
 * PCI entry points 
 */
static int my_pci_probe(struct pci_dev *my_pci_dev, 
                        const struct pci_device_id *id) {
    unsigned long mmio_start, mmio_end, mmio_len, mmio_flags;
    void __iomem *ioaddr;
    int rval;
 
    printk(KERN_ALERT DRV_NAME ": pci probe called.\n");
    
    /*
     * PCI initialization
     */
    // 1) Enable PCI device 
    rval = pci_enable_device(my_pci_dev);
    if (rval) {
        printk(KERN_ALERT DRV_NAME ": pci device enabled failed\n");
        return rval;
    }
 
    // 2) Enable bus mastering by pci device 
    pci_set_master(my_pci_dev);
 
    // 3) Access MMIO region 1 
    mmio_start = pci_resource_start(my_pci_dev, 1);
    mmio_len =   pci_resource_len(my_pci_dev, 1);
    mmio_end =   pci_resource_end(my_pci_dev, 1);
    mmio_flags = pci_resource_flags(my_pci_dev, 1);
 
    // 4) check region is memory-mapped IO
    if (mmio_flags & IORESOURCE_IO) {
        printk(KERN_ALERT DRV_NAME ": PCI IO region 1 is port mapped\n");
        goto disable;
    }
 
    // 5) take ownership of region
    request_region(mmio_start, mmio_len, DRV_NAME);
 
    // 6) map IO region
    ioaddr = pci_iomap(my_pci_dev, 1, mmio_len);
 
    // 7) Set consistent (coherent) 32bit DMA
    rval = pci_set_consistent_dma_mask(my_pci_dev, DMA_BIT_MASK(32));
    if (rval) {
        printk(KERN_ALERT DRV_NAME ": couldnt set 32 bit DMA\n");
        goto unmap;
    }
     
    return 0;
 
unmap:
    pci_iounmap(my_pci_dev, ioaddr);
 
//release:
    pci_release_regions(my_pci_dev);
 
disable:
    pci_disable_device(my_pci_dev);
 
    return -1;
}
 
static void my_pci_remove(struct pci_dev *my_pci_dev) {
    printk(KERN_ALERT DRV_NAME ": pci remove called\n");
    //pci_iounmap(my_pci_dev, ioaddr);
    pci_release_regions(my_pci_dev);
    pci_disable_device(my_pci_dev);
}
 
static int my_init(void) {
    printk(KERN_ALERT DRV_NAME ": initializing module\n");
    if (pci_register_driver(my_pci_driver) < 0) {
        printk(KERN_ALERT DRV_NAME ": pci_register_driver failed\n");
    } else {
        printk(KERN_ALERT DRV_NAME ": pci driver registered\n");
    }
    return 0;
}
static void my_exit(void) {
    printk(KERN_ALERT DRV_NAME ": uninitializing module\n");    
    pci_unregister_driver(my_pci_driver);
    printk(KERN_ALERT DRV_NAME ": pci driver unregistered\n");
}
 
 
MODULE_AUTHOR("jlmedina123");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("PCI driver (PCI part only) for the PCI Ethernet card on my laptop");
 
module_init(my_init);
module_exit(my_exit);
