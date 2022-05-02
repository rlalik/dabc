// $Id$

/************************************************************
 * The Data Acquisition Backbone Core (DABC)                *
 ************************************************************
 * Copyright (C) 2009 -                                     *
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH      *
 * Planckstr. 1, 64291 Darmstadt, Germany                   *
 * Contact:  http://dabc.gsi.de                             *
 ************************************************************
 * This software can be used under the GPL license          *
 * agreements as stated in LICENSE.txt file                 *
 * which is part of the distribution.                       *
 ************************************************************/

#include "hadaq/FilterModule.h"

#include "hadaq/Iterator.h"

hadaq::FilterModule::FilterModule(const std::string &name, dabc::Command cmd) :
   dabc::ModuleAsync(name, cmd)
{
   // we need at least one input and one output port
   EnsurePorts(1, 1, dabc::xmlWorkPool);

   double flushtime = Cfg(dabc::xmlFlushTimeout, cmd).AsDouble(0.3);

   if (flushtime > 0.)
      CreateTimer("FlushTimer", flushtime);

   CreatePar("FilterData").SetRatemeter(false, 3.).SetUnits("MB");
   CreatePar("FilterEvents").SetRatemeter(false, 3.).SetUnits("Ev");

   // fTriggersRange = Cfg(hadaq::xmlHadaqTrignumRange, cmd).AsUInt(0x1000000);
}


bool hadaq::FilterModule::retransmit()
{
   int cnt = 20;

   while (CanSendToAllOutputs() && CanRecv() && CanTakeBuffer() && (cnt-- > 0)) {

      auto buf = Recv();
      if (buf.null()) break;

      if (buf.GetTypeId() == dabc::mbt_EOF) {
         SendToAllOutputs(buf);
         continue;
      }

      hadaq::ReadIterator iter(buf);

      hadaq::WriteIterator iter2(TakeBuffer());

      int numevents = 0;
      while (iter.NextSubeventsBlock()) {
         bool accept = true;
         while (iter.NextSubEvent()) {
            numevents++;
         }

         if (accept)
            if (!iter2.CopyEvent(iter))
               EOUT("Fail to copy event!!!");
      }

      auto outbuf = iter2.Close();
      if (!outbuf.null() && (outbuf.GetTotalSize() > 0)) {

         fEventRateCnt += numevents;
         fDataRateCnt += outbuf.GetTotalSize();

         Par("FilterData").SetValue(fDataRateCnt/1024./1024.);
         Par("FilterEvents").SetValue(fEventRateCnt);

         SendToAllOutputs(outbuf);
      }
   }

   return (cnt <= 0); // if cnt less than 0, reinject event
}


void hadaq::FilterModule::ProcessTimerEvent(unsigned)
{
   retransmit();
}

int hadaq::FilterModule::ExecuteCommand(dabc::Command cmd)
{
   return dabc::ModuleAsync::ExecuteCommand(cmd);
}
