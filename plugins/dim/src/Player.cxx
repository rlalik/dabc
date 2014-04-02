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

#include "dimc/Player.h"

#include <sys/timeb.h>

#include "dabc/Publisher.h"
#include "dabc/Url.h"

#include "mbs/SlowControlData.h"


dimc::Player::Player(const std::string& name, dabc::Command cmd) :
   dabc::ModuleAsync(name, cmd),
   DimInfoHandler(),
   fDimDns(),
   fDimMask(),
   fDimPeriod(1.),
   fDimBr(0),
   fDimInfos(),
   fLastScan(),
   fNeedDnsUpdate(true),
   fSubeventId(8),
   fEventNumber(0),
   fLastSendTime(),
   fIter(),
   fFlushTime(10)
{
   //EnsurePorts(0, 0, dabc::xmlWorkPool);

   strncpy(fNoLink, "no_link", sizeof(fNoLink));

   fDimDns = Cfg("DimDns", cmd).AsStr();
   fDimMask = Cfg("DimMask", cmd).AsStr("*");
   fDimPeriod = Cfg("DimPeriod", cmd).AsDouble(1);
   if (fDimMask.empty()) fDimMask = "*";

   fSubeventId = Cfg("DimSubeventId", cmd).AsUInt(fSubeventId);

   fWorkerHierarchy.Create("DIMC", true);
   // fWorkerHierarchy.EnableHistory(100, true); // TODO: make it configurable

   CreateTimer("update", (fDimPeriod>0.01) ? fDimPeriod : 0.01, false);

   fFlushTime = Cfg(dabc::xmlFlushTimeout,cmd).AsDouble(10.);

   Publish(fWorkerHierarchy, "DIMC");
}

dimc::Player::~Player()
{
   for (DimServicesMap::iterator iter = fDimInfos.begin(); iter!=fDimInfos.end();iter++) {
      delete iter->second.info;
      iter->second.info = 0;
   }
   fDimInfos.clear();

   delete fDimBr; fDimBr = 0;
}


void dimc::Player::OnThreadAssigned()
{
   dabc::ModuleAsync::OnThreadAssigned();

   if (!fDimDns.empty()) {
      dabc::Url url(fDimDns);
      if (url.IsValid()) {
        if (url.GetPort()>0)
           ::DimClient::setDnsNode(url.GetHostName().c_str(), url.GetPort());
        else
           ::DimClient::setDnsNode(url.GetHostName().c_str());
      }
   }

   fDimBr = new ::DimBrowser();
}

void dimc::Player::ScanDimServices()
{
   char *service_name, *service_descr;
   int type;

// DimClient::addErrorHandler(errHandler);

   {
      dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());

      for (DimServicesMap::iterator iter = fDimInfos.begin(); iter!=fDimInfos.end();iter++) {
         iter->second.flag = 0;
      }
   }

   int nservices = fDimBr->getServices(fDimMask.c_str());
   DOUT0("found %d DIM services", nservices);

   while((type = fDimBr->getNextService(service_name, service_descr))!= 0)
   {
      nservices--;

      dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());

      DOUT0("type %d name %s descr %s", type, service_name, service_descr);

      if ((service_descr==0) || ((type!=1) && (type!=2))) continue;

      DimServicesMap::iterator iter = fDimInfos.find(service_name);
      if (iter!=fDimInfos.end()) {
         iter->second.flag = type;  // mark entry as found
         continue;
      }

      MyEntry entry;
      entry.flag = type;

      // we processing call-back only for normal records
      if (type==1)
         entry.info = new DimInfo(service_name, (void*) fNoLink, (int) sizeof(fNoLink), (DimInfoHandler*) this);

      fDimInfos[service_name] = entry;

      DOUT3("Create entry %p type %d name %s descr %s", entry.info, type, service_name, service_descr);

      dabc::Hierarchy item = fWorkerHierarchy.CreateHChild(service_name);

      if (type==1) {
         item.EnableHistory(100);
      } else
      if (type==2) {
         item.SetField(dabc::prop_kind, "DABC.Command");
         item.SetField("dimcmd", service_name);
         item.SetField("numargs", 1);
         item.SetField("arg0", "PAR");
         item.SetField("arg0_dflt", "");
         item.SetField("arg0_kind", "string");
      }
   }

   {
      dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());

      DimServicesMap::iterator iter = fDimInfos.begin();
      while (iter!=fDimInfos.end()) {
         if (iter->second.flag != 0) { iter++; continue; }

         delete iter->second.info;
         iter->second.info = 0;

         DOUT3("Destroy entry %s", iter->first.c_str());

         fWorkerHierarchy.RemoveHChild(iter->first);

         fDimInfos.erase(iter++);
      }
   }

   fLastScan.GetNow();
   fNeedDnsUpdate = false;
}

