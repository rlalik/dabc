#include "dabc/ApplicationPlugin.h"

#include "dabc/Manager.h"
#include "dabc/logging.h"

dabc::ApplicationPlugin::ApplicationPlugin(Manager* m, const char* name, const char* thrdname) :
   Folder(m->GetPluginFolder(true), name, true),
   WorkingProcessor(this),
   fConnCmd(0),
   fConnTmout(0)
{
   DOUT3(("Plugin %s created", GetName())); 

   if ((thrdname==0) || (strlen(thrdname)==0))
      thrdname = Manager::MgrThrdName();

   m->MakeThreadFor(this, thrdname);
}


dabc::ApplicationPlugin::~ApplicationPlugin()
{
   DOUT3(("Start ApplicationPlugin %s destructor", GetName())); 

   dabc::Command::Reply(fConnCmd, false);
   fConnCmd = 0;

   // delete childs (parameters) before destructor is finished that one
   // can correcly use GetParsHolder() in Manager::ParameterEvent()
   DeleteChilds();

   DOUT3(("Did ApplicationPlugin %s destructor", GetName())); 
}

int dabc::ApplicationPlugin::ExecuteCommand(dabc::Command* cmd)
{
   int cmd_res = cmd_false;
    
   if (cmd->IsName("CreateAppModules")) {
      cmd_res = cmd_bool(CreateAppModules());
   } else
   if (cmd->IsName("ConnectAppModules")) {
      if (fConnCmd!=0) {
         EOUT(("There is active connect command !!!"));
      } else {
         int res = IsAppModulesConnected(); 
         if (res==1) cmd_res = cmd_true; else
         if (res==0) cmd_res = cmd_false; else {
            fConnCmd = cmd;
            fConnTmout = SMCommandTimeout();
            ActivateTimeout(0.2); 
            cmd_res = cmd_postponed;   
         }
      }
   } else
   if (cmd->IsName("BeforeAppModulesStarted")) {
      cmd_res = cmd_bool(BeforeAppModulesStarted());  
   } else
   if (cmd->IsName("AfterAppModulesStopped")) {
      cmd_res = cmd_bool(AfterAppModulesStopped());
   } else 
   if (cmd->IsName("BeforeAppModulesDestroyed")) {
      cmd_res = cmd_bool(BeforeAppModulesDestroyed());
   } else
      cmd_res = dabc::WorkingProcessor::ExecuteCommand(cmd); 
      
   return cmd_res;
}

double dabc::ApplicationPlugin::ProcessTimeout(double last_diff)
{
   // we using timeout events to check if connection is established
    
   fConnTmout -= last_diff;
    
   int res = IsAppModulesConnected(); 
    
   if ((res==2) && (fConnTmout<0)) res = 0;
    
   if ((res==1) || (res==0)) {
      dabc::Command::Reply(fConnCmd, (res==1));
      fConnCmd = 0;
      return -1;
   }
    
   return 0.2;
}


bool dabc::ApplicationPlugin::DoStateTransition(const char* state_trans_name) 
{ 
   // method called from SM thread, one should use Execute at this place
   
   bool res = true;
   
   if (strcmp(state_trans_name, dabc::Manager::stcmdDoConfigure)==0) {

      DOUT1(("Doing CreateAppModules %d", SMCommandTimeout()));

      res = Execute("CreateAppModules", SMCommandTimeout());
      
      DOUT1(("Did CreateAppModules"));
      
      res = GetManager()->CreateMemoryPools() && res;

      DOUT1(("Did CreateMemoryPools"));

   } else
   if (strcmp(state_trans_name, dabc::Manager::stcmdDoEnable)==0) {
      res = Execute("ConnectAppModules", SMCommandTimeout());
   } else 
   if (strcmp(state_trans_name, dabc::Manager::stcmdDoStart)==0) {
      res = Execute("BeforeAppModulesStarted", SMCommandTimeout());
      res = GetManager()->StartAllModules() && res;
   } else
   if (strcmp(state_trans_name, dabc::Manager::stcmdDoStop)==0) {
      res = GetManager()->StopAllModules();
      res = Execute("AfterAppModulesStopped", SMCommandTimeout()) && res;
   } else
   if (strcmp(state_trans_name, dabc::Manager::stcmdDoHalt)==0) {
      res = Execute("BeforeAppModulesDestroyed", SMCommandTimeout());
      res = GetManager()->CleanupManager() && res;
   } else
   if (strcmp(state_trans_name, dabc::Manager::stcmdDoError)==0) {
      res = GetManager()->StopAllModules(); 
   } else
      res = false;

   return res;
}

