#ifndef MBS_GeneratorModule
#define MBS_GeneratorModule

#ifndef DABC_ModuleAsync
#include "dabc/ModuleAsync.h"
#endif

namespace mbs {

   class GeneratorModule : public dabc::ModuleAsync {

      protected:

         dabc::PoolHandle*       fPool;

         dabc::BufferSize_t      fBufferSize;

         uint32_t fEventCount;
         uint16_t fNumSubevents;
         uint16_t fFirstProcId;
         uint32_t fSubeventSize;
         bool fIsGo4RandomFormat;

      public:
         GeneratorModule(const char* name);
         virtual ~GeneratorModule();

         virtual void BeforeModuleStart();
         virtual void ProcessOutputEvent(dabc::Port* port);


         bool GeneratePacket();

         bool GenerateGo4RandomPacket(dabc::Buffer* buf);
   };


}


#endif
