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

#include "dabc/Publisher.h"

#include "dabc/Manager.h"
#include "dabc/HierarchyStore.h"


dabc::PublisherEntry::~PublisherEntry()
{
   if (store!=0) {
      store->CloseFile();
      delete store;
      store = 0;
   }
}

// ======================================================================

dabc::Publisher::Publisher(const std::string& name, dabc::Command cmd) :
   dabc::ModuleAsync(name, cmd),
   fGlobal(),
   fLocal(),
   fLastLocalVers(0),
   fPublishers(),
   fSubscribers(),
   fCnt(0),
   fMgrHiearchy()
{
   fLocal.Create("LOCAL");

   CreateTimer("Timer", 0.5);

   if (Cfg("manager",cmd).AsBool(false))
      fMgrHiearchy.Create("Manager");

   fStoreDir = Cfg("storedir", cmd).AsStr();
   fStoreSel = Cfg("storesel", cmd).AsStr();
   fFileLimit = Cfg("filelimit", cmd).AsInt(100);
   fTimeLimit = Cfg("timelimit", cmd).AsInt(600);
   fStorePeriod = Cfg("period",cmd).AsDouble(5.);

   if (!Cfg("store", cmd).AsBool()) fStoreDir.clear();

   DOUT0("Create publisher");
}

void dabc::Publisher::BeforeModuleStart()
{
   if (!fMgrHiearchy.null()) {
      std::string path = "DABC/";
      std::string addr = dabc::mgr.GetLocalAddress();
      size_t pos = addr.find(":");
      if (pos<addr.length()) addr[pos]='_';
      path += addr;

      DOUT0("dabc::Publisher::BeforeModuleStart mgr path %s", path.c_str());

      fPublishers.push_back(PublisherEntry());
      fPublishers.back().id = fCnt++;
      fPublishers.back().path = path;
      fPublishers.back().worker = ItemName();
      fPublishers.back().fulladdr = WorkerAddress(true);
      fPublishers.back().hier = fMgrHiearchy();
      fPublishers.back().local = true;

      fLocal.GetFolder(path, true);
   }
}

void dabc::Publisher::InvalidateGlobal()
{
   fLastLocalVers = 0;

   for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {
      if (!iter->local) iter->lastglvers = 0;
   }
}


