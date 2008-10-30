#include "dabc/VerbsDevice.h"

#include <sys/poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>


#ifndef  __NO_MULTICAST__ 
#include "dabc/VerbsOsm.h"
#else
#include <infiniband/arch.h>
#endif

#include "dabc/timing.h"
#include "dabc/logging.h"

#include "dabc/MemoryPool.h"
#include "dabc/Manager.h"
#include "dabc/Port.h"
#include "dabc/Factory.h"

#include "dabc/VerbsCQ.h"
#include "dabc/VerbsQP.h"

#include "dabc/VerbsThread.h"
#include "dabc/VerbsTransport.h"

const int LoopBackQueueSize = 8;
const int LoopBackBufferSize = 64;

int null_gid(union ibv_gid *gid)
{
   return !(gid->raw[8] | gid->raw[9] | gid->raw[10] | gid->raw[11] |
       gid->raw[12] | gid->raw[13] | gid->raw[14] | gid->raw[15]);
}

// this boolean indicates if one can use verbs calls from different threads
// if no, all post/recv/complition operation for all QP/CQ will happens in the same thread

bool dabc::VerbsDevice::fThreadSafeVerbs = true;

namespace dabc {
   class VerbsFactory : public Factory {
      public:
         VerbsFactory(const char* name) : Factory(name) {}
         
         virtual Device* CreateDevice(Basic* parent,
                                      const char* classname, 
                                      const char* devname, 
                                      Command*);
         
         virtual WorkingThread* CreateThread(Basic* parent, const char* classname, const char* thrdname, const char* thrddev, Command* cmd);
   };
   

   VerbsFactory verbsfactory("verbs");
}

dabc::Device* dabc::VerbsFactory::CreateDevice(Basic* parent,
                                               const char* classname, 
                                               const char* devname, 
                                               Command*) 
{
   if (strcmp(classname, "VerbsDevice")!=0) return 0;

   return new VerbsDevice(parent, devname);
}                            

dabc::WorkingThread* dabc::VerbsFactory::CreateThread(Basic* parent, const char* classname, const char* thrdname, const char* thrddev, Command* cmd)
{
   if ((classname==0) || (strcmp(classname,"VerbsThread")!=0)) return 0;
    
   if (thrddev==0) { 
      EOUT(("Device name not specified to create verbs thread")); 
      return 0;
   }
   
   VerbsDevice* dev = dynamic_cast<VerbsDevice*> (Manager::Instance()->FindDevice(thrddev));
   if (dev==0) {
      EOUT(("Did not found verbs device with name %s", thrddev));
      return 0;
   }
   
   return new VerbsThread(dev, parent, thrdname);
}

// *******************************************************************

dabc::VerbsPoolRegistry::VerbsPoolRegistry(VerbsDevice* verbs, MemoryPool* pool, bool local) : 
   Basic(local ? 0 : verbs->GetPoolRegFolder(true), pool->GetName()),
   fVerbs(verbs),
   fPool(pool),
   fWorkMutex(0),
   fUsage(0),
   fLastChangeCounter(0),
   f_nummr(0),
   f_mr(0),
   fBlockChanged(0)
{
   if (GetManager() && fPool) 
     GetManager()->RegisterDependency(this, fPool); 
    
   LockGuard lock(fPool->GetPoolMutex());
   
   // we only need to lock again pool mutex when pool can be potentially changed
   // otherwise structure, created once, can be used forever
   if (!fPool->IsMemLayoutFixed()) fWorkMutex = fPool->GetPoolMutex();

   _UpdateMRStructure();
}   
          
dabc::VerbsPoolRegistry::~VerbsPoolRegistry()
{
   DOUT3(("~VerbsPoolRegistry %s", GetName())); 
    
   if (GetManager() && fPool) 
      GetManager()->UnregisterDependency(this, fPool);
      
   CleanMRStructure();

   DOUT3(("~VerbsPoolRegistry %s done", GetName())); 

}

void dabc::VerbsPoolRegistry::DependendDestroyed(Basic* obj) 
{
   if (obj == fPool) {
//      EOUT(("!!!!!!!!! Hard error - memory pool %s destroyed behind the scene", fPool->GetName()));
      CleanMRStructure();
      fPool = 0;
      fWorkMutex = 0;
      DeleteThis();
   }
}

