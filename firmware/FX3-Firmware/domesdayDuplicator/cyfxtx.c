/*
 ## Cypress USB 3.0 Platform source file (cyfxtx.c)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2011,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
*/

/* This file defines the porting required for the ThreadX RTOS.
 * This file shall be provided in source form and must be compiled
 * with the application source code
 */

#include <cyu3os.h>
#include <cyu3error.h>

/*
   The MEM heap is a Memory byte pool which is used to allocate OS objects
   such as thread stacks and memory for message queues. The Cypress FX3
   libraries require a Mem heap size of at least 32 KB.
 */
#define CY_U3P_MEM_HEAP_BASE         ((uint8_t *)0x40038000)
#define CY_U3P_MEM_HEAP_SIZE         (0x8000)

/* The last 32 KB of RAM is reserved for 2-stage boot operation. This value can be changed to
   0x40080000 if 2-stage boot is not used by the application. */
#define CY_U3P_SYS_MEM_TOP           (0x40078000)

/*
   The buffer heap is used to obtain data buffers for DMA transfers in or out of
   the FX3 device. The reference implementation of the buffer allocator makes use
   of a reserved area in the SYSTEM RAM and ensures that all allocated DMA buffers
   are aligned to cache lines.
 */
#define CY_U3P_BUFFER_HEAP_BASE      (((uint32_t)(CY_U3P_MEM_HEAP_BASE) + (CY_U3P_MEM_HEAP_SIZE)))
#define CY_U3P_BUFFER_HEAP_SIZE      ((CY_U3P_SYS_MEM_TOP) - (CY_U3P_BUFFER_HEAP_BASE))

#define CY_U3P_BUFFER_ALLOC_TIMEOUT  (10)
#define CY_U3P_MEM_ALLOC_TIMEOUT     (10)

#define CY_U3P_MAX(a,b)                 (((a) > (b)) ? (a) : (b))
#define CY_U3P_MIN(a,b)                 (((a) < (b)) ? (a) : (b))

CyBool_t         glMemPoolInit = CyFalse;
CyU3PBytePool    glMemBytePool;
CyU3PDmaBufMgr_t glBufferManager = {{0}, 0, 0, 0, 0, 0};

/* These functions are exception handlers. These are default
 * implementations and the application firmware can have a
 * re-implementation. All these exceptions are not currently
 * handled and are mapped to while (1) */

/* This function is the undefined instruction handler. This
 * occurs when the CPU encounters an undefined instruction. */
void
CyU3PUndefinedHandler (
        void)
{
    for (;;);
}

/* This function is the intruction prefetch error handler. This
 * occurs when the CPU encounters an instruction prefetch error.
 * Since there are no virtual memory use case, this is an unknown
 * memory access error. This is a fatal error. */
void
CyU3PPrefetchHandler (
        void)
{
    for (;;);
}

/* This function is the data abort error handler. This occurs when
 * the CPU encounters an data prefetch error. Since there are no
 * virtual memory use case, this is an unknown memory access error.
 * This is a fatal error. */
void
CyU3PAbortHandler (
        void)
{
    for (;;);
}

/* This function is expected to be invoked by the RTOS kernel after
 * initialization. No explicit call to this function must be made.
 */
void
tx_application_define (
        void *unusedMem)
{
    (void) unusedMem;
    CyU3PApplicationDefine ();
}

/* This function initializes the custom heap for OS specific dynamic memory allocation.
 * The function should not be explicitly invoked. This function is called from the 
 * API library. Modify this function depending upon the heap requirement of 
 * application code. The minimum required value is specified by the predefined macro.
 * Any value less than specified can cause the drivers to stop functioning.
 * The function creates a global byte pool.
 */
void
CyU3PMemInit (
        void)
{
    if (!glMemPoolInit)
    {
	glMemPoolInit = CyTrue;
	CyU3PBytePoolCreate (&glMemBytePool, CY_U3P_MEM_HEAP_BASE, CY_U3P_MEM_HEAP_SIZE);
    }
}

void *
CyU3PMemAlloc (
        uint32_t size)
{
    void     *ret_p;
    uint32_t status;

    /* Cannot wait in interrupt context */
    if (CyU3PThreadIdentify ())
    {
        status = CyU3PByteAlloc (&glMemBytePool, (void **)&ret_p, size, CY_U3P_MEM_ALLOC_TIMEOUT);
    }
    else
    {
        status = CyU3PByteAlloc (&glMemBytePool, (void **)&ret_p, size, CYU3P_NO_WAIT);
    }

    if(status == CY_U3P_SUCCESS)
    {
        return ret_p;
    }

    return (NULL);
}

void
CyU3PMemFree (
        void *mem_p)
{
    CyU3PByteFree (mem_p);
}

void
CyU3PMemSet (
        uint8_t *ptr,
        uint8_t data,
        uint32_t count)
{
    /* Loop unrolling for faster operation */
    while (count >> 3)
    {
        ptr[0] = data;
        ptr[1] = data;
        ptr[2] = data;
        ptr[3] = data;
        ptr[4] = data;
        ptr[5] = data;
        ptr[6] = data;
        ptr[7] = data;

        count -= 8;
        ptr += 8;
    }

    while (count--)
    {
        *ptr = data;
        ptr++;
    }
}

