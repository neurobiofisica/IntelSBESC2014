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
#define FIFO_STM    2
#define FIFO_ARG    3

// Commands
#define CMD_STOP_ACQ      0
#define CMD_START_ACQ     1
#define CMD_CLR_FIFO_STM  2
#define CMD_CHANGE_BFSTM  3
#define CMD_CHANGE_PATT   4
#define CMD_LSTIM_TYPE    5

// Defines addresses configured on QSys
#define PCIE_TXS	0x0000
#define PCIE_CRA	0x8000
#define ACQSYS		0xc000

// Defines some CRA addresses
#define PCIE_INTSTAREG  0x40

// Defines for the AcqSys Interface
// We are considering words of 4 bytes
#define ACQ_STARTED     0x00
#define ACQ_CHANNELFLG  0x04
#define ACQ_TIMESTAMP   0x08
#define ACQ_STIM        0x0c

// Invalid flag from channels (empty FIFO)
#define INVALID_FLAG    0x80000000

static void *acq_base;

static uint8_t n_devices = 0;

static const int NumChannels = 8;

typedef enum {
	STIM_ZIGZAG = 0,
	STIM_FIFO = 1,
} stimuli_type;

static stimuli_type long_stimuli_type = STIM_ZIGZAG;
static uint8_t stim = 0x18;
static int dir = 1;

static uint32_t pattern_bin_size = 1;     /* bin size in units of screen refresh period */
static uint64_t pattern_bits = 0xff;      /* pattern bins */
static uint64_t pattern_mask = 0;         /* mask of valid bits in pattern_bits */
static uint64_t signal_bits  = 0;         /* signal bins */

static uint32_t screen_tick_counter = 0;

static int spike_since_last_bin = 0;

static int brief_stimuli_active = 0;
static uint8_t brief_stimuli[4096];
static int brief_stimuli_len = 0;
static int brief_stimuli_pos = 0;


// Auxiliary function
static int rtf_get_assert(int minor, void *buf, int count) {
    int readcount = rtf_get(minor, buf, count);
    if(unlikely(readcount != count)) {
        rt_printk("pcie_interrupt_driver: ERROR: rtf_get requested %d bytes, got only %d bytes\n", count, readcount);
    }
    return readcount;
}