void dabc::Publisher::ProcessTimerEvent(unsigned timer)
{
//   DOUT0("dabc::Publisher::ProcessTimerEvent");

   bool is_any_global(false);
   bool rebuild_global = fLocal.GetVersion() > fLastLocalVers;
/*
   static int mycnt = 0;
   if ((mycnt++ % 20 == 0) && !fStoreDir.empty()) {
      HierarchyReading rr;
      rr.SetBasePath(fStoreDir);
      DOUT0("-------------- DO SCAN -------------");
      rr.ScanTree();

      dabc::Hierarchy hh;
      rr.GetStrucutre(hh);

      DOUT0("GOT\n%s", hh.SaveToXml().c_str());

      dabc::DateTime from;
      from.SetOnlyDate("2013-09-18");
      from.SetOnlyDate("13:05:00");

      dabc::DateTime till = from;
      till.SetOnlyDate("14:05:00");

      dabc::Hierarchy sel = rr.GetSerie("/FESA/Test/TestRate", from, till);

      if (!sel.null())
         DOUT0("SELECT\n%s",sel.SaveToXml(dabc::xmlmask_History).c_str());
      else
         DOUT0("???????? SELECT FAILED ?????????");


      DOUT0("-------------- DID SCAN -------------");
   }
*/
   DateTime storetm; // stamp  used to mark time when next store operation was triggered

   for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {

      if (!iter->local) {
         is_any_global = true;
         if (iter->version > iter->lastglvers) rebuild_global = true;
      }

      if (iter->waiting_publisher) continue;

      iter->waiting_publisher = true;

      if (iter->hier == fMgrHiearchy())
      {
         // first, generate current objects hierarchy
         dabc::Hierarchy curr;
         curr.BuildNew(dabc::mgr);
         curr.Field(prop_producer).SetStr(WorkerAddress());

         // than use update to mark all changes
         fMgrHiearchy.Update(curr);

         // DOUT0("MANAGER %u\n %s", fMgrHiearchy.GetVersion(), fMgrHiearchy.SaveToXml().c_str());

         // generate diff to the last requested version
         Buffer diff = fMgrHiearchy.SaveToBuffer(dabc::stream_NamesList, iter->version);

         // and finally, apply diff to the main hierarchy
         ApplyEntryDiff(iter->id, diff, fMgrHiearchy.GetVersion());

      } else
      if (iter->local) {
         CmdPublisher cmd;
         bool dostore = false;
         cmd.SetReceiver(iter->worker);
         cmd.SetUInt("version", iter->version);
         cmd.SetPtr("hierarchy", iter->hier);
         cmd.SetPtr("mutex", iter->mutex);
         cmd.SetUInt("recid", iter->id);
         if (iter->store && iter->store->CheckForNextStore(storetm, fStorePeriod, fTimeLimit)) {
            cmd.SetPtr("store", iter->store);
            dostore = true;
         }
         cmd.SetTimeout(dostore ? 50. : 5.);
         dabc::mgr.Submit(Assign(cmd));
      } else {
         Command cmd("GetLocalHierarchy");
         cmd.SetReceiver(iter->fulladdr);
         cmd.SetUInt("version", iter->version);
         cmd.SetUInt("recid", iter->id);
         cmd.SetTimeout(10.);
         dabc::mgr.Submit(Assign(cmd));
      }

//      DOUT0("Submit command to worker %s id %u", iter->worker.c_str(), iter->id);
   }

   if (rebuild_global && is_any_global) {
      // recreate global structures again

      fGlobal.Release();
      fGlobal.Create("Global");

      //DOUT0("LOCAL version before:%u now:%u", (unsigned) fLastLocalVers, (unsigned) fLocal.GetVersion());

      fGlobal.Duplicate(fLocal);
      fLastLocalVers = fLocal.GetVersion();

      for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {

         if (iter->local || (iter->version==0)) continue;

         //DOUT0("REMOTE %s version before:%u now:%u", iter->fulladdr.c_str(), (unsigned) iter->lastglvers, (unsigned) iter->version);

         fGlobal.Duplicate(iter->rem);

         iter->lastglvers = iter->version;
      }

      //DOUT0("GLOBAL\n%s", fGlobal.SaveToXml().c_str());
   } else
   if (!is_any_global) {
      fGlobal.Release();
      fLastLocalVers = 0;
   }

   for (SubscribersList::iterator iter = fSubscribers.begin(); iter != fSubscribers.end(); iter++) {
      if (iter->waiting_worker) continue;

      if (iter->hlimit < 0) continue;

      // here direct request can be submitted, do it later, may be even not here
   }
}

void dabc::Publisher::CheckDnsSubscribers()
{
   for (SubscribersList::iterator iter = fSubscribers.begin(); iter != fSubscribers.end(); iter++) {
      if (iter->waiting_worker) continue;

      if (iter->hlimit >= 0) continue;

      dabc::Hierarchy h = iter->local ? fLocal.GetFolder(iter->path) : fGlobal.GetFolder(iter->path);

      if (h.null()) { EOUT("Subscribed path %s not found", iter->path.c_str()); continue; }
   }
}

