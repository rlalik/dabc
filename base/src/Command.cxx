/********************************************************************
 * The Data Acquisition Backbone Core (DABC)
 ********************************************************************
 * Copyright (C) 2009-
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 * Planckstr. 1
 * 64291 Darmstadt
 * Germany
 * Contact:  http://dabc.gsi.de
 ********************************************************************
 * This software can be used under the GPL license agreements as stated
 * in LICENSE.txt file which is part of the distribution.
 ********************************************************************/
#include "dabc/Command.h"

#include <stdlib.h>
#include <map>

#include "dabc/WorkingProcessor.h"
#include "dabc/logging.h"
#include "dabc/threads.h"
#include "dabc/XmlEngine.h"



class dabc::Command::CommandParametersList : public std::map<std::string, std::string> {};

namespace dabc {

   struct CallerRec {
      WorkingProcessor* proc;
      bool*             exe_ready;
      CallerRec() : proc(0), exe_ready(0) {}
   };

   class CallersQueue : public Queue<CallerRec> {} ;

}


dabc::Command::Command(const char* name) :
   Basic(0, name),
   fParams(0),
   fCallers(0),
   fCmdId(0),
   fCmdMutex(0)
{
   DOUT5(("New command %p name %s", this, GetName()));

//   fCmdMutex = new Mutex;

}

dabc::Command::~Command()
{
   {
      LockGuard lock(fCmdMutex);

      if (fCallers) {
         EOUT(("Non empty callers list in cmd %s !!!!!!!!!!!!!!", GetName()));
         exit(888);
         delete fCallers;
         fCallers = 0;
      }
   }

   if (fParams) {
      delete fParams;
      fParams = 0;
   }

   delete fCmdMutex; fCmdMutex = 0;

   DOUT5(("Delete command %p name %s", this, GetName()));
}

void dabc::Command::AddCaller(WorkingProcessor* proc, bool* exe_ready)
{
   LockGuard lock(fCmdMutex);

   if (fCallers==0) {
      fCallers = new CallersQueue;
      fCallers->Init(8, true);
   }

   CallerRec* rec = fCallers->PushEmpty();

   rec->proc = proc;
   rec->exe_ready = exe_ready;
}

void dabc::Command::RemoveCaller(WorkingProcessor* proc)
{
   LockGuard lock(fCmdMutex);

   if (fCallers==0) return;

   unsigned n(0);

   while (n<fCallers->Size())
      if (fCallers->Item(n).proc == proc)
         fCallers->RemoveItem(n);
      else
         n++;

   if (fCallers->Size()==0) {
      delete fCallers;
      fCallers = 0;
   }

}

bool dabc::Command::IsLastCallerSync()
{
   LockGuard lock(fCmdMutex);

   if ((fCallers==0) || (fCallers->Size()==0)) return false;

   return fCallers->Back().exe_ready != 0;
}


void dabc::Command::SetPar(const char* name, const char* value)
{
   if (value==0)
      RemovePar(name);
   else {
      if (fParams==0) fParams = new CommandParametersList;
      (*fParams)[name] = value;
   }
}

const char* dabc::Command::GetPar(const char* name) const
{
   if (fParams==0) return 0;
   CommandParametersList::const_iterator iter = fParams->find(name);
   if (iter==fParams->end()) return 0;
   return iter->second.c_str();
}

bool dabc::Command::HasPar(const char* name) const
{
   if (fParams==0) return false;

   return fParams->find(name) != fParams->end();

}

void dabc::Command::RemovePar(const char* name)
{
   if (fParams==0) return;
   CommandParametersList::iterator iter = fParams->find(name);
   if (iter!=fParams->end())
     fParams->erase(iter);
}

void dabc::Command::SetStr(const char* name, const char* value)
{
   SetPar(name, value);
}

const char* dabc::Command::GetStr(const char* name, const char* deflt) const
{
   const char* val = GetPar(name);
   return val ? val : deflt;
}

void dabc::Command::SetBool(const char* name, bool v)
{
   SetPar(name, v ? xmlTrueValue : xmlFalseValue);
}

bool dabc::Command::GetBool(const char* name, bool deflt) const
{
   const char* val = GetPar(name);
   if (val==0) return deflt;

   if (strcmp(val, xmlTrueValue)==0) return true; else
   if (strcmp(val, xmlFalseValue)==0) return false;

   return deflt;
}

void dabc::Command::SetInt(const char* name, int v)
{
   char buf[100];
   sprintf(buf,"%d",v);
   SetPar(name, buf);
}

int dabc::Command::GetInt(const char* name, int deflt) const
{
   const char* val = GetPar(name);
   if (val==0) return deflt;
   int res = 0;
   if (sscanf(val, "%d", &res) != 1) return deflt;
   return res;
}

void dabc::Command::SetDouble(const char* name, double v)
{
   char buf[100];
   sprintf(buf,"%lf",v);
   SetPar(name, buf);
}

double dabc::Command::GetDouble(const char* name, double deflt) const
{
   const char* val = GetPar(name);
   if (val==0) return deflt;
   double res = 0.;
   if (sscanf(val, "%lf", &res) != 1) return deflt;
   return res;
}

void dabc::Command::SetUInt(const char* name, unsigned v)
{
   char buf[100];
   sprintf(buf, "%u", v);
   SetPar(name, buf);
}

unsigned dabc::Command::GetUInt(const char* name, unsigned deflt) const
{
   const char* val = GetPar(name);
   if (val==0) return deflt;

   unsigned res = 0;
   if (sscanf(val, "%u", &res) != 1) return deflt;
   return res;
}

