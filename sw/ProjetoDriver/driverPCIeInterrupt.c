#include <linux/module.h>
#include <linux/pci.h>
#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>

#define PCI_INTERRUPT_LINE  0x3c
#define PCI_INTERRUPT_PIN   0x3d

#define KB  1024
#define MB  1024*KB

#define FIFO_DAT    0
#define FIFO_CMD    1




#define rt_printk	printk




static uint8_t irq = 0; //Good value for start??

// COLOCAR CABECALHOS
static void pcie_interrupt_handler(void);

// PCI Driver Interface

static struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(0x1172, 0x0004), },
    { PCI_DEVICE(0X1172, 0xe001), },
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
    uint32_t vendor;
    uint8_t int_act;

    int retval;

    retval = pci_enable_device(dev);

    // Read vendor ID
/*    pci_read_config_dword(dev, 0, &vendor);
    rt_printk("pcie_interrupt_driver: Found Vendor id: %x\n", vendor);

    // Read IRQ Number
    pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
    rt_printk("pcie_interrupt_driver: Found IRQ: %d\n", irq);
*/
    // Verify if other devices are sharing this IRQ???
    // What to do in this case???????

    // Verify if Interrupt are active
/*    pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &int_act);
    if (int_act)
        rt_printk("pcie_interrupt_driver: Interrupts are active.\n");
    else
        rt_printk("pcie_interrupt_driver: Interrupts are disabled.\n");

    // Request this irq
    if(likely(rt_request_global_irq(irq, pcie_interrupt_handler) == 0)) {
        rt_printk("pcie_interrupt_driver: IRQ %d requested successfully.\n", irq);
    }
    else {
        rt_printk("pcie_interrupt_driver: ERROR: couldn't request IRQ %d.\n", irq);
    }

    // IRQ will be enabled later by user space using FIFO_CMD
    rt_disable_irq(irq); ///////////??????????????????????????????????????????*/

    return 0;
}

static void pci_remove(struct pci_dev *dev) {
//    rt_disable_irq(irq);
  //  rt_free_global_irq(irq);
}

// ------------------------------------------

static int fifo_cmd_handler (unsigned int fifo) {
    rt_printk("pcie_interrupt_driver: Command received from user space.");
    // Make User Space interface (Enable IRQ!!!)
    return 0;
}

static void pcie_interrupt_handler(void) {
    rt_printk("pcie_interrupt_driver: Interruption occured.");
    // Read Data from PCIe (it will be necessary to register!?)
    //
    // Write Data on user space FIFO
}

static int pcie_interrupt_init(void) {
    int retval;
    // Make RT Oneshot mode
    //rt_set_oneshot_mode();
    
    // Create User Space FIFO and Command FIFO
//    rtf_create(FIFO_DAT, 10*MB);
//    rtf_create(FIFO_CMD, 1*KB);

    // Install fifo handler
//    rtf_create_handler(FIFO_CMD, fifo_cmd_handler);

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
    rt_printk("pcie_interrupt_driver: pci driver unregisted.\n");
    // Destroy FIFOs
//    rtf_destroy(FIFO_DAT);
//    rtf_destroy(FIFO_CMD);


    // Unregister PCI driver
    pci_unregister_driver(&pci_driver);
}

module_init(pcie_interrupt_init);
module_exit(pcie_interrupt_exit);
MODULE_LICENSE("GPL");