bool dabc::Publisher::ApplyEntryDiff(unsigned recid, dabc::Buffer& diff, uint64_t version, bool witherror)
{
   PublishersList::iterator iter = fPublishers.begin();
   while (iter != fPublishers.end()) {
      if (iter->id == recid) break;
      iter++;
   }

   if (iter == fPublishers.end()) {
      EOUT("Get reply for non-existing id %u", recid);
      return false;
   }

   iter->waiting_publisher = false;

   if (witherror) {
      iter->errcnt++;
      EOUT("Command failed for rec %u addr %s errcnt %d", recid, iter->fulladdr.c_str(), iter->errcnt);
      return false;
   }

   iter->errcnt = 0;
   iter->version = version;

   if (iter->local) {
      dabc::Hierarchy top = fLocal.GetFolder(iter->path);
      if (!top.null()) {
         top.UpdateFromBuffer(diff);
         top.Field(prop_producer).SetStr(iter->fulladdr);
      }
   } else {
      iter->rem.UpdateFromBuffer(diff);
   }

   // DOUT0("LOCAL ver %u diff %u itemver %u \n%s",  fLocal.GetVersion(), diff.GetTotalSize(), iter->version, fLocal.SaveToXml(2).c_str());

   // check if hierarchy was changed
   CheckDnsSubscribers();

   return true;
}


bool dabc::Publisher::ReplyCommand(Command cmd)
{
   if (cmd.IsName(CmdPublisher::CmdName())) {
      dabc::Buffer diff = cmd.GetRawData();

      ApplyEntryDiff(cmd.GetUInt("recid"), diff, cmd.GetUInt("version"), cmd.GetResult() != cmd_true);

      return true;
   } else
   if (cmd.IsName("GetLocalHierarchy")) {
      dabc::Buffer diff = cmd.GetRawData();

      ApplyEntryDiff(cmd.GetUInt("recid"), diff, cmd.GetUInt("version"), cmd.GetResult() != cmd_true);

      return true;
   }

   return dabc::ModuleAsync::ReplyCommand(cmd);
}


dabc::Hierarchy dabc::Publisher::GetWorkItem(const std::string& path)
{

   dabc::Hierarchy top = fGlobal.null() ? fLocal : fGlobal;

   if (path.empty() || (path=="/")) return top;

   return top.FindChild(path.c_str());
}


