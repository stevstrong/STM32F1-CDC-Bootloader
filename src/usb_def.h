
#ifndef USB_DEF_H
#define USB_DEF_H

#include <stdbool.h>
#include <stdint.h>


//#define USB_DEBUG 1

//-----------------------------------------------------------------------------
// Drive USB as virtual COM port
//-----------------------------------------------------------------------------

/* For devices with 2 x 16 bits / uint16_t access schema (e.g. STM32L0, STM32F303xD and xE) */
//#define UMEM_SHIFT 0
//#define UMEM_FAKEWIDTH uint16_t

/* For devices with 1 x 16 bits / uint16_t access schema (e.g. STM32F103, STM32F302, STM32F303xB and xC) */
#define UMEM_SHIFT 1
#define UMEM_FAKEWIDTH uint32

/* The name of the IRQ handler must match startup_stm32.s */
#define NAME_OF_USB_IRQ_HANDLER USB_LP_CAN_RX0_IRQHandler

/* Take the number from the reference manual of your �C. */
#define USB_IRQ_NUMBER 20

// Enable trace messages
#define ENABLE_TRACING 0

#if ENABLE_TRACING
    #include <stdio.h>

    #define ITM_PORT0_8   (*(volatile char *)(0xE0000000UL))
    #define ITM_PORT0_32  (*(volatile unsigned long *)(0xE0000000UL))
    #define ITM_TER       (*(volatile unsigned long *)(0xE0000E00UL))
    #define ITM_TCR       (*(volatile unsigned long *)(0xE0000E80UL))

    static void trace(char *ptr)
    {
        while (*ptr)
        {
            if (((ITM_TCR & 1UL) != 0UL) &&   // ITM enabled
                ((ITM_TER & 1UL) != 0UL))     // ITM and port 0 enabled
            {
                while (ITM_PORT0_32 == 0UL)
                {
                    asm volatile("NOP");
                }
                ITM_PORT0_8 = *ptr++;
            }
        }
    }
#else
#include "systick.h"
	#ifdef USB_DEBUG
		extern void DoTrace(char *, int);
		extern void NrTrace(int, int);
		//#define trace(s) { char* dest = &dBuf[dLen]; dLen += (sizeof(s)-1); const char* src = s; for (uint32_t i=0; i<(sizeof(s)-1); i++) *dest++ = *src++; }
		#define trace(s) DoTrace(s, sizeof(s)-1)
		#define ntrace(d,nl) NrTrace(d, nl)
		#define TIME_STAMP trace("["); NrTrace(millis(), 0); trace("]");
	#else
		#define trace(...)
		#define ntrace(d,nl)
		#define TIME_STAMP
	#endif
#define strace(...)
#endif


//-----------------------------------------------------------------------------
#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#define READ_BIT(REG, BIT)    ((REG) & (BIT))

#define CLEAR_REG(REG)        ((REG) = (0x0))

#define SET_REG(REG, VAL)   ((REG) = (VAL))

#define GET_REG(REG)         ((REG))

#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

/***************************  Constant  ********************************/

#define BIT(shift)                     (1UL << (shift))
/* Cortex-M NVIC Register */
#define NVIC_ISER  (*(volatile uint32_t (*) [16])(0xE000E100))
#define NVIC_ICER  (*(volatile uint32_t (*) [16])(0xE000E180))

/*
 All USB-Registers are 16 Bit, but they must be read/write as 32 bit data
 */

/* USB device (base address 0x4000 5C00) */
#define USB_BASE      0x40005C00
#define USB_EpRegs(x) (*(volatile uint32_t *)(0x40005C00 + 4*(x)))
#define USB_EP0R      (*(volatile uint32_t *)(0x40005C00))
#define USB_EP1R      (*(volatile uint32_t *)(0x40005C04))
#define USB_EP2R      (*(volatile uint32_t *)(0x40005C08))
#define USB_EP3R      (*(volatile uint32_t *)(0x40005C0C))
#define USB_EP4R      (*(volatile uint32_t *)(0x40005C10))
#define USB_EP5R      (*(volatile uint32_t *)(0x40005C14))
#define USB_EP6R      (*(volatile uint32_t *)(0x40005C18))
#define USB_EP7R      (*(volatile uint32_t *)(0x40005C1C))