void dabc::VerbsPoolRegistry::CleanMRStructure()
{
   DOUT3(("CleanMRStructure %s call ibv_dereg_mr %u", GetName(), f_nummr)); 

   for (unsigned n=0;n<f_nummr;n++) 
     if (f_mr[n] != 0) {
        DOUT3(("CleanMRStructure %s mr[%u] = %p", GetName(), n, f_mr[n]));
//        if (strcmp(GetName(),"TransportPool")!=0)
           ibv_dereg_mr(f_mr[n]);
//        else
//           EOUT(("Skip ibv_dereg_mr(f_mr[n])"));   
        DOUT3(("CleanMRStructure %s mr[%u] = %p done", GetName(), n, f_mr[n]));
     }
   
   delete[] fBlockChanged; fBlockChanged = 0;
   
   delete[] f_mr; 
   f_mr = 0;
   f_nummr = 0;
   
   fLastChangeCounter = 0;
}


void dabc::VerbsPoolRegistry::_UpdateMRStructure()
{
   if (fPool==0) return; 
   
   if (f_nummr < fPool->NumMemBlocks()) {
      // this is a case, when pool size was extended   
      
      struct ibv_mr* *new_mr = new struct ibv_mr* [fPool->NumMemBlocks()];
      
      unsigned *new_changed = new unsigned [fPool->NumMemBlocks()];
      
      for (unsigned n=0;n<fPool->NumMemBlocks();n++) {
         new_mr[n] = 0;
         new_changed[n] = 0;
         if (n<f_nummr) {
             new_mr[n] = f_mr[n];
             new_changed[n] = fBlockChanged[n];
         }
      }
      
      delete[] f_mr;
      delete[] fBlockChanged;
      
      f_mr = new_mr;
      fBlockChanged = new_changed;
      
   } else
   if (f_nummr > fPool->NumMemBlocks()) {
      //  this is a case, when pool size was reduced 
      // first, cleanup no longer avaliable memory regions
      for (unsigned n=fPool->NumMemBlocks(); n<f_nummr; n++) {
         if (f_mr[n]) {
            ibv_dereg_mr(f_mr[n]);
            f_mr[n] = 0;
         }
         fBlockChanged[n] = 0;
      }
   }
    
   f_nummr = fPool->NumMemBlocks();
   
   for (unsigned n=0;n<f_nummr;n++) 
      if ( fPool->IsMemoryBlock(n) && 
          ((f_mr[n]==0) || (fBlockChanged[n] != fPool->MemBlockChangeCounter(n))) ) {
              if (f_mr[n]!=0) {
                 ibv_dereg_mr(f_mr[n]);
              }
              
              f_mr[n] = ibv_reg_mr(fVerbs->pd(),
                                   fPool->GetMemBlock(n),
                                   fPool->GetMemBlockSize(n),
                                   IBV_ACCESS_LOCAL_WRITE);
                                   
//                                   (ibv_access_flags) (IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE));

              DOUT3(("New VerbsPoolRegistry %s mr[%u] = %p done", GetName(), n, f_mr[n]));

              if (f_mr[n]==0) {
                 EOUT(("Fail to registry VERBS memory - HALT"));
                 exit(1);   
              }
              fBlockChanged[n] = fPool->MemBlockChangeCounter(n);
          } else 
          
          if (!fPool->IsMemoryBlock(n)) {
             if (f_mr[n]!=0) ibv_dereg_mr(f_mr[n]); 
             f_mr[n] = 0;
             fBlockChanged[n] = 0;
          }
   
   fLastChangeCounter = fPool->GetChangeCounter();
}

// ____________________________________________________________________

dabc::VerbsDevice::VerbsDevice(Basic* parent, const char* name) : 
   Device(parent, name),
   fIbPort(0),
   fContext(0),
   fPD(0),
   f_mtu(2048),
   fOsm(0),
   fAllocateIndividualCQ(false)
{
   DOUT3(("Creating VERBS device %s", name)); 
    
   if (!OpenVerbs(true)) {
      EOUT(("FATAL. Cannot start VERBS device"));
      exit(1);
   } 
    
   DOUT3(("Creating VERBS thread %s", GetName())); 
   
   VerbsThread* thrd = GetVerbsThread(GetName(), true);

   DOUT3(("Assign VERBS device to thread %s", GetName())); 
   
   AssignProcessorToThread(thrd);
   
   DOUT3(("Creating VERBS device %s done", name)); 

}

