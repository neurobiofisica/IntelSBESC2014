#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>

#define KB  1024
#define MB  1024*KB

#define FIFO_DAT    0

// Defines addresses configured on QSys
#define PCIE_TXS	0x0000
#define PCIE_CRA	0x8000
#define PIO_S1		0xc000

// Defines for the PIO Interface
// We are considering words of 4 bytes
#define PIO_DATA	0x0
#define PIO_DIRECTION	0x4
#define PIO_INTERRUPTMASK	0x8
#define PIO_EDGECAPTURE	0xB


typedef struct {
    uint32_t mask;
    void *irqmask;
    void *edgecapture;
} dev_data;


// Auxiliary function
static int rtf_get_assert(int minor, void *buf, int count) {
    int readcount = rtf_get(minor, buf, count);
    if(unlikely(readcount != count)) {
        rt_printk("pcie_interrupt_driver: ERROR: rtf_get requested %d bytes, got only %d bytes\n", count, readcount);
    }
    return readcount;
}

// PCI Driver Interface

static struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(0x1172, 0x0004), },
    { PCI_DEVICE(0x1172, 0xe001), }, // Demo numbers
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void pci_remove(struct pci_dev *dev);

static struct pci_driver pci_driver = {
    .name       = "pcie_interrupt",
    .id_table   = pci_ids,
    .probe      = pci_probe,
    .remove     = pci_remove,
};

static int irq_handler(unsigned irq, void *cookie_) {
    dev_data *cookie = (dev_data*)cookie_;

    if (ioread32(cookie->edgecapture)) {
        printk("pcie_interrupt_driver: Interruption arrived on IRQ %d\n", irq);

        // Clear Edge Capture register
        iowrite32(0x0, cookie->edgecapture);
        // Renable interrupts on the device
        iowrite32(cookie->mask, cookie->irqmask);

        rt_enable_irq(irq);

        return 1;
    }

    rt_enable_irq(irq);
    return 0;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    unsigned long resource;

    int retval;

    dev_data *cookie = (dev_data*)kmalloc(sizeof(dev_data), GFP_KERNEL | __GFP_REPEAT);
    if(!cookie)
        return -ENOMEM;

    pci_set_drvdata(dev, cookie);

    cookie->mask = ~(0x0);

    retval = pci_enable_device(dev);

    if(retval) {
        rt_printk("pcie_interrupt_driver: ERROR: Device not enabled%d\n",retval);
        return retval;
    }
    else
        rt_printk("pcie_interrupt_driver: device enabled!\n");

    // Gets a pointer to the PIO configuration registers related to the interrupt
    resource = pci_resource_start(dev,0);
    cookie->irqmask = ioremap_nocache(resource + PIO_S1 + PIO_INTERRUPTMASK, 0x4);
    cookie->edgecapture = ioremap_nocache(resource + PIO_S1 + PIO_EDGECAPTURE, 0x4);
    
    // Clears the edgecapture register
    iowrite32(0x0, cookie->edgecapture);
    // Enable interrupt on the device
    iowrite32(cookie->mask, cookie->irqmask);

    // Read vendor ID
    rt_printk("pcie_interrupt_driver: Found Vendor id: %x\n", dev->vendor);
    rt_printk("pcie_interrupt_driver: Found Device id: %x\n", dev->device);

    // Read IRQ Number
    rt_printk("pcie_interrupt_driver: Found IRQ: %d\n", dev->irq);

    // Request IRQ and install handler
    retval = rt_request_irq(dev->irq, irq_handler, cookie, 0);
    if(retval) {
        rt_printk("pcie_interrupt_driver: request_irq failed!");
        return retval;
    }
    rt_startup_irq(dev->irq);

    return 0;
}

static void pci_remove(struct pci_dev *dev) {
    dev_data *cookie = (dev_data*)pci_get_drvdata(dev);
    
    rt_release_irq(dev->irq);

    // Clear Edge Capture register
    iowrite32(0x0, cookie->edgecapture);
    // Clear interrupts on the device
    iowrite32(0x0, cookie->irqmask);

    iounmap(cookie->irqmask);
    iounmap(cookie->edgecapture);

    kfree(cookie);
    pci_set_drvdata(dev,NULL);
    rt_printk("pcie_interrupt_driver: Interrupt handler uninstalled.\n");
}

// ------------------------------------------

static int pcie_interrupt_init(void) {
    int retval;

    // Create User Space FIFO and Command FIFO
    rtf_create(FIFO_DAT, 10*MB);

    // Register PCI Driver
    // IRQ is requested on pci_probe
    retval = pci_register_driver(&pci_driver);
    if (retval<0)
        rt_printk("pcie_interrupt_driver: error: cannot register pci.\n");
    else
        rt_printk("pcie_interrupt_driver: pci driver registered.\n");

    return retval;
}

static void pcie_interrupt_exit(void) {
    // Destroy FIFOs
    rtf_destroy(FIFO_DAT);

    // Unregister PCI driver
    pci_unregister_driver(&pci_driver);
    rt_printk("pcie_interrupt_driver: pci driver unregistered.\n");
}

module_init(pcie_interrupt_init);
module_exit(pcie_interrupt_exit);
MODULE_LICENSE("GPL");
