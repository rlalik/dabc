/********************************************************************
 * The Data Acquisition Backbone Core (DABC)
 ********************************************************************
 * Copyright (C) 2009- 
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH 
 * Planckstr. 1
 * 64291 Darmstadt
 * Germany
 * Contact:  http://dabc.gsi.de
 ********************************************************************
 * This software can be used under the GPL license agreements as stated
 * in LICENSE.txt file which is part of the distribution.
 ********************************************************************/
#ifndef PCI_BoardDevice
#define PCI_BoardDevice

#include "dabc/Device.h"
#include "dabc/Basic.h"
#include "dabc/Buffer.h"

#include <queue>

#include "mprace/DMABuffer.h"

class mprace::Board;

class dabc::MemoryPool;
class dabc::Command;

namespace pci {

class BoardDevice :  public dabc::Device {
   public:

        BoardDevice();

        BoardDevice(Basic* parent, const char* name, bool usedma);

        virtual ~BoardDevice();

        /** Map buffers of assigned pool into DMA buffer objects.*/
        bool MapDMABuffers(dabc::MemoryPool* pool);

        void CleanupDMABuffers();


        /**
         * Write this buffer to the pci board. Depending
         * on DMA mode, this is done by DMA or with block PIO.
         * returns size of written data in bytes, or negative value as specified for DataTransport::Read_Complete
         * This method will block until write is complete!
         */
        virtual int WritePCI(dabc::Buffer* buf);



        /**
         * Read next buffer from the pci board. Depending
         * on DMA mode, this is done by DMA or with block PIO.
         * returns size of read data in bytes, or negative value as specified for DataTransport::Read_Complete
         * This method will block until read is complete!
         */
        virtual int ReadPCI(dabc::Buffer* buf);




         /**
         * Start reading of next buffer from the pci board. Depending
         * on DMA mode, this is done by DMA or with block PIO.
         * returns true if reading started successfully.
         * This method will return before read is complete!
         */
        virtual bool ReadPCIStart(dabc::Buffer* buf);

         /**
         * Wait until reading of the buffer from the pci board is complete. Depending
         * on DMA mode, reading is done by DMA or with block PIO.
         * returns size of read data in bytes, or negative value as specified for DataTransport::Read_Complete
         * Requires to call ReadPCIBegin for this buffer before.
         */
        virtual int ReadPCIComplete(dabc::Buffer* buf);

       unsigned int GetReadLength() { return fReadLength; }

       unsigned int GetWriteLength() { return fWriteLength; }


        /** Define memory range on PCI board for reading.
          * specifies bar, start address and length*/
        void SetReadBuffer(unsigned int bar, unsigned int address, unsigned int length)
            {
                fReadBAR=bar;
                fReadAddress=address;
                fReadLength=length;
            }
          /** Define memory range on PCI board for writing.
          * specifies bar, start address and length*/
        void SetWriteBuffer(unsigned int bar, unsigned int address, unsigned int length)
            {
                fWriteBAR=bar;
                fWriteAddress=address;
                fWriteLength=length;
            }


         virtual int ExecuteCommand(dabc::Command* cmd);

         virtual int CreateTransport(dabc::Command* cmd, dabc::Port* port);


    protected:


        virtual bool DoDeviceCleanup(bool full = false);


            /** read dma into a locked buffer buf; save against operations on dma buffer list*/
        bool ReadDMA(const unsigned int address, mprace::DMABuffer& buf, const unsigned int count);

          /** write dma from locked buffer buf; save against operations on dma buffer list*/
        bool WriteDMA(const unsigned int address, mprace::DMABuffer& buf, const unsigned int count);


            /** Deliver DMA buffer that maps memory pool buffer buf.*/
        mprace::DMABuffer* GetDMABuffer(dabc::Buffer* buf);

        void LockDMABuffers(){fMutex.Lock();}

        void UnlockDMABuffers(){fMutex.Unlock();}

        /**
         * if true, use DMA for data transfer; otherwise PIO
         */
        bool fEnableDMA;

        /**
         *@link aggregation
         *@supplierCardinality 1
         */
        mprace::Board * fBoard;

        /** BAR number of board memory used for Read*/
        unsigned int fReadBAR;

        /** Start address of board memory buffer for Read.
          * this is relative to read BAR number*/
        unsigned int fReadAddress;

        /** Length of board memory buffer (integers) for Read. */
        unsigned int fReadLength;

         /** BAR number of board memory used for Write*/
        unsigned int fWriteBAR;

        /** Start address of board memory buffer for Write.
          * this is relative to write BAR number*/
        unsigned int fWriteAddress;

        /** Length of board memory buffer (integers) for next write*/
        unsigned int fWriteLength;
private:
        /** keep mapped dma buffers. Memory is owned by exernal pool*/
        std::vector<mprace::DMABuffer*> fDMABuffers;

        /** true if list of dma buffers must be remapped from dabc mempool*/
        bool fDMARemapRequired;


        /** protects list of dma buffer mappings */
        dabc::Mutex                  fMutex;


        /** conter for transport threads, for unique naming.*/
        static unsigned int fgThreadnum;
};

}
#endif //PCIBOARDDEVICE_H