int dabc::Publisher::ExecuteCommand(Command cmd)
{
   if (cmd.IsName("OwnCommand")) {

      std::string path = cmd.GetStr("Path");
      std::string worker = cmd.GetStr("Worker");

      switch (cmd.GetInt("cmdid")) {
         case 1: { // REGISTER

            for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {
               if (iter->path == path) {
                  EOUT("Path %s already registered!!!", path.c_str());
                  return cmd_false;
               }
            }

            dabc::Hierarchy h = fLocal.GetFolder(path);
            if (!h.null()) {
               EOUT("Path %s already present in the hierarchy", path.c_str());
               return cmd_false;
            }

            DOUT0("PUBLISH folder %s", path.c_str());

            fPublishers.push_back(PublisherEntry());
            fPublishers.back().id = fCnt++;
            fPublishers.back().path = path;
            fPublishers.back().worker = worker;
            fPublishers.back().fulladdr = dabc::mgr.ComposeAddress("", worker);
            fPublishers.back().hier = cmd.GetPtr("Hierarchy");
            fPublishers.back().mutex = cmd.GetPtr("Mutex");
            fPublishers.back().local = true;

            if (!fStoreDir.empty()) {
               if (fStoreSel.empty() || (path.find(fStoreSel) == 0)) {
                  DOUT0("Create store for %s", path.c_str());
                  fPublishers.back().store = new HierarchyStore();
                  fPublishers.back().store->SetBasePath(fStoreDir + path);
               }
            }

            fLocal.GetFolder(path, true);

            // ShootTimer("Timer");

            return cmd_true;
         }

         case 2:  { // UNREGISTER
            bool find = false;
            for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {
               if (iter->local && (iter->path == path) && (iter->worker == worker)) {

                  if (!fLocal.RemoveEmptyFolders(path))
                     EOUT("Not found local entry with path %s", path.c_str());

                  fPublishers.erase(iter);
                  find = true;
                  break;
               }
            }

            return cmd_bool(find);
         }

         case 3: {   // SUBSCRIBE

            bool islocal = true;

            dabc::Hierarchy h = fLocal.GetFolder(path);
            if (h.null()) { h = fGlobal.GetFolder(path); islocal = false; }
            if (h.null()) {
               EOUT("Path %s not exists", path.c_str());
               return cmd_false;
            }

            fSubscribers.push_back(SubscriberEntry());

            SubscriberEntry& entry = fSubscribers.back();

            entry.id = fCnt++;
            entry.path = path;
            entry.worker = worker;
            entry.local = islocal;
            entry.hlimit = 0;

            return cmd_true;
         }

         case 4: {   // UNSUBSCRIBE

            SubscribersList::iterator iter = fSubscribers.begin();
            while (iter != fSubscribers.end()) {
               if ((iter->worker == worker) && (iter->path == path))
                  fSubscribers.erase(iter++);
               else
                  iter++;
            }

            return cmd_true;
         }

         case 5: {  // REMOVE WORKER

            DOUT0("REMOVE WORKER %s", worker.c_str());

            PublishersList::iterator iter = fPublishers.begin();
            while (iter!=fPublishers.end()) {
               if (iter->worker == worker)
                  fPublishers.erase(iter++);
               else
                  iter++;
            }

            SubscribersList::iterator iter2 = fSubscribers.begin();
            while (iter2 != fSubscribers.end()) {
               if (iter2->worker == worker)
                  fSubscribers.erase(iter2++);
               else
                  iter2++;
            }

            return cmd_true;
         }

         case 6: {  // ADD REMOTE NODE

            std::string remoteaddr = dabc::mgr.ComposeAddress(path, dabc::Publisher::DfltName());

            for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {
               if ((iter->fulladdr == remoteaddr) && !iter->local) {
                  EOUT("Path %s already registered!!!", path.c_str());
                  return cmd_false;
               }
            }

            fPublishers.push_back(PublisherEntry());
            fPublishers.back().id = fCnt++;
            fPublishers.back().path = "";
            fPublishers.back().worker = worker;
            fPublishers.back().fulladdr = remoteaddr;
            fPublishers.back().hier = 0;
            fPublishers.back().local = false;
            fPublishers.back().rem.Create("remote");

            DOUT0("PUBLISH NODE %s", path.c_str());

            return cmd_true;
         }

         default:
           EOUT("BAD OwnCommand ID");
           return cmd_false;
      }
   } else
   if (cmd.IsName("GetLocalHierarchy")) {

      Buffer diff = fLocal.SaveToBuffer(dabc::stream_NamesList, cmd.GetUInt("version"));

      cmd.SetRawData(diff);
      cmd.SetUInt("version", fLocal.GetVersion());

      return cmd_true;
   } else
   if (cmd.IsName("GetGlobalNamesListAsXml")) {

      std::string path = cmd.GetStr("path");

      dabc::Hierarchy h = GetWorkItem(path);
      if (h.null()) return cmd_false;

      if (!path.empty() && (path[path.length()-1]!='/')) path.append("/");

      std::string res =
            dabc::format("<dabc version=\"2\" xmlns:dabc=\"http://dabc.gsi.de/xhtml\" path=\"%s\">\n", path.c_str())+
            h.SaveToXml() +
            std::string("</dabc>\n");

      cmd.SetStr("xml",res);

      return cmd_true;
   } else
   if (cmd.IsName(CmdPublisherGet::CmdName()) ||
       cmd.IsName(CmdGetBinary::CmdName())) {

      // if we get command here, we need to find destination for it

      std::string itemname = cmd.GetStr("Item");
      std::string producer_name, request_name;
      bool islocal(true);

      DOUT0("PUBLISHER CMD %s ITEM %s", cmd.GetName(), itemname.c_str());


      // first look in local structures
      dabc::Hierarchy h = fLocal.GetFolder(itemname);

      if (!h.null()) {
         // we need to redirect command to appropriate worker (or to ourself)

         producer_name = h.FindBinaryProducer(request_name);
         DOUT0("Producer = %s request %s", producer_name.c_str(), request_name.c_str());

      } else
      for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {
         if (iter->local) continue;

         dabc::Hierarchy h = iter->rem.GetFolder(itemname);
         if (h.null()) continue;

         // we need to redirect command to remote node

         producer_name = h.FindBinaryProducer(request_name);
         islocal = false;
         break;
      }

      if (producer_name.empty()) {
         EOUT("Not found producer for item %s", itemname.c_str());
         return cmd_false;
      }

      bool producer_local(true);
      std::string producer_server, producer_item;

      if (!dabc::mgr.DecomposeAddress(producer_name, producer_local, producer_server, producer_item)) {
         EOUT("Wrong address specified as producer %s", producer_name.c_str());
         return cmd_false;
      }

      if (islocal || producer_local) {
         // this is local case, we need to redirect command to the appropriate worker
         // but first we should locate hierarchy which is assigned with the worker

         for (PublishersList::iterator iter = fPublishers.begin(); iter != fPublishers.end(); iter++) {
            if (!iter->local) continue;

            if ((iter->worker != producer_item) && (iter->worker != std::string("/") + producer_item)) continue;

            // we redirect command to local worker
            // manager should find proper worker for execution

            DOUT0("Submit GET command to %s subitem %s", producer_item.c_str(), request_name.c_str());
            cmd.SetReceiver(iter->worker);
            cmd.SetPtr("hierarchy", iter->hier);
            cmd.SetPtr("mutex", iter->mutex);
            cmd.SetStr("subitem", request_name);
            dabc::mgr.Submit(cmd);
            return cmd_postponed;
         }

         EOUT("Not found producer %s, which is correspond to item %s", producer_item.c_str(), itemname.c_str());
         return cmd_false;
      }

      if (cmd.GetBool("analyzed")) {
         EOUT("Command to get item %s already was analyzed - something went wrong", itemname.c_str());
         return cmd_false;
      }

      cmd.SetReceiver(dabc::mgr.ComposeAddress(producer_server, dabc::Publisher::DfltName()));
      cmd.SetBool("analyzed", true);
      dabc::mgr.Submit(cmd);
      return cmd_postponed;
   }

   return dabc::ModuleAsync::ExecuteCommand(cmd);
}

