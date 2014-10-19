#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
//#include <rtai.h>
//#include <rtai_sched.h>
//#include <rtai_fifos.h>

#define KB  1024
#define MB  1024*KB

#define FIFO_DAT    0
#define FIFO_CMD    1

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




#define rt_printk 	printk





static uint8_t irq = 0; //Good value for start??
static uint32_t mask = ~(0x0);
static void *pio_cfg_irqmask;
static void *pio_cfg_edgecapture;

// COLOCAR CABECALHOS
//static void pcie_interrupt_handler(void);

// Auxiliary function
/*static int rtf_get_assert(int minor, void *buf, int count) {
    int readcount = rtf_get(minor, buf, count);
    if(unlikely(readcount != count)) {
        rt_printk("pcie_interrupt_driver: ERROR: rtf_get requested %d bytes, got only %d bytes\n", count, readcount);
    }
    return readcount;
}*/

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

static irqreturn_t irq_handler(int irq, void *dev_id) {

    if (ioread32(pio_cfg_edgecapture)) {
        printk("pcie_interrupt_driver: Interruption arrived on IRQ %d\n", irq);

        // Clear Edge Capture register
        iowrite32(0x0, pio_cfg_edgecapture);
        // Renable interrupts on the device
        iowrite32(mask, pio_cfg_irqmask);
        wmb();
    }

    return IRQ_HANDLED;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    unsigned long resource;
    uint32_t vendor;

    int retval;

    retval = pci_enable_device(dev);

    if(retval) 
        rt_printk("pcie_interrupt_driver: ERROR: Device not enabled%d\n",retval);
    else
        rt_printk("pcie_interrupt_driver: device enabled!\n");

    // Gets a pointer to the PIO configuration registers related to the interrupt
    resource = pci_resource_start(dev,0);
    pio_cfg_irqmask = ioremap_nocache(resource + PIO_S1 + PIO_INTERRUPTMASK, 0x4);
    pio_cfg_edgecapture = ioremap_nocache(resource + PIO_S1 + PIO_EDGECAPTURE, 0x4);
    
    // Clears the edgecapture register
    iowrite32(0x0, pio_cfg_edgecapture);
    // Enable interrupt on the device
    iowrite32(mask, pio_cfg_irqmask);
    wmb();

    // Read vendor ID
    pci_read_config_dword(dev, 0, &vendor);
    rt_printk("pcie_interrupt_driver: Found Vendor id: %x\n", vendor);

    // Read IRQ Number
    irq = dev->irq;
    rt_printk("pcie_interrupt_driver: Found IRQ: %d\n", irq);
    enable_irq(irq);

    // Request IRQ and install handler
    retval = request_irq(irq, irq_handler, IRQF_SHARED, "pcie_interrupt", dev);
    rt_printk("pcie_interrut_driver: o que retornou do request_irq foi %d\n", retval);

    // Verify if other devices are sharing this IRQ???
    // What to do in this case???????

    // Request this irq
    /*if(likely(rt_request_global_irq(irq, pcie_interrupt_handler) == 0)) {
        rt_printk("pcie_interrupt_driver: IRQ %d requested successfully.\n", irq);
    }
    else {
        rt_printk("pcie_interrupt_driver: ERROR: couldn't request IRQ %d.\n", irq);
    }
*/
    // IRQ will be enabled later by user space using FIFO_CMD
//  rt_printk("pcie_interrupt_driver: Interrupts disabled.\n");

    return 0;
}

static void pci_remove(struct pci_dev *dev) {
    // Clear Edge Capture register
    iowrite32(0x0, pio_cfg_edgecapture);
    // Clear interrupts on the device
    iowrite32(0x0, pio_cfg_irqmask);
    free_irq(irq, dev);
    iounmap(pio_cfg_irqmask);
    iounmap(pio_cfg_edgecapture);
    rt_printk("pcie_interrupt_driver: Interrupt handler uninstalled.\n");
}

// ------------------------------------------

/*static int fifo_cmd_handler (unsigned int fifo) {
    uint32_t cmd = 0;
    rtf_get_assert(FIFO_CMD, &cmd, sizeof(cmd));
    rt_printk("pcie_interrupt_driver: Received command=%u\n", cmd);

    switch(cmd) {
    case 0:
        rt_disable_irq(irq);
        rtf_reset(FIFO_DAT);
        rt_printk("pcie_interrupt_driver: Interrupts disabled.\n");
        break;
    case 1:
        rtf_reset(FIFO_DAT);
        rt_enable_irq(irq);
        rt_printk("pcie_interrupt_driver: Interrupts enabled. IRQ %u\n", irq);
        break;
    }

    
    return 0;
}*/

/*static void pcie_interrupt_handler(void) {
    rt_printk("pcie_interrupt_driver: Interruption occured.");
    // Read Data from PCIe (it will be necessary to register!?)
    //
    // Write Data on user space FIFO
}*/

static int pcie_interrupt_init(void) {
    int retval;
    // Make RT Oneshot mode
/*    rt_set_oneshot_mode();
    
    // Create User Space FIFO and Command FIFO
    rtf_create(FIFO_DAT, 10*MB);
    rtf_create(FIFO_CMD, 1*KB);

    // Install fifo handler
    rtf_create_handler(FIFO_CMD, fifo_cmd_handler);
*/
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
  /*  rtf_destroy(FIFO_DAT);
    rtf_destroy(FIFO_CMD);*/

    // Unregister PCI driver
    pci_unregister_driver(&pci_driver);
    rt_printk("pcie_interrupt_driver: pci driver unregistered.\n");
}

module_init(pcie_interrupt_init);
module_exit(pcie_interrupt_exit);
MODULE_LICENSE("GPL");
