#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>

#define KB  1024
#define MB  1024*KB

#define FIFO_DAT    0
#define FIFO_CMD    1

// Commands
#define CLEAR_FIFO  0


// Defines addresses configured on QSys
#define PCIE_TXS	0x0000
#define PCIE_CRA	0x8000
#define ACQSYS		0xc000

// Defines some CRA addresses
#define PCIE_INTSTAREG  0x40

// Defines for the AcqSys Interface
// We are considering words of 4 bytes
#define ACQ_STARTED     0x0
#define ACQ_CHANNELFLG  0x4
#define ACQ_TIMESTAMP   0x8

// Invalid flag from channels (empty FIFO)
#define INVALID_FLAG    0x80000000

static void *acq_base;

static uint8_t n_devices = 0;


// Auxiliary function
static int rtf_get_assert(int minor, void *buf, int count) {
    int readcount = rtf_get(minor, buf, count);
    if(unlikely(readcount != count)) {
        rt_printk("pcie_interrupt_driver: ERROR: rtf_get requested %d bytes, got only %d bytes\n", count, readcount);
    }
    return readcount;
}

/*******************************
 *    PCI Driver Interface     *
 *******************************/


// -------- Interrupt handler --------
static int irq_handler(unsigned irq, void *cookie_) {
    uint32_t flags = ioread32(acq_base + ACQ_CHANNELFLG);
    uint32_t timestamp;

    uint64_t data;

    // Verifies if the interruption is from our hardware
    if (flags != INVALID_FLAG) {
        rt_printk("pcie_interrupt_driver: Interruption arrived from channel %x\n", flags);

        // Read timestamp (Reset is a side-effect)
        timestamp = ioread32(acq_base + ACQ_TIMESTAMP);

        data = ((uint64_t)timestamp << 32) | (uint64_t)flags;
        rtf_put(FIFO_DAT, &data, sizeof(data));

        // Unmask IRQ
        rt_unmask_irq(irq);

        return 1;
    }

    rt_unmask_irq(irq);

    return 0;
}

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

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    unsigned long resource;
    int retval;

    if(++n_devices > 1)
        return -EOVERFLOW;

    retval = pci_enable_device(dev);

    if(retval) {
        rt_printk("pcie_interrupt_driver: ERROR: Device not enabled%d\n",retval);
        return retval;
    }
    else
        rt_printk("pcie_interrupt_driver: device enabled!\n");

    // Gets a pointer to the AcqSys configuration registers
    resource = pci_resource_start(dev,0);
    acq_base = ioremap_nocache(resource + ACQSYS + ACQ_STARTED, 0xc);

    // Enable Acquisition
    iowrite32(0x1, acq_base + ACQ_STARTED);

    // Read vendor ID
    rt_printk("pcie_interrupt_driver: Found Vendor id: 0x%0.4x\n", dev->vendor);
    rt_printk("pcie_interrupt_driver: Found Device id: 0x%0.4x\n", dev->device);

    // Read IRQ Number
    rt_printk("pcie_interrupt_driver: Found IRQ: %d\n", dev->irq);

    // Request IRQ and install handler
    retval = rt_request_irq(dev->irq, irq_handler, NULL, 0);
    if(retval) {
        rt_printk("pcie_interrupt_driver: request_irq failed!");
        return retval;
    }

    rt_startup_irq(dev->irq);

    return 0;
}

static void pci_remove(struct pci_dev *dev) {
    
    rt_release_irq(dev->irq);

    --n_devices;

    // Clear the STARTED Register
    iowrite32(0x0, acq_base + ACQ_STARTED);

    iounmap(acq_base);

    pci_set_drvdata(dev,NULL);
    rt_printk("pcie_interrupt_driver: Interrupt handler uninstalled.\n");
}

/*******************************
 *     FIFO Command Handler    *
 *******************************/

static int fifo_cmd_handler(unsigned int fifo) {
    uint32_t cmd = 0;

    rtf_get_assert(FIFO_CMD, &cmd, sizeof(cmd));
    rt_printk("pcie_interrupt_driver: Received cmd=%u\n", cmd);
    switch(cmd) {
    case CLEAR_FIFO: 
        rt_printk("pcie_interrupt_driver: Resetting FIFO_DAT\n");
        rtf_reset(FIFO_DAT);
        break;
    }
    return 0;
}


/*******************************
 *  Driver Init/Exit Functions *
 *******************************/

static int pcie_interrupt_init(void) {
    int retval;

    // Create User Space FIFO and Command FIFO
    rtf_create(FIFO_DAT, 10*MB);
    rtf_create(FIFO_CMD, 5*KB);

    // Create FIFO Handler
    rtf_create_handler(FIFO_CMD, fifo_cmd_handler);

    // Register PCI Driver
    // IRQ is requested on pci_probe
    retval = pci_register_driver(&pci_driver);
    if (retval)
        rt_printk("pcie_interrupt_driver: ERROR: cannot register pci.\n");
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