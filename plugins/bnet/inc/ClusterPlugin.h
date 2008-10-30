#ifndef BNET_ClusterPlugin
#define BNET_ClusterPlugin

#include "dabc/ApplicationPlugin.h"

#include "dabc/collections.h"

#include <vector>

namespace bnet {
    
   class ClusterPlugin : public dabc::ApplicationPlugin {
      
      friend class ClusterModule; 
       
      public:
         ClusterPlugin(dabc::Manager* m);
         virtual ~ClusterPlugin();
         
         virtual dabc::Module* CreateModule(const char* classname, const char* modulename, dabc::Command* cmd);
         
         virtual bool CreateAppModules();
         
         const char* NetDevice() const { return GetParCharStar("NetDevice", "SocketDevice"); }
         bool IsRunning() const { return GetParInt("IsRunning") > 0; }
         bool WithController() const { return GetParInt("WithController", 0) > 0; }
         int NumEventsCombine() const { return GetParInt("NumEventsCombine", 1); }
         
         const char* ControlPoolName() const { return "ControlPool"; }
         uint64_t    ControlBufferSize() const { return GetParInt("CtrlBuffer", 2*1024); }
         uint64_t    ControlPoolSize() const { return GetParInt("CtrlPoolSize", 4*0x100000); }
         
         uint64_t    TransportBufferSize() const { return GetParInt("TransportBuffer", 8*1024); }

         int   CfgNumNodes() const { return GetParInt("CfgNumNodes", 1); }
         int   CfgNodeId() const { return GetParInt("CfgNodeId", 0); }
         
         void SetPars(bool withcontrol,
                      int ControlBufferSize,
                      int TransportBufferSize,
                      int eventscombine = 1);

         virtual int ExecuteCommand(dabc::Command* cmd);

         // this is public method, which must be used from SM thread  
         virtual bool DoStateTransition(const char* state_trans_name);
         
      protected:
      
         const char* NodeCurrentState(int nodeid);
      
         bool ActualTransition(const char* state_trans_name);
      
         enum NodeMaskValues {
            mask_Sender = 0x1,
            mask_Receiever = 0x2 };

         bool ExecuteClusterSMCommand(const char* smcmdname);

         bool StartDiscoverConfig(dabc::Command* mastercmd);
         bool StartClusterSMCommand(dabc::Command* mastercmd);
         bool StartModulesConnect(dabc::Command* mastercmd);
         bool StartConfigureApply(dabc::Command* mastercmd);

         virtual void ParameterChanged(dabc::Parameter* par);
         
         std::vector<int> fNodeMask; // info about items on each node
         std::vector<dabc::String> fNodeNames; // info about names of currently configured nodes
         std::vector<dabc::String> fSendMatrix; // info about send connections
         std::vector<dabc::String> fRecvMatrix; // info about recv connections
         
         dabc::Mutex fSMMutex;
         dabc::String fSMRunningSMCmd;
         
   };
}

extern "C" dabc::ApplicationPlugin* ClusterPluginsInit(dabc::Manager* mgr);

#endif