static void bin_maker_routine(void) {
    signal_bits = (signal_bits << 1) | spike_since_last_bin;
    if(!brief_stimuli_active && ((signal_bits & pattern_mask) == pattern_bits)) {
        brief_stimuli_pos = 0;
        brief_stimuli_active = 1;
    }
    //t |= (brief_stimuli_active << 1) | spike_since_last_bin;
    //rtf_put(FIFO_RFD, &t, sizeof(t));
    spike_since_last_bin = 0;
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
        //if(flags != 0x100)
        //    rt_printk("pcie_interrupt_driver: Interruption arrived from channel 0x%x\n", flags);
        
        // add "brief stimuli is active" flag
        flags |= brief_stimuli_active << (NumChannels + 1);

        if(flags & 0x100) {
			/* -- screen/stimulus refresh -- */
			
			/* synchronous bin frontier */
			if(++screen_tick_counter >= pattern_bin_size) {
				bin_maker_routine();
				screen_tick_counter = 0;
			}
			
			if (long_stimuli_type == STIM_FIFO) {
				rtf_get(FIFO_STM, &stim, sizeof(stim));
			}
			
			if (brief_stimuli_active) {
				stim = brief_stimuli[brief_stimuli_pos++];
			}
			else if (long_stimuli_type == STIM_ZIGZAG) {
				if (dir == 1)
				   stim = stim >> 1;
				else if (dir == -1)
					stim = stim << 1;
				if (stim == 0x01) { // Threw the bit away
					dir = -1;
					stim = 0x03;
				}
				if (stim == 0x80) {
					dir = 1;
					stim = 0xc0;
				}
			}
			
            iowrite32(stim, acq_base + ACQ_STIM);
            
			if(brief_stimuli_pos >= brief_stimuli_len)
				brief_stimuli_active = 0;
        }
        
        if (flags & 0x1) {
			/* -- spike in channel CH0 -- */
			spike_since_last_bin = 1;
		}

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
    acq_base = ioremap_nocache(resource + ACQSYS + ACQ_STARTED, 0x4000);

    // Load initial stimulus
    iowrite32(stim, acq_base + ACQ_STIM);

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

    // Enable Acquisition
    //iowrite32(0x1, acq_base + ACQ_STARTED);

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
    case CMD_STOP_ACQ:
        /* -- stop acquisition -- */
        rt_printk("pcie_interrupt_driver: stopping\n");
		// Clear the STARTED Register
		iowrite32(0x0, acq_base + ACQ_STARTED);
        // Clear the stimulus FIFO
        rtf_reset(FIFO_STM);
        break;
    case CMD_START_ACQ:
        /* -- start acquisition -- */
        rt_printk("pcie_interrupt_driver: starting\n");
        brief_stimuli_active = 0;
		// Enable Acquisition
		iowrite32(0x1, acq_base + ACQ_STARTED);
        break;
    case CMD_CLR_FIFO_STM:
        /* -- clear the stimuli FIFO -- */
        rtf_reset(FIFO_STM);
        break;
    case CMD_CHANGE_BFSTM:
        /* -- change brief stimuli -- */
        {
            uint32_t new_brief_len;
            rtf_get_assert(FIFO_ARG, &new_brief_len, sizeof(new_brief_len));
            if(new_brief_len > sizeof(brief_stimuli)) {
                rt_printk("pcie_interrupt_driver: WARNING: truncating requested brief stim len=%u\n", new_brief_len);
                new_brief_len = sizeof(brief_stimuli);
            }
            rtf_get_assert(FIFO_ARG, brief_stimuli, new_brief_len);
            brief_stimuli_len = new_brief_len;
            brief_stimuli_pos = 0;
            rt_printk("pcie_interrupt_driver: changing brief stim (len=%u)\n", brief_stimuli_len);
        }
    case CMD_CHANGE_PATT:
        /* -- change pattern -- */
        {
            uint32_t new_bin_size;
            rtf_get_assert(FIFO_ARG, &new_bin_size, sizeof(new_bin_size));
            rtf_get_assert(FIFO_ARG, &pattern_bits, sizeof(pattern_bits));
            rtf_get_assert(FIFO_ARG, &pattern_mask, sizeof(pattern_mask));
            pattern_bin_size = new_bin_size;
            rt_printk("pcie_interrupt_driver: changing pattern (binsize=%u us, pattern=0x%x, mask=0x%x)\n", pattern_bin_size, pattern_bits, pattern_mask);
        }
        break;
    case CMD_LSTIM_TYPE:
        /* -- change long stimulus type -- */
        {
			uint32_t tp;
			rtf_get_assert(FIFO_ARG, &tp, sizeof(tp));
			long_stimuli_type = tp;
			rt_printk("pcie_interrupt_driver: changed long stimulus type to %u\n", long_stimuli_type);
		}
    }
    return 0;
}


/*******************************
 *  Driver Init/Exit Functions *
 *******************************/

static int pcie_interrupt_init(void) {
    int retval;

    // Create the FIFOs
    rtf_create(FIFO_DAT, 10*MB);
    rtf_create(FIFO_CMD, 5*KB);
    rtf_create(FIFO_STM, 8*MB);
    rtf_create(FIFO_ARG, 2*MB);

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
    rtf_destroy(FIFO_CMD);
    rtf_destroy(FIFO_STM);
    rtf_destroy(FIFO_ARG);
    
    // Unregister PCI driver
    pci_unregister_driver(&pci_driver);
    rt_printk("pcie_interrupt_driver: pci driver unregistered.\n");
}

module_init(pcie_interrupt_init);
module_exit(pcie_interrupt_exit);
MODULE_LICENSE("GPL");