dabc::VerbsDevice::~VerbsDevice()
{
   DOUT5(("Start VerbsDevice::~VerbsDevice()")); 

   // one should call method here, while thread(s) must be deleted
   // before CQ, Channel, PD will be destroyed
   CleanupDevice(true);

   DOUT5(("Cleanup verbs device done"));
   
   RemoveProcessorFromThread(true);
   
   Folder* f = GetManager()->GetThreadsFolder();
   if (f) {
      for (int n=f->NumChilds()-1; n>=0; n--) {
         VerbsThread* thrd = dynamic_cast<VerbsThread*> (f->GetChild(n));
         if (thrd && (thrd->GetDevice()==this))
            if (!thrd->IsItself()) {
               if (thrd==ProcessorThread()) EOUT(("AAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBB")); 
               delete thrd;
            } else {
               thrd->CloseThread();
               GetManager()->DestroyObject(thrd);
            }
      }
   }

   DOUT5(("DestroyRegisteredThreads done"));

   CloseVerbs();

   DOUT5(("Stop VerbsDevice::~VerbsDevice()"));
}

bool dabc::VerbsDevice::OpenVerbs(bool withmulticast, const char* devicename, int ibport)
{
   DOUT4(("Call  VerbsDevice::Open"));
    
#ifndef  __NO_MULTICAST__ 
   if (withmulticast) {
      fOsm = new VerbsOsm;
      if (!fOsm->Init()) return false;
   }
#endif   

   int num_of_hcas = 0;

   struct ibv_device **dev_list = ibv_get_device_list(&num_of_hcas);

   if ((dev_list==0) || (num_of_hcas<=0)) {
      EOUT(("No verbs devices found"));
      return false;
   }

   DOUT4(( "Number of hcas %d", num_of_hcas));

   struct ibv_device *selected_device = 0;
   uint64_t gid;
   bool res = false;

   if (devicename==0) {
      selected_device = dev_list[0];
      fIbPort = 1;
      devicename = ibv_get_device_name(selected_device);
   } else {
      fIbPort = ibport;
      if (fIbPort<=0) fIbPort = 1;
      for (int n=0;n<num_of_hcas;n++)
         if (strcmp(ibv_get_device_name(dev_list[n]), devicename)==0)
            selected_device = dev_list[n];
      if (selected_device==0) {
         EOUT(("No verbs device with name %s", devicename));
      }
   }

   if (selected_device==0) goto cleanup;

   fContext = ibv_open_device(selected_device);
   if (fContext==0) {
      EOUT(("Cannot open device %s", devicename));
      goto cleanup;
   }
   
   if (ibv_query_device(fContext, &fDeviceAttr)) {
      EOUT(("Failed to query device props"));
      goto cleanup;
   }

   gid = ibv_get_device_guid(selected_device);
   DOUT4(( "Open device: %s  gid: %016lx",  devicename, ntohll(gid)));

   fPD = ibv_alloc_pd(fContext);
   if (fPD==0) {
      EOUT(("Couldn't allocate protection domain (PD)"));
      goto cleanup;
   }

   if (ibv_query_port(fContext, fIbPort, &fPortAttr)) {
      EOUT(("Fail to query port attributes"));
      goto cleanup;
   }
   
//   PrintDevicesList(true);

   DOUT4(("Call new TVerbsConnMgr(this) done"));

   res = true;
   
cleanup:

   ibv_free_device_list(dev_list);

   return res;

}

bool dabc::VerbsDevice::CloseVerbs()
{
   if (fContext==0) return true;

   bool res = true;
   
   Folder* fold = GetPoolRegFolder(false);
   if (fold!=0) fold->DeleteChilds();

   if (ibv_dealloc_pd(fPD)) {
      EOUT(("Fail to deallocate PD"));
      res = false;
   }
   if (ibv_close_device(fContext)) {
      EOUT(("Fail to close device context"));
      res = false;
   }

   fContext = 0;

#ifndef  __NO_MULTICAST__ 
   if (fOsm!=0) {
      fOsm->Close();
      delete fOsm;
      fOsm = 0;
   }
#endif

   return res;
}