// ===================================================================

bool dabc::PublisherRef::SaveGlobalNamesListAsXml(const std::string& path, std::string& str)
{
   if (null()) return false;

   dabc::Command cmd("GetGlobalNamesListAsXml");
   cmd.SetStr("path", path);

   if (Execute(cmd) != cmd_true) return false;

   str = cmd.GetStr("xml");

   return true;
}

dabc::Hierarchy dabc::PublisherRef::Get(const std::string& fullname, uint64_t version, unsigned hlimit, double tmout)
{
   dabc::Hierarchy res;

   if (null()) return res;

   CmdPublisherGet cmd;
   cmd.SetStr("Item", fullname);
   cmd.SetUInt("version", version > 0 ? version+1 : 0);
   cmd.SetUInt("history", hlimit);
   cmd.SetTimeout(tmout);

   if (Execute(cmd) != cmd_true) return res;

   dabc::Buffer buf = cmd.GetRawData();

   res.Create("get");
   res.SetVersion(cmd.GetUInt("version"));

   res.ReadFromBuffer(buf);

   return res;
}

dabc::Buffer dabc::PublisherRef::GetBinary(const std::string& fullname, uint64_t version, double tmout)
{
   dabc::Buffer res;

   if (null()) return res;

   CmdGetBinary cmd;
   cmd.SetStr("Item", fullname);
   cmd.SetUInt("version", version);
   cmd.SetTimeout(tmout);

   if (Execute(cmd) == cmd_true)
      res = cmd.GetRawData();

   return res;
}


dabc::Hierarchy dabc::PublisherRef::Get(const std::string& fullname, double tmout)
{
   return Get(fullname, 0, 0, tmout);
}
