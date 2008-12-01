#include "mbs/GeneratorModule.h"

#include <math.h>

#include "dabc/Port.h"
#include "dabc/PoolHandle.h"
#include "dabc/Manager.h"
#include "mbs/Iterator.h"

double Gauss_Rnd(double mean, double sigma)
{

   double x, y, z;

//   srand(10);

   z = 1.* rand() / RAND_MAX;
   y = 1.* rand() / RAND_MAX;

   x = z * 6.28318530717958623;
   return mean + sigma*sin(x)*sqrt(-2*log(y));
}


mbs::GeneratorModule::GeneratorModule(const char* name) :
   dabc::ModuleAsync(name)
{
   fEventCount = 0;
   fNumSubevents = GetCfgInt("NumSubevents", 2);
   fFirstProcId = GetCfgInt("FirstProcId", 0);
   fSubeventSize = GetCfgInt("SubeventSize", 32);
   fIsGo4RandomFormat = GetCfgBool("Go4Random", true);
   fBufferSize = GetCfgInt(dabc::xmlBufferSize, 16*1024);

   fPool = CreatePool("Pool", 10, fBufferSize);

   CreateOutput("Output", fPool, 5);
}

mbs::GeneratorModule::~GeneratorModule()
{

}

void mbs::GeneratorModule::BeforeModuleStart()
{
   for (unsigned n=0;n<Output(0)->OutputQueueCapacity();n++)
      GeneratePacket();
}

void mbs::GeneratorModule::ProcessOutputEvent(dabc::Port* port)
{
   GeneratePacket();
}

bool mbs::GeneratorModule::GeneratePacket()
{
   if (Output(0)->OutputBlocked()) return false;

   dabc::Buffer* buf = fPool->TakeBuffer(fBufferSize, false);
   if (buf==0) { EOUT(("AAAAAAAAAAAA")); return false; }

   if (fIsGo4RandomFormat) {
      if (!GenerateGo4RandomPacket(buf)) {
         dabc::Buffer::Release(buf);
         return false;
      }
   } else {
      EOUT(("Not implemented"));
      dabc::Buffer::Release(buf);
      return false;
   }

   Output(0)->Send(buf);

   return true;

}

bool mbs::GeneratorModule::GenerateGo4RandomPacket(dabc::Buffer* buf)
{
   if (buf==0) return false;

   unsigned numval = fSubeventSize / sizeof(uint32_t);

   mbs::WriteIterator iter(buf);

   while (iter.NewEvent(fEventCount)) {

      bool eventdone = true;

      for (unsigned subcnt = 0; subcnt < fNumSubevents; subcnt++)
         if (iter.NewSubevent(numval * sizeof(uint32_t), 0, fFirstProcId+subcnt)) {
            uint32_t* value = (uint32_t*) iter.rawdata();

            for (unsigned nval=0;nval<numval;nval++)
               *value++ = (uint32_t) Gauss_Rnd(nval*100 + 2000, 500./(nval+1));

            iter.FinishSubEvent(numval * sizeof(uint32_t));
         } else {
            eventdone = false;
            break;
         }

      if (!eventdone) break;

      if (!iter.FinishEvent()) break;

      fEventCount++;
   }

   return true;
}


extern "C" void StartMbsGenerator()
{
    if (dabc::mgr()==0) {
       EOUT(("Manager is not created"));
       exit(1);
    }

    DOUT0(("Start MBS generator module"));

    dabc::Module* m = new mbs::GeneratorModule("Generator");
    dabc::mgr()->MakeThreadForModule(m);
    dabc::mgr()->CreateMemoryPools();

    dabc::Command* cmd = new dabc::CmdCreateTransport("Modules/Generator/Ports/Output", mbs::typeServerTransport, "MbsTransport");
    cmd->SetStr(mbs::xmlServerKind, mbs::ServerKindToStr(mbs::TransportServer));
//    cmd->SetInt(dabc::xmlBufferSize, BUFFERSIZE);
    if (!dabc::mgr()->Execute(cmd)) {
       EOUT(("Cannot create MBS transport server"));
       exit(1);
    }

    m->Start();
}


class MbsTestReadoutModule : public dabc::ModuleAsync {

   public:
      MbsTestReadoutModule(const char* name) :
         dabc::ModuleAsync(name)
      {
         dabc::BufferSize_t size = GetCfgInt(dabc::xmlBufferSize, 16*1024);

         dabc::PoolHandle* pool = CreatePool("Pool1", 5, size);

         CreateInput("Input", pool, 5);
      }

      virtual void ProcessInputEvent(dabc::Port* port)
      {
         dabc::Buffer* buf = 0;
         port->Recv(buf);

         unsigned cnt = mbs::ReadIterator::NumEvents(buf);

         DOUT1(("Found %u events in MBS buffer", cnt));

         dabc::Buffer::Release(buf);
      }

      virtual void AfterModuleStop()
      {
      }
};

extern "C" void StartMbsClient()
{
   const char* hostname = "lxg0526";
   int nport = 6000;

   if (dabc::mgr()==0) {
      EOUT(("Manager is not created"));
      exit(1);
   }

   DOUT1(("Start as client for node %s:%d", hostname, nport));

   dabc::Module* m = new MbsTestReadoutModule("Receiver");

   dabc::mgr()->MakeThreadForModule(m);

   dabc::mgr()->CreateMemoryPools();

   dabc::Command* cmd = new dabc::CmdCreateTransport("Modules/Receiver/Ports/Input", mbs::typeClientTransport, "MbsTransport");
   cmd->SetStr(mbs::xmlServerKind, mbs::ServerKindToStr(mbs::TransportServer));
   cmd->SetStr(mbs::xmlServerName, hostname);
   if (nport>0) cmd->SetInt(mbs::xmlServerPort, nport);
//   cmd->SetInt(dabc::xmlBufferSize, BUFFERSIZE);

   if (!dabc::mgr()->Execute(cmd)) {
      EOUT(("Cannot create data input for receiver"));
      exit(1);
   }

   m->Start();

//   dabc::LongSleep(3);
}