void dimc::Player::ProcessTimerEvent(unsigned timer)
{
   if (fDimBr==0) return;

   if (fLastScan.Expired(10.) || (fDimInfos.size()==0) || fNeedDnsUpdate)
      ScanDimServices();

//   dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());
//   fWorkerHierarchy.MarkChangedItems();

}

int dimc::Player::ExecuteCommand(dabc::Command cmd)
{
   if (cmd.IsName(dabc::CmdHierarchyExec::CmdName())) {
      std::string path = cmd.GetStr("Item");
      std::string arg = cmd.GetStr("PAR");
      std::string dimcmd;

      {
         dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());
         dabc::Hierarchy item = fWorkerHierarchy.FindChild(path.c_str());
         if (item.null()) {
            EOUT("Did not found command item %s in DIM hierarchy", path.c_str());
            return dabc::cmd_false;
         }
         dimcmd = item.GetField("dimcmd").AsStr();
      }

      if (dimcmd.empty()) return dabc::cmd_false;

      DOUT0("Execute DIM command %s arg %s", dimcmd.c_str(), arg.c_str());

      DimClient::sendCommand(dimcmd.c_str(), arg.c_str());

      return dabc::cmd_true;
   }

   return dabc::ModuleAsync::ExecuteCommand(cmd);
}

void dimc::Player::infoHandler()
{
   // DIM method, called when service is updated

   DimInfo *info = getInfo();
   if (info==0) return;

   if (info->getData() == fNoLink) {
      DOUT0("Get nolink for %s", info->getName());
      return;
   }

   if (info->getFormat() == 0) {
      EOUT("Get null format for %s", info->getName());
      return;
   }

   dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());
   dabc::Hierarchy item = fWorkerHierarchy.GetHChild(info->getName());

   if (item.null()) {
      EOUT("Fail to find item for %s", info->getName());
      return;
   }

   DimServicesMap::iterator iter = fDimInfos.find(info->getName());
   if ((iter==fDimInfos.end()) || (iter->second.info!=info)) {
      EOUT("Did not found service %s in infos map", info->getName());
      return;
   }

   if (strcmp(info->getName(),"DIS_DNS/SERVER_LIST")==0) {
      DOUT0("Get DIS_DNS/SERVER_LIST");
      fNeedDnsUpdate = true;
   }
   // DOUT0("Get change for info %p name :%s: format %s", info, info->getName(), info->getFormat());

   bool changed = true;

   iter->second.fKind = 0;
   if (strcmp(info->getFormat(),"I")==0) {
      item.SetField("value", info->getInt());
      item.SetField(dabc::prop_kind,"rate");
      iter->second.fKind = 1;
      iter->second.fLong = info->getInt();
   } else
   if (strcmp(info->getFormat(),"F")==0) {
      item.SetField("value", info->getFloat());
      item.SetField(dabc::prop_kind,"rate");
      iter->second.fKind = 2;
      iter->second.fDouble = info->getFloat();
   } else
   if (strcmp(info->getFormat(),"D")==0) {
      item.SetField("value", info->getDouble());
      item.SetField(dabc::prop_kind,"rate");
      iter->second.fKind = 2;
      iter->second.fDouble = info->getDouble();
   } else
   if (strcmp(info->getFormat(),"C")==0) {
      item.SetField("value", info->getString());
      item.SetField(dabc::prop_kind,"log");
   } else
   if ((strcmp(info->getFormat(),"F:1;I:1;F:1;F:1;F:1;F:1;C:16;C:16;C:16")==0) && (strncmp(info->getName(),"DABC/",5)==0)) {
      // old DABC rate record
      item.SetField("value", info->getFloat());
      item.SetField(dabc::prop_kind,"rate");
      iter->second.fKind = 2;
      iter->second.fDouble = info->getFloat();
   } else
   if ((strcmp(info->getFormat(),"I:1;C:16;C:128")==0) && (strncmp(info->getName(),"DABC/",5)==0)) {
      // old DABC info record
      item.SetField("value", (const char*) info->getData() + sizeof(int) + 16);
      item.SetField(dabc::prop_kind,"log");
   } else
   if (strlen(info->getFormat()) > 2) {
      const char* fmt = info->getFormat();

      // DOUT0("Process FORMAT %s", fmt);
      int fldcnt = 0;
      char* ptr = (char*) info->getData();
      while (*fmt != 0) {
         char kind = *fmt++;
         if (*fmt++!=':') break;
         int size = 0;
         while ((*fmt>='0') && (*fmt<='9')) {
            size = size*10 + (*fmt - '0');
            fmt++;
         }
         if (*fmt==';') fmt++;

         if (size<=0) break;

         // DOUT0("Process item %c size %d", kind, size);

         dabc::RecordField fld;
         switch(kind) {
            case 'I' :
               if (size==1) {
                  fld.SetInt(*((int*)ptr));
               } else {
                 std::vector<int64_t> vect;
                 for (int n=0;n<size;n++) vect.push_back(((int*)ptr)[n]);
                 fld.SetVectInt(vect);
               }
               ptr += sizeof(int) * size;
               break;
            case 'F' :
               if (size==1) {
                  fld.SetDouble(*((float*)ptr));
               } else {
                 std::vector<double> vect;
                 for (int n=0;n<size;n++) vect.push_back(((float*)ptr)[n]);
                 fld.SetVectDouble(vect);
               }
               ptr += sizeof(float) * size;
               break;
            case 'D' :
               if (size==1) {
                  fld.SetDouble(*((double*)ptr));
               } else {
                 std::vector<double> vect;
                 for (int n=0;n<size;n++) vect.push_back(((double*)ptr)[n]);
                 fld.SetVectDouble(vect);
               }
               ptr += sizeof(double) * size;
               break;
            case 'C' : {
               int slen(0);
               while ((slen<size) && (ptr[slen]!=0)) slen++;
               if (slen<size)
                  fld.SetStr(ptr);
               else
                  fld.SetStr(std::string(ptr,size));
               ptr += size;
               break;
            }
            default:
               EOUT("Unknown data format %c", kind);
               break;
         }
         // unrecognized field
         if (fld.null()) break;

         item.SetField(dabc::format("fld%d", fldcnt++), fld);
      }
   } else {
      if (strlen(info->getFormat())>0)
         EOUT("Not processed DIM format %s for record %s", info->getFormat(), info->getName());
      changed = false;
   }

   if (changed) item.MarkChangedItems();

}