void
CyU3PMemCopy (
        uint8_t *dest, 
        uint8_t *src,
        uint32_t count)
{
    /* Loop unrolling for faster operation */
    while (count >> 3)
    {
        dest[0] = src[0];
        dest[1] = src[1];
        dest[2] = src[2];
        dest[3] = src[3];
        dest[4] = src[4];
        dest[5] = src[5];
        dest[6] = src[6];
        dest[7] = src[7];

        count -= 8;
        dest += 8;
        src += 8;
    }

    while (count--)
    {
        *dest = *src;
        dest++;
        src++;
    }
}

int32_t 
CyU3PMemCmp (
        const void* s1,
        const void* s2, 
        uint32_t n)
{
    const uint8_t *ptr1 = s1, *ptr2 = s2;

    while(n--)
    {
        if(*ptr1 != *ptr2)
        {
            return *ptr1 - *ptr2;
        }
        
        ptr1++;
        ptr2++;
    }  
    return 0;
}

/* This function shall be invoked by the API library 
 * and should not be explicitly invoked.
 * If other buffer sizes are required by the application code, this function must
 * be modified to create other block pools.
 */
void
CyU3PDmaBufferInit (
        void)
{
    uint32_t status, size;
    uint32_t tmp;

    /* If buffer manager has already been initialized, just return. */
    if ((glBufferManager.startAddr != 0) && (glBufferManager.regionSize != 0))
    {
        return;
    }

    /* Create a mutex variable for safe allocation. */
    status = CyU3PMutexCreate (&glBufferManager.lock, CYU3P_NO_INHERIT);
    if (status != CY_U3P_SUCCESS)
    {
        return;
    }

    /* No threads are running at this point in time. There is no need to
       get the mutex. */

    /* Allocate the memory buffer to be used to track memory status.
       We need one bit per 32 bytes of memory buffer space. Since a 32
       bit array is being used, round up to the necessary number of
       32 bit words. */
    size = ((CY_U3P_BUFFER_HEAP_SIZE / 32) + 31) / 32;
    glBufferManager.usedStatus = (uint32_t *)CyU3PMemAlloc (size * 4);
    if (glBufferManager.usedStatus == 0)
    {
        CyU3PMutexDestroy (&glBufferManager.lock);
        return;
    }

    /* Initially mark all memory as available. If there are any status bits
       beyond the valid memory range, mark these as unavailable. */
    CyU3PMemSet ((uint8_t *)glBufferManager.usedStatus, 0, (size * 4));
    if ((CY_U3P_BUFFER_HEAP_SIZE / 32) & 31)
    {
        tmp = 32 - ((CY_U3P_BUFFER_HEAP_SIZE / 32) & 31);
        glBufferManager.usedStatus[size - 1] = ~((1 << tmp) - 1);
    }

    /* Initialize the start address and region size variables. */
    glBufferManager.startAddr  = CY_U3P_BUFFER_HEAP_BASE;
    glBufferManager.regionSize = CY_U3P_BUFFER_HEAP_SIZE;
    glBufferManager.statusSize = size;
    glBufferManager.searchPos  = 0;
}

/* This function shall be invoked by the API library 
 * and should not be explicitly invoked.
 */
void
CyU3PDmaBufferDeInit (
        void)
{
    uint32_t status;

    /* Get the mutex lock. */
    if (CyU3PThreadIdentify ())
    {
        status = CyU3PMutexGet (&glBufferManager.lock, CYU3P_WAIT_FOREVER);
    }
    else
    {
        status = CyU3PMutexGet (&glBufferManager.lock, CYU3P_NO_WAIT);
    }

    if (status != CY_U3P_SUCCESS)
    {
        return;
    }

    /* Free memory and zero out variables. */
    CyU3PMemFree (glBufferManager.usedStatus);
    glBufferManager.usedStatus = 0;
    glBufferManager.startAddr  = 0;
    glBufferManager.regionSize = 0;
    glBufferManager.statusSize = 0;

    /* Free up and destroy the mutex variable. */
    CyU3PMutexPut (&glBufferManager.lock);
    CyU3PMutexDestroy (&glBufferManager.lock);
}

/* Helper function for the DMA buffer manager. Used to set/clear
   a set of status bits from the alloc/free functions. */
static void
CyU3PDmaBufMgrSetStatus (
        uint32_t startPos,
        uint32_t numBits,
        CyBool_t value)
{
    uint32_t wordnum  = (startPos >> 5);
    uint32_t startbit, endbit, mask;

    startbit = (startPos & 31);
    endbit   = CY_U3P_MIN (32, startbit + numBits);

    /* Compute a mask that has a 1 at all bit positions to be altered. */
    mask  = (endbit == 32) ? 0xFFFFFFFFU : ((uint32_t)(1 << endbit) - 1);
    mask -= ((1 << startbit) - 1);

    /* Repeatedly go through the array and update each 32 bit word as required. */
    while (numBits)
    {
        if (value)
        {
            glBufferManager.usedStatus[wordnum] |= mask;
        }
        else
        {
            glBufferManager.usedStatus[wordnum] &= ~mask;
        }

        wordnum++;
        numBits -= (endbit - startbit);
        if (numBits >= 32)
        {
            startbit = 0;
            endbit   = 32;
            mask     = 0xFFFFFFFFU;
        }
        else
        {
            startbit = 0;
            endbit   = numBits;
            mask     = ((uint32_t)(1 << numBits) - 1);
        }
    }
}