int dabc::VerbsDevice::GetGidIndex(ibv_gid* lookgid)
{
   ibv_gid gid;
   int ret = 0;

   DOUT5(( "Serach for gid in table: %016lx : %016lx  ",
             ntohll(lookgid->global.subnet_prefix),
             ntohll(lookgid->global.interface_id)));

   for (int i = 0; !ret; i++) {
      ret = ibv_query_gid(fContext, fIbPort, i, &gid);

        if (ret) break;

        if (null_gid(&gid)) continue;

        DOUT5(( "   gid[%2d]: %016lx : %016lx  ", i,
             ntohll(gid.global.subnet_prefix),
             ntohll(gid.global.interface_id)));

        if (!ret && !memcmp(lookgid, &gid, sizeof(ibv_gid))) return i;
   }
   return 0;
}

struct ibv_ah* dabc::VerbsDevice::CreateAH(uint32_t dest_lid, int dest_port)
{
   ibv_ah_attr ah_attr;
   memset(&ah_attr, 0, sizeof(ah_attr));
   ah_attr.is_global  = 0;
   ah_attr.dlid       = dest_lid;
   ah_attr.sl         = 0;
   ah_attr.src_path_bits = 0;
   ah_attr.port_num   = dest_port>=0 ? dest_port : IbPort(); // !!!!!!!  probably, here should be destination port

   ibv_ah *ah = ibv_create_ah(pd(), &ah_attr);
   if (ah==0) {
      EOUT(("Failed to create Address Handle"));
   }
   return ah;
}

bool dabc::VerbsDevice::RegisterMultiCastGroup(ibv_gid* mgid, uint16_t& mlid)
{
   mlid = 0;
#ifndef  __NO_MULTICAST__ 
   if ((mgid==0) || (fOsm==0)) return false;
   return fOsm->ManageMultiCastGroup(true, mgid->raw, &mlid);
#else
   return false;
#endif   
}

bool dabc::VerbsDevice::UnRegisterMultiCastGroup(ibv_gid* mgid, uint16_t mlid)
{
#ifndef  __NO_MULTICAST__ 
   if ((mgid==0) || (fOsm==0)) return false;
   return fOsm->ManageMultiCastGroup(false, mgid->raw, &mlid);
#else
   return false;
#endif
}

struct ibv_ah* dabc::VerbsDevice::CreateMAH(ibv_gid* mgid, uint32_t mlid, int mport)
{
   if (mgid==0) return 0; 
    
   ibv_ah_attr mah_attr;
   memset(&mah_attr, 0, sizeof(ibv_ah_attr));

   mah_attr.dlid = mlid; // in host order ?
   mah_attr.port_num = mport>=0 ? mport : IbPort();
   mah_attr.sl = 0;
   mah_attr.static_rate = 0; //0x83; // shold be copied from mmember rec

   mah_attr.is_global  = 1;
   memcpy(&(mah_attr.grh.dgid), mgid, sizeof(ibv_gid));
   mah_attr.grh.sgid_index = 0; // GetGidIndex(mgid);

   mah_attr.grh.flow_label = 0; // shold be copied from mmember rec
   mah_attr.grh.hop_limit = 63; // shold be copied from mmember rec
   mah_attr.grh.traffic_class = 0; // shold be copied from mmember rec

   //DOUT1(("Addr %02x %02x", ah_attr.grh.dgid.raw[0], ah_attr.grh.dgid.raw[1]));

   struct ibv_ah* f_ah = ibv_create_ah(pd(), &mah_attr);
   if (f_ah==0) {
     EOUT(("Failed to create Multicast Address Handle"));
   }
   
   return f_ah;
}

void dabc::VerbsDevice::CreatePortQP(const char* thrd_name, Port* port, int conn_type,
                                     VerbsCQ* &port_cq, VerbsQP* &port_qp)
{
   ibv_qp_type qp_type = IBV_QPT_RC;
    
   if (conn_type>0) qp_type = (ibv_qp_type) conn_type;
   
   VerbsThread* thrd = GetVerbsThread(thrd_name, true);
   
   bool isowncq = IsAllocateIndividualCQ() && !thrd->IsFastModus();
   
   if (isowncq)
      port_cq = new VerbsCQ(fContext, port->NumOutputBuffersRequired() + port->NumInputBuffersRequired() + 2, thrd->Channel());
   else
      port_cq = thrd->MakeCQ();
   
   port_qp = new VerbsQP(this, qp_type, 
                         port_cq, port->NumOutputBuffersRequired(), fDeviceAttr.max_sge - 1, 
                         port_cq, port->NumInputBuffersRequired(), /*fDeviceAttr.max_sge / 2*/ 2);
    if (!isowncq)
       port_cq = 0; 
}