void dimc::Player::SendDataToOutputs()
{

   mbs::SlowControlData rec;


   {
      dabc::LockGuard lock(fWorkerHierarchy.GetHMutex());

      for (DimServicesMap::iterator iter = fDimInfos.begin(); iter!=fDimInfos.end(); iter++) {
         switch (iter->second.fKind) {
            case 1: rec.AddLong(iter->first, iter->second.fLong); break;
            case 2: rec.AddDouble(iter->first, iter->second.fDouble); break;
            default: break;
         }
      }
   }

   unsigned nextsize = rec.GetRawSize();

   if (fIter.IsAnyEvent() && !fIter.IsPlaceForEvent(nextsize, true)) {

      // if output is blocked, do not produce data
      if (!CanSendToAllOutputs()) return;

      dabc::Buffer buf = fIter.Close();
      SendToAllOutputs(buf);

      fLastSendTime.GetNow();
   }

   if (!fIter.IsBuffer()) {
      dabc::Buffer buf = TakeBuffer();
      // if no buffer can be taken, skip data
      if (buf.null()) { EOUT("Cannot take buffer for EZCA data"); return; }
      fIter.Reset(buf);
   }

   if (!fIter.IsPlaceForEvent(nextsize, true)) {
      EOUT("EZCA event %u too large for current buffer size", nextsize);
      return;
   }

   fEventNumber++;

   struct timeb s_timeb;
   ftime(&s_timeb);

   rec.SetEventId(fEventNumber);
   rec.SetEventTime(s_timeb.time);

   fIter.NewEvent(fEventNumber);
   fIter.NewSubevent2(fSubeventId);

   unsigned size = rec.Write(fIter.rawdata(), fIter.maxrawdatasize());

   if (size==0) {
      EOUT("Fail to write data into MBS subevent");
   }

   fIter.FinishSubEvent(size);
   fIter.FinishEvent();

   if (fLastSendTime.Expired(fFlushTime) && CanSendToAllOutputs()) {
      dabc::Buffer buf = fIter.Close();
      SendToAllOutputs(buf);
      fLastSendTime.GetNow();
   }
}