void dabc::Command::SetPtr(const char* name, void* p)
{
   char buf[100];
   sprintf(buf,"%p",p);
   SetPar(name, buf);
}

void* dabc::Command::GetPtr(const char* name, void* deflt) const
{
   const char* val = GetPar(name);
   if (val==0) return deflt;

   void* p = 0;
   int res = sscanf(val,"%p", &p);
   return res>0 ? p : deflt;
}

void dabc::Command::AddValuesFrom(const dabc::Command* cmd, bool canoverwrite)
{
   if ((cmd==0) || (cmd->fParams==0)) return;

   CommandParametersList::const_iterator iter = cmd->fParams->begin();

   while (iter!=cmd->fParams->end()) {
      const char* parname = iter->first.c_str();

      if (canoverwrite || !HasPar(parname))
         SetPar(parname, iter->second.c_str());
      iter++;
   }
}

void dabc::Command::SaveToString(std::string& v)
{
   v.clear();

   v = "Command";
   v+=": ";
   v+=GetName();
   v+=";";

   if (fParams==0) return;

   CommandParametersList::iterator iter = fParams->begin();

   for (;iter!=fParams->end();iter++) {

      // exclude streaming of parameters with symbol # in the name
      if (iter->first.find_first_of("#")!=std::string::npos) continue;

      v+=" ";
      v+=iter->first;
      v+=": ";
      if (iter->second.find_first_of(";:#$")==std::string::npos)
         v+=iter->second;
      else {
         v+="'";
         v+=iter->second;
         v+="'";
//         DOUT1(("!!!!!!!!!!!!!!!!!!! SPECIAL SYNTAX par = %s val = %s", iter->first.c_str(), iter->second.c_str()));
      }

      v+=";";
   }
}

bool dabc::Command::ReadFromString(const char* s, bool onlyparameters)
{
   // onlyparameters = true indicates, that only parameters values must be present
   // in the string, command name should not be in the string at all

   delete fParams; fParams = 0;

   if ((s==0) || (*s==0)) {
      EOUT(("Empty command"));
      return false;
   }

   const char myquote = '\'';

   const char* curr = s;

   int cnt = onlyparameters ? 1 : 0;

   while (*curr != 0) {
      const char* separ = strchr(curr, ':');

      if (separ==0) {
         EOUT(("Command format error 1: %s",s));
         return false;
      }

      std::string name(curr, separ-curr);

      if (cnt==0)
        if (name.compare("Command")!=0) {
          EOUT(("Wrong syntax, starts with: %s, expects: Command", name.c_str()));
          return false;
        }

      curr = separ+1;
      while (*curr == ' ') curr++;
      if (curr==0) {
         EOUT(("Command format error 4: %s",s));
         return false;
      }

      std::string val;

      if (*curr==myquote) {
         curr++;
         separ = strchr(curr, myquote);
         if ((separ==0) || (*(separ+1) != ';')) {
            EOUT(("Command format error 2: %s",s));
            return false;
         }

         val.assign(curr, separ-curr);
         separ++;

      } else {
         separ = strchr(curr, ';');
         if (separ==0) {
            EOUT(("Command format error 3: %s",s));
            return false;
         }
         val.assign(curr, separ-curr);
      }

      if (cnt==0)
         SetName(val.c_str());
      else
         SetPar(name.c_str(), val.c_str());

      cnt++;

      curr = separ+1;
      while (*curr==' ') curr++;
   }

   return true;
}

bool dabc::Command::ReadParsFromDimString(const char* pars)
{
   if ((pars==0) || (*pars==0)) return true;

   XmlEngine xml;

   XMLNodePointer_t node = xml.ReadSingleNode(pars);
   if (node==0) {
      EOUT(("Cannot parse DIM command %s", pars));
      return false;
   }

   XMLNodePointer_t child = xml.GetChild(node);

   while (child!=0) {

      if (xml.HasAttr(child, "name") && xml.HasAttr(child, "value")) {
         const char* parname = xml.GetAttr(child, "name");
         const char* parvalue = xml.GetAttr(child, "value");
         SetPar(parname, parvalue);
//         DOUT3(("Param %s = %s", parname, parvalue));
      }
      child = xml.GetNext(child);
   }

   xml.FreeNode(node);

   return true;
}


void dabc::Command::Reply(Command* cmd, int res)
{
   if (cmd) cmd->SetResult(res);
   dabc::Command::Finalise(cmd);
}

void dabc::Command::Finalise(Command* cmd)
{
   if (cmd==0) return;

   bool process = false;

   DOUT5(("Cmd %s Finalize", cmd->GetName()));

   do {

      process = false;
      CallerRec rec;

      {

         LockGuard lock(cmd->fCmdMutex);

         if ((cmd->fCallers==0) || (cmd->fCallers->Size()==0)) break;

         DOUT5(("Cmd %s Finalize callers %u", cmd->GetName(), cmd->fCallers->Size()));

         if (cmd->fCallers && cmd->fCallers->Size()>0) {
            rec = cmd->fCallers->PopBack();
            process = true;
            if (cmd->fCallers->Size()==0) {
               delete cmd->fCallers;
               cmd->fCallers = 0;
            }
         }
      }

      if (process && rec.proc) {
         process = rec.proc->GetCommandReply(cmd, rec.exe_ready);
         if (!process) { EOUT(("AAAAAAAAAAAAAAAAAAAAAAAA Problem with cmd %s", cmd->GetName())); }
      }
   } while (!process);

   if (!process) delete cmd;
}