dabc::Folder* dabc::VerbsDevice::GetPoolRegFolder(bool force)
{
   return GetFolder("PoolReg", force, true);
}

dabc::VerbsPoolRegistry* dabc::VerbsDevice::FindPoolRegistry(MemoryPool* pool)
{
   if (pool==0) return 0; 
    
   Folder* fold = GetPoolRegFolder(false);
   if (fold==0) return 0;
   
   for (unsigned n=0; n<fold->NumChilds(); n++) {
       VerbsPoolRegistry* reg = (VerbsPoolRegistry*) fold->GetChild(n);
       if ((reg!=0) && (reg->GetPool()==pool)) return reg;
   }
   
   return 0;
}

dabc::VerbsPoolRegistry* dabc::VerbsDevice::RegisterPool(dabc::MemoryPool* pool)
{
   if (pool==0) return 0;
   
   VerbsPoolRegistry* entry = FindPoolRegistry(pool);
   
   if (entry==0) entry = new VerbsPoolRegistry(this, pool);
   
   entry->IncUsage();
   
   return entry;
}

void dabc::VerbsDevice::UnregisterPool(VerbsPoolRegistry* entry)
{
   if (entry==0) return; 

   DOUT3(("Call UnregisterPool %s", entry->GetName())); 
    
   entry->DecUsage();

   if (entry->GetUsage()<=0) {
      DOUT3(("Pool %s no longer used in verbs device: %s entry %p", entry->GetName(), GetName(), entry));
      //delete entry;
      // entry->GetParent()->RemoveChild(entry);
      // delete entry;
      
      //GetManager()->DestroyObject(entry);
      
      //entry->CleanMRStructure();
      
      // DOUT1(("Clean entry %p done", entry));
   }

   DOUT3(("Call UnregisterPool done")); 
}

void dabc::VerbsDevice::CreateVerbsTransport(const char* thrdname, const char* portname, VerbsCQ* cq, VerbsQP* qp)
{
   if (qp==0) return;

   Port* port = GetManager()->FindPort(portname); 
    
   VerbsThread* thrd = GetVerbsThread(thrdname, false);

   if ((thrd==0) || (port==0)) {
      EOUT(("VerbsThread %s:%p or Port %s:%p is dissapiar!!!", thrdname, thrd, portname, port));
      delete qp;
      delete cq;
      return;
   }

   VerbsTransport* tr = new VerbsTransport(this, cq, qp, port);
   
   tr->AssignProcessorToThread(thrd);
   
   port->AssignTransport(tr);
}

bool dabc::VerbsDevice::ServerConnect(Command* cmd, Port* port, const char* portname)
{
   if (cmd==0) return false; 
   
   return ((VerbsThread*) ProcessorThread())->DoServer(cmd, port, portname);
}

bool dabc::VerbsDevice::ClientConnect(Command* cmd, Port* port, const char* portname)
{
   if (cmd==0) return false; 

   return ((VerbsThread*) ProcessorThread())->DoClient(cmd, port, portname);
}

bool dabc::VerbsDevice::SubmitRemoteCommand(const char* servid, const char* channelid, Command* cmd)
{
   return false; 
}

dabc::VerbsThread* dabc::VerbsDevice::GetVerbsThread(const char* name, bool force)
{
   VerbsThread* thrd = dynamic_cast<VerbsThread*> (Manager::Instance()->FindThread(name, "VerbsThread"));

   if (thrd || !force) return thrd;
    
   return dynamic_cast<VerbsThread*> (Manager::Instance()->CreateThread(name, "VerbsThread", 0, GetName()));
}

int dabc::VerbsDevice::CreateTransport(Command* cmd, Port* port)
{
   bool isserver = cmd->GetBool("IsServer", true);
   
   const char* portname = cmd->GetPar("PortName");
      
   if (isserver ? ServerConnect(cmd, port, portname) : ClientConnect(cmd, port, portname))
      return cmd_postponed;
      
   return cmd_false;
}

int dabc::VerbsDevice::ExecuteCommand(dabc::Command* cmd)
{
   int cmd_res = cmd_true;
   
   DOUT5(("Execute command %s", cmd->GetName()));
   
   if (cmd->IsName("StartServer")) {
      String servid;
      ((VerbsThread*) ProcessorThread())->FillServerId(servid);
      cmd->SetPar("ConnId", servid.c_str());
   } else 
      cmd_res = dabc::Device::ExecuteCommand(cmd);
      
   return cmd_res;
}
