#ifndef BNET_TestBuilderModule
#define BNET_TestBuilderModule

#include "dabc/ModuleAsync.h"

#include <vector>

namespace bnet {
    
   class WorkerPlugin;
   
   class TestBuilderModule : public dabc::ModuleAsync {
      protected:
         dabc::PoolHandle*  fInpPool;
         dabc::PoolHandle*  fOutPool;
         int                  fNumSenders;
         int                  fOutBufferSize;
         
         std::vector<dabc::Buffer*> fBuffers;
         
      public:
         TestBuilderModule(dabc::Manager* m, const char* name, WorkerPlugin* factory);
         virtual ~TestBuilderModule();
         
         virtual void ProcessUserEvent(dabc::ModuleItem* item, uint16_t evid);

         virtual void BeforeModuleStart();
         virtual void AfterModuleStop();
   };   
}

#endif
