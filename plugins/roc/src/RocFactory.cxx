#include "roc/RocFactory.h" 
#include "roc/RocCombinerModule.h"
#include "roc/RocCalibrModule.h"
#include "roc/RocDevice.h" 
#include "roc/RocTreeOutput.h" 
//#include "dabc/Manager.h"
#include "dabc/Command.h"
//#include "dabc/Parameter.h"

#include "dabc/logging.h"


roc::RocFactory roc::RocFactory::gRocFactory;

roc::RocFactory::RocFactory() 
 : dabc::Factory(DABC_ROC_NAME_PLUGIN)
{ 
   DOUT1(("-----------RocFactory::CONSTRUCTOR!"));
}


 
dabc::Module* roc::RocFactory::CreateModule(const char* classname, const char* modulename, dabc::Command* cmd) 
{
   DOUT1(("RocFactory::CreateModule called for class:%s, module:%s", classname, modulename));

   if (strcmp(classname,"RocCombinerModule")==0) {
      dabc::Module* mod= new roc::RocCombinerModule(GetManager(),modulename,cmd);
      DOUT1(("RocFactory::CreateModule - Created RocCombiner module %s ", modulename));
      return mod;     
   } else 
   if (strcmp(classname,"RocCalibrModule")==0) {
      dabc::Module* mod = new roc::RocCalibrModule(GetManager(), modulename, cmd);
      DOUT1(("RocFactory::CreateModule - Created RocCalibrModule module %s ", modulename));
      return mod;     
   } 
   
   return 0;   
}

dabc::Device* roc::RocFactory::CreateDevice(dabc::Basic* parent, const char* classname, const char* devname, dabc::Command* cmd)
{
   if (strcmp(classname,"RocDevice")!=0) return 0;
   
   DOUT1(("RocFactory::CreateDevice - Creating  ROC device %s ...", devname));   
   roc::RocDevice* dev = new  roc::RocDevice(parent, devname); 
   return dev;    
}

dabc::DataOutput* roc::RocFactory::CreateDataOutput(const char* typ, const char* name, dabc::Command* cmd)
{
   if (strcmp(typ, "RocTreeOutput")!=0) return 0;
   
   roc::RocTreeOutput* out = new roc::RocTreeOutput(name, cmd->GetInt("SizeLimit", 0));
   
   if (!out->IsOk()) { delete out; out = 0; }
   
   return out;
}






