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
#include "TestGeneratorModule.h"

#include "dabc/logging.h"
#include "dabc/Buffer.h"
#include "dabc/Pointer.h"

#include "bnet/common.h"

bnet::TestGeneratorModule::TestGeneratorModule(const std::string& name, dabc::Command cmd) :
   dabc::ModuleAsync(name, cmd),
   fEventCnt(0)
{
   fUniquieId = Cfg("UniqueId",cmd).AsInt(0);

   fBufferSize = Cfg(xmlReadoutBuffer,cmd).AsInt(1024);

   CreatePoolHandle(Cfg(CfgReadoutPool,cmd).AsStr(ReadoutPoolName));

   CreateOutput("Output", Pool(), ReadoutQueueSize);

   DOUT1("Test Generator %s: UniqueId:%llu", GetName(), fUniquieId);
}

void bnet::TestGeneratorModule::ProcessOutputEvent(unsigned indx)
{
   if (!CanSend(indx)) {
      EOUT("Should not happen with generator");
      return;
   }

   dabc::Buffer* buf = Pool()->TakeBuffer(fBufferSize);
   if (buf==0) {
      EOUT("No free buffer - generator will block");
      return;
   }

   buf->SetDataSize(fBufferSize);

   fEventCnt++;

   dabc::Pointer ptr(buf);

   ptr.copyfrom(&fEventCnt, sizeof(fEventCnt));
   ptr+=sizeof(fEventCnt);

   ptr.copyfrom(&fUniquieId, sizeof(fUniquieId));
   ptr+=sizeof(fUniquieId);

   DOUT3("Generate packet id %llu of size %d need %d", fUniquieId, buf->GetTotalSize(), fBufferSize);

   Send(indx, buf);
}
