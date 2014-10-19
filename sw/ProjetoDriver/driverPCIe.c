/************************************************************
 * For newbies:                                             *
 *    Read this module stating from the bottom to the up to *
 *    understand the sequence of the events                 *
 ************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

// -------------------
// ----- Defines -----
// -------------------

// These defines are default for altera,
// but they can be changed on QSys Hard IP
// PCI Express Compiler configurations
#define PCI_VENDORID    0x1172
#define PCI_PRODUCTID   0x0004

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Basic Driver PCIHello");
MODULE_AUTHOR("Patrick Schaumont");

// Defines for onchip memory proprieties
// They were specified on QSys
#define MEMORY_ADDR 0XC000
// 4096 bytes
#define MEMORY_SIZE 0X1000

// ------------------------------------------
// ---------------- Headers -----------------
// ------------------------------------------

// Char device operation headers
static int      char_device_open    (struct inode*, struct file*);
static int      char_device_release (struct inode*, struct file*);
static ssize_t  char_device_read    (struct file*, char *, size_t, loff_t *);
static ssize_t  char_device_write   (struct file*, const char *, size_t, loff_t *);

// PCI device probe and remove headers
// There is the option of defining suspend and resume methods too.
static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void pci_remove(struct pci_dev *dev);

// Pointers for device addresses
static void *memory;

// ------------------------------------------
// --------- Char Driver Interface ----------
// ------------------------------------------

// Char driver interface, for acesing it on user space
// This is an old way of doing it. There is a way of
// defining the MAJOR_NUMBER dynamically
static int access_count  =    0;
static int MAJOR_NUMBER =   91;

// Char device open implementation
// This function is called every time a process open the device file descriptor with the major number specified on register_chrdev
// on driver inicialization
static int char_device_open (struct inode *inodep, struct file *filep) {
    // This variable describes how much processes are accessing the device file descriptor
    access_count++; // Increments the access counter
    printk(KERN_ALERT "altera_driver: opened %d time(s)\n", access_count);
    return 0;
}

// Char device release implementation
// This function is called every time a process closes the device file descriptor
static int char_device_release (struct inode *inodep, struct file *filep) {
    printk(KERN_ALERT "altera_driver: device closed.\n");
    return 0;
}

// Char device read implementation
// This method is called every time the user space calls de read() system call
static ssize_t char_device_read (struct file *filep, char *buf, size_t len, loff_t *off) {
    int16_t data;
    size_t count = len;

    while (len > 0) {
        data = ioread16(memory);
        rmb();
        put_user(data % 0XFF, buf++);

        len -= 2;
    }
    return count;
}

// Char device write implementation
// This method is called every time the user space calls de write() system call
static ssize_t char_device_write (struct file *filep, const char *buf, size_t len, loff_t *off) {
    uint16_t data;
    char *ptr = (char *) buf;
    size_t count = len;
    int b = 0;

    while (b < len) {
        get_user(data, ptr);
        iowrite16(data, memory);
        wmb();

        ptr += 2;
        b += 2;
    }
    return count;
}

// Char device operations structure
// This struct installs the functions associated with the device operations.
static struct file_operations file_opts = {
    .read       =   char_device_read,
    .open       =   char_device_open,
    .write      =   char_device_write,
    .release    =   char_device_release
};

// ------------------------------------------
// ---------- PCI Device Interface ----------
// ------------------------------------------

// PCI registration of VendorID and ProductID
static struct pci_device_id pci_ids[] = {
    { PCI_DEVICE( PCI_VENDORID, PCI_PRODUCTID), },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

// PCI struct definion/registration of the IDs table and probe/remove functions
static struct pci_driver pci_driver = {
    .name       = "pci_lirio_rafinha",
    .id_table   = pci_ids,
    .probe      = pci_probe,
    .remove     = pci_remove,
    // There is the option of inserting .suspend and .resume functions.
};

// pci_probe implementation
static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
    unsigned long resource;
    int retval;

    // Enable PCI device for normal functions
    retval = pci_enable_device(dev); // Just for shutting up the warning

    // Reads the first address of the BAR 0
    resource = pci_resource_start(dev, 0);
    
    // Gets a pointer for the internal FPGA memory. MEMORY_ADDR and MEMORY_SIZE were configured on QSys
    memory = ioremap_nocache(resource + MEMORY_ADDR, MEMORY_SIZE);

    return 0;
}

// pci_remove implementation
static void pci_remove(struct pci_dev *dev) {
    // Frees the pointer fo the internal FPGA memory
    iounmap(memory);
}

// ------------------------------------------
// ------- Global module registration -------
// ------------------------------------------

static int __init altera_driver_init(void) {
   int t = register_chrdev(MAJOR_NUMBER, "de2i150_altera", &file_opts); //This is a older way of registering
   // Note that the MAJOR_NUMBER is explicit here! There is no dynamic assignment
   t = t | pci_register_driver(&pci_driver);

   if(t<0)
      printk(KERN_ALERT "altera_driver: error: cannot register char or pci.\n");
   else
     printk(KERN_ALERT "altera_driver: char+pci drivers registered.\n");

   return t;
}

static void __exit altera_driver_exit(void) {
  printk(KERN_ALERT "Goodbye from de2i150_altera.\n");

  unregister_chrdev(MAJOR_NUMBER, "de2i150_altera");
  pci_unregister_driver(&pci_driver);
}

module_init(altera_driver_init);
module_exit(altera_driver_exit);
