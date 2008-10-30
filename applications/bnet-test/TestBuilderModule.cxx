#include "TestBuilderModule.h"

#include "dabc/logging.h"
#include "dabc/PoolHandle.h"
#include "dabc/Command.h"
#include "dabc/Port.h"
#include "dabc/Parameter.h"

#include "bnet/common.h"
#include "bnet/WorkerPlugin.h"

bnet::TestBuilderModule::TestBuilderModule(dabc::Manager* m, const char* name, WorkerPlugin* factory) :
   dabc::ModuleAsync(m, name),
   fInpPool(0),
   fOutPool(0),
   fNumSenders(1)
{
   fOutPool = CreatePool(factory->EventPoolName());

   CreateOutput("Output", fOutPool, BuilderOutQueueSize);

   fInpPool = CreatePool(factory->TransportPoolName());

   CreateInput("Input", fInpPool, BuilderInpQueueSize, sizeof(bnet::SubEventNetHeader));
   
   fOutBufferSize = factory->EventBufferSize();
   
   new dabc::StrParameter(this, "SendMask", "xxxx");
}

bnet::TestBuilderModule::~TestBuilderModule()
{
   for (unsigned n=0; n<fBuffers.size(); n++)
      dabc::Buffer::Release(fBuffers[n]);
   fBuffers.clear();
}

void bnet::TestBuilderModule::ProcessUserEvent(dabc::ModuleItem*, uint16_t)
{
   while (fBuffers.size() < (unsigned) fNumSenders) {
      if (Input(0)->InputBlocked()) return; 
      dabc::Buffer* buf = 0;
      Input(0)->Recv(buf);
      if (buf==0) return;
      fBuffers.push_back(buf);
   }

   if (Output(0)->OutputBlocked()) return;

   dabc::Buffer* outbuf = fOutPool->TakeBuffer(fOutBufferSize, true);
   if (outbuf==0) return;

   uint64_t evid = 0;
      
   for (unsigned n=0; n<fBuffers.size(); n++) {
      uint64_t* mem = (uint64_t*) fBuffers[n]->GetDataLocation(); 
      if (evid==0) evid = *mem; else
        if (evid!=*mem) { 
            EOUT(("Missmatch in events id %llu %llu", evid, *mem));
            exit(1);
        }
          
      dabc::Buffer::Release(fBuffers[n]);
   }
      
   fBuffers.clear();
      
   uint64_t* mem = (uint64_t*) outbuf->GetDataLocation();
   *mem = evid;
      
   Output(0)->Send(outbuf);

//   DOUT1(("!!!!!!! SEND EVENT %llu DONE", evid));
}

void bnet::TestBuilderModule::BeforeModuleStart()
{
   fNumSenders = bnet::NodesVector(GetParCharStar("SendMask")).size();
   
//   DOUT1(("TestBuilderModule::BeforeModuleStart numsend = %d", fNumSenders));
}

void bnet::TestBuilderModule::AfterModuleStop()
{
}