/* This function shall be invoked from the DMA module for buffer allocation */
void *
CyU3PDmaBufferAlloc (
        uint16_t size)
{
    uint32_t tmp;
    uint32_t wordnum, bitnum;
    uint32_t count, start = 0;
    void *ptr = 0;

    /* Get the lock for the buffer manager. */
    if (CyU3PThreadIdentify ())
    {
        tmp = CyU3PMutexGet (&glBufferManager.lock, CY_U3P_BUFFER_ALLOC_TIMEOUT);
    }
    else
    {
        tmp = CyU3PMutexGet (&glBufferManager.lock, CYU3P_NO_WAIT);
    }

    if (tmp != CY_U3P_SUCCESS)
    {
        return ptr;
    }

    /* Make sure the buffer manager has been initialized. */
    if ((glBufferManager.startAddr == 0) || (glBufferManager.regionSize == 0))
    {
        CyU3PMutexPut (&glBufferManager.lock);
        return ptr;
    }

    /* Find the number of 32 byte chunks required. The minimum size that can be handled is
       64 bytes. */
    size = (size <= 32) ? 2 : (size + 31) / 32;

    /* Search through the status array to find the first block that fits the need. */
    wordnum = glBufferManager.searchPos;
    bitnum  = 0;
    count   = 0;
    tmp     = 0;

    /* Stop searching once we have checked all of the words. */
    while (tmp < glBufferManager.statusSize)
    {
        if ((glBufferManager.usedStatus[wordnum] & (1 << bitnum)) == 0)
        {
            if (count == 0)
            {
                start = (wordnum << 5) + bitnum + 1;
            }
            count++;
            if (count == (size + 1))
            {
                /* The last bit corresponding to the allocated memory is left as zero.
                   This allows us to identify the end of the allocated block while freeing
                   the memory. We need to search for one additional zero while allocating
                   to account for this hack. */
                glBufferManager.searchPos = wordnum;
                break;
            }
        }
        else
        {
            count = 0;
        }

        bitnum++;
        if (bitnum == 32)
        {
            bitnum = 0;
            wordnum++;
            tmp++;
            if (wordnum == glBufferManager.statusSize)
            {
                /* Wrap back to the top of the array. */
                wordnum = 0;
                count   = 0;
            }
        }
    }

    if (count == (size + 1))
    {
        /* Mark the memory region identified as occupied and return the pointer. */
        CyU3PDmaBufMgrSetStatus (start, size - 1, CyTrue);
        ptr = (void *)(glBufferManager.startAddr + (start << 5));
    }

    CyU3PMutexPut (&glBufferManager.lock);
    return (ptr);
}

/* This function shall be invoked from the DMA module for buffer de-allocation */
int
CyU3PDmaBufferFree (
        void *buffer)
{
    uint32_t status, start, count;
    uint32_t wordnum, bitnum;
    int      retVal = -1;

    /* Get the lock for the buffer manager. */
    if (CyU3PThreadIdentify ())
    {
        status = CyU3PMutexGet (&glBufferManager.lock, CY_U3P_BUFFER_ALLOC_TIMEOUT);
    }
    else
    {
        status = CyU3PMutexGet (&glBufferManager.lock, CYU3P_NO_WAIT);
    }

    if (status != CY_U3P_SUCCESS)
    {
        return retVal;
    }

    /* If the buffer address is within the range specified, count the number of consecutive ones and
       clear them. */
    start = (uint32_t)buffer;
    if ((start > glBufferManager.startAddr) && (start < (glBufferManager.startAddr + glBufferManager.regionSize)))
    {
        start = ((start - glBufferManager.startAddr) >> 5);

        wordnum = (start >> 5);
        bitnum  = (start & 0x1F);
        count   = 0;

        while ((wordnum < glBufferManager.statusSize) && ((glBufferManager.usedStatus[wordnum] & (1 << bitnum)) != 0))
        {
            count++;
            bitnum++;
            if (bitnum == 32)
            {
                bitnum = 0;
                wordnum++;
            }
        }

        CyU3PDmaBufMgrSetStatus (start, count, CyFalse);

        /* Start the next buffer search at the top of the heap. This can help reduce fragmentation in cases where
           most of the heap is allocated and then freed as a whole. */
        glBufferManager.searchPos = 0;
        retVal = 0;
    }

    /* Free the lock before we go. */
    CyU3PMutexPut (&glBufferManager.lock);
    return retVal;
}

void
CyU3PFreeHeaps (
	void)
{
    /* Free up the mem and buffer heaps. */
    CyU3PDmaBufferDeInit ();
    CyU3PBytePoolDestroy (&glMemBytePool);
    glMemPoolInit = CyFalse;
}

/* [ ] */