#define USB_CNTR      (*(volatile uint32_t *)(0x40005C40))
#define USB_ISTR      (*(volatile uint32_t *)(0x40005C44))
#define USB_FNR       (*(volatile uint32_t *)(0x40005C48))
#define USB_DADDR     (*(volatile uint32_t *)(0x40005C4C))
#define USB_BTABLE    (*(volatile uint32_t *)(0x40005C50))

/* Control register (USB_CNTR) */
#define USB_CNTR_CTRM_BIT              15
#define USB_CNTR_PMAOVERM_BIT          14
#define USB_CNTR_ERRM_BIT              13
#define USB_CNTR_WKUPM_BIT             12
#define USB_CNTR_SUSPM_BIT             11
#define USB_CNTR_RESETM_BIT            10
#define USB_CNTR_SOFM_BIT              9
#define USB_CNTR_ESOFM_BIT             8
#define USB_CNTR_RESUME_BIT            4
#define USB_CNTR_FSUSP_BIT             3
#define USB_CNTR_LP_MODE_BIT           2
#define USB_CNTR_PDWN_BIT              1
#define USB_CNTR_FRES_BIT              0

#define USB_CNTR_CTRM                  BIT(USB_CNTR_CTRM_BIT)
#define USB_CNTR_PMAOVERM              BIT(USB_CNTR_PMAOVERM_BIT)
#define USB_CNTR_ERRM                  BIT(USB_CNTR_ERRM_BIT)
#define USB_CNTR_WKUPM                 BIT(USB_CNTR_WKUPM_BIT)
#define USB_CNTR_SUSPM                 BIT(USB_CNTR_SUSPM_BIT)
#define USB_CNTR_RESETM                BIT(USB_CNTR_RESETM_BIT)
#define USB_CNTR_SOFM                  BIT(USB_CNTR_SOFM_BIT)
#define USB_CNTR_ESOFM                 BIT(USB_CNTR_ESOFM_BIT)
#define USB_CNTR_RESUME                BIT(USB_CNTR_RESUME_BIT)
#define USB_CNTR_FSUSP                 BIT(USB_CNTR_FSUSP_BIT)
#define USB_CNTR_LP_MODE               BIT(USB_CNTR_LP_MODE_BIT)
#define USB_CNTR_PDWN                  BIT(USB_CNTR_PDWN_BIT)
#define USB_CNTR_FRES                  BIT(USB_CNTR_FRES_BIT)

/* Bits in USB_CNTR */
#define  FRES     (1<<0)
#define  PDWN     (1<<1)
#define  LP_MODE  (1<<2)
#define  FSUSP    (1<<3)
#define  RESUME   (1<<4)

#define  ESOFM    (1<<8)
#define  SOFM     (1<<9)
#define  RESETM   (1<<10)
#define  SUSPM    (1<<11)
#define  WKUPM    (1<<12)
#define  ERRM     (1<<13)
#define  PMAOVRM  (1<<14)
#define  CTRM     (1<<15)

/* Bits in USB_ISTR */
#define  DIR      (1<<4)
#define  ESOF     (1<<8)
#define  SOF      (1<<9)
#define  RESET    (1<<10)
#define  SUSP     (1<<11)
#define  WKUP     (1<<12)
#define  ERR      (1<<13)
#define  PMAOVR   (1<<14)
#define  CTR      (1<<15)

/* Bits in den USB_EPnR */
#define  CTR_RX   (1<<15)
#define  DTOG_RX  (1<<14)
#define  STAT_RX  (3<<12)
#define  SETUP    (1<<11)
#define  EP_TYPE  (3<<9)
#define  EP_KIND  (1<<8)
#define  CTR_TX   (1<<7)
#define  DTOG_TX  (1<<6)
#define  STAT_TX  (3<<4)
#define  MASK_EP  (15)

/* EndPoint Register Mask (No Toggle Fields) */
#define  EP_MASK_NoToggleBits  (CTR_RX|SETUP|EP_TYPE|EP_KIND|CTR_TX|MASK_EP)

/*
 Attention! The following special RAM handling is not valid for STM32F303xD and xE!

 Layout of USB RAM (up to 512 Bytes)
 ================================
 The RAM goes (seen by the CPU) from 0x40006000 to 0x400063FF, = 0x400 (1024) bytes
 The RAM is made by 4 bytes slots, wherein only the lower 16 bits are used for data

 Example memory for "Hello-World"
 0x40006000: 48 65 00 00 6C 6C 00 00 6F 2D 00 00 57 6F 00 00 72 6C 00 00 64 ...
             H  e        l  l        o  -        W  o        r  l        d ...
 */
#define EP_DATA_LEN   64

#define EP_RX_LEN_ID   ((1<<15)|(1<<10))

#define EP_INT_MAX_LEN    8
#define EP_INT_LEN_ID    (4<<10)


// EP buffer address offsets
// EP0 = control
#define EP_CTRL_TX_OFFSET   0									// start: 0, length: 64
#define EP_CTRL_RX_OFFSET  (EP_CTRL_TX_OFFSET + EP_DATA_LEN)	// start: +64, length: 64

// EP1 = Bulk-IN+OUT for DATA
#define EP_DATA_TX_OFFSET  (EP_CTRL_RX_OFFSET + EP_DATA_LEN)	// start: +64, length: 64
#define EP_DATA_RX_OFFSET  (EP_DATA_TX_OFFSET + EP_DATA_LEN)	// start: +64, length: 64

// EP2 = Bulk-IN+OUT for COMM
#define EP_COMM_TX_OFFSET  (EP_DATA_RX_OFFSET + EP_DATA_LEN)	// start: +64, length: 8
#define EP_COMM_RX_OFFSET  (EP_COMM_TX_OFFSET + EP_INT_MAX_LEN)	// start: +8, length: 8


// Allocation of the EP buffers
#define USB_RAM       0x40006000
/**/
#define EP_CTRL_TX_BUF_ADDRESS	(USB_RAM + (EP_CTRL_TX_OFFSET<<UMEM_SHIFT))
#define EP_CTRL_RX_BUF_ADDRESS	(USB_RAM + (EP_CTRL_RX_OFFSET<<UMEM_SHIFT))

#define EP_DATA_TX_BUF_ADDRESS	(USB_RAM + (EP_DATA_TX_OFFSET<<UMEM_SHIFT))
#define EP_DATA_RX_BUF_ADDRESS	(USB_RAM + (EP_DATA_RX_OFFSET<<UMEM_SHIFT))

#define EP_COMM_TX_BUF_ADDRESS	(USB_RAM + (EP_COMM_TX_OFFSET<<UMEM_SHIFT))
#define EP_COMM_RX_BUF_ADDRESS	(USB_RAM + (EP_COMM_RX_OFFSET<<UMEM_SHIFT))


// EP table
typedef struct epTableEntry_t
{
    UMEM_FAKEWIDTH txOffset;
    UMEM_FAKEWIDTH txCount;
    UMEM_FAKEWIDTH rxOffset;
    UMEM_FAKEWIDTH rxCount;
} epTableEntry_t;

typedef struct epTableAddress_t {
    uint32_t * txAddr;
    uint32_t * rxAddr;
} epTableAddress_t;
extern const epTableAddress_t epTableAddr[2]; // number of EPs

#define EP_TABLE_OFFSET		400    // storing 64 bytes after 400

#define EpTable   ((epTableEntry_t *) (USB_RAM + (EP_TABLE_OFFSET<<UMEM_SHIFT)))



#endif // USB_DEF_H
