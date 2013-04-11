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

#include "dabc/CommandsSet.h"

#include "dabc/logging.h"
#include "dabc/Manager.h"
#include "dabc/Thread.h"

dabc::CommandsSet::CommandsSet(ThreadRef thrd, bool parallel) :
   Worker(0, "set", true),
   fReceiver(dabc::mgr()),
   fCmds(10),
   fParallelExe(parallel),
   fConfirm(),
   fSyncMode(false),
   fCompleted(false)
{
   if (thrd()) AssignToThread(thrd, true);
}

dabc::CommandsSet::~CommandsSet()
{
   fCmds.Reset();
}

void dabc::CommandsSet::Cleanup()
{
   fCmds.Reset();
   fCompleted = false;
}

void dabc::CommandsSet::SetReceiver(Worker* proc)
{
   fReceiver = proc;
}

void dabc::CommandsSet::Add(Command cmd, Worker* recv, bool do_submit)
{
   if (cmd.null()) return;

   if (!HasThread() && !do_submit) {
      EOUT("Command %s cannot be submitted by the user when set not assigned to the thread!", cmd.GetName());
   }

   CmdRec rec;

   rec.cmd = cmd;
   rec.recv = recv ? recv : fReceiver;
   rec.state = do_submit ? 0 : 1;

   if (!do_submit) Assign(cmd);

   fCmds.Push(rec);
}

unsigned dabc::CommandsSet::GetNumber() const
{
   return fCmds.Size();
}

dabc::Command dabc::CommandsSet::GetCommand(unsigned n)
{
   if (n>=fCmds.Size()) return dabc::Command();

   if (fCmds.Item(n).state != 2) return dabc::Command();

   return fCmds.Item(n).cmd;
}

int dabc::CommandsSet::GetCmdResult(unsigned n)
{
   dabc::Command cmd = GetCommand(n);
   return cmd.null() ? cmd_timedout : cmd.GetResult();
}

int dabc::CommandsSet::ExecuteCommand(Command cmd)
{
   if (!fConfirm.null()) {
      EOUT("Something wrong - confirmation command already exists");
      return Worker::ExecuteCommand(cmd);
   }

   DOUT4("CommandsSet get command %s", cmd.GetName());

   // first submit commands which should be submitted
   while (SubmitNextCommand())
     if (!fParallelExe) break;

   int res = CheckExecutionResult();

   if (res==cmd_postponed)
      fConfirm = cmd;
   else {
      if (!fCompleted) SetCompleted(res);
      fCompleted = true;
   }

   DOUT4("CommandsSet execution res = %d", res);

   return res;
}

bool dabc::CommandsSet::ReplyCommand(Command cmd)
{
   for (unsigned n=0; n<fCmds.Size();n++) {
      if (fCmds.Item(n).cmd == cmd) {
         if (fCmds.Item(n).state != 1)
            EOUT("Wrong state %d for command %s at reply", fCmds.Item(n).state, cmd.GetName());
         fCmds.Item(n).state = 2;

         DOUT5("Command %s replied res = %d", cmd.GetName(), cmd.GetResult());
         break;
      }
   }

   SetCommandCompleted(cmd);

   SubmitNextCommand();

   int res = CheckExecutionResult();

   if (res!=cmd_postponed) {
      DOUT4("CommandsSet completed res = %d !!!", res);

      if (!fCompleted) SetCompleted(res);
      fCompleted = true;

      fConfirm.Reply(res);

      if (!fSyncMode) DeleteThis();
   }

   // keep commands stored to have access to them later
   return false;
}

bool dabc::CommandsSet::SubmitNextCommand()
{
   for (unsigned n=0; n<fCmds.Size();n++)
      if (fCmds.Item(n).state==0) {
         if (fCmds.Item(n).recv) {

            DOUT5("CommandsSet distributes cmd %s  item %u size %u", fCmds.Item(n).cmd.GetName(), n, fCmds.Size());

            fCmds.Item(n).state = 1;

            fCmds.Item(n).recv->Submit(Assign(fCmds.Item(n).cmd));

            return true;

         } else {
            EOUT("Not able submit next command %s", fCmds.Item(n).cmd.GetName());
            fCmds.Item(n).state = 2;
         }
      }

   return false;
}

int dabc::CommandsSet::CheckExecutionResult()
{
   int res = cmd_true;

   for (unsigned n=0; n<fCmds.Size();n++) {
      if (fCmds.Item(n).state!=2)
         return cmd_postponed;

      if (fCmds.Item(n).cmd.GetResult()==cmd_false)
         res = cmd_false;
   }

   return res;
}

int dabc::CommandsSet::ExecuteSet(double tmout)
{
   Thread curr(0, "Current");

   bool didassign = false;

   if (!HasThread()) {

      if (dabc::mgr()==0) {
         EOUT("Cannot use CommandsSet without running manager !!!");
         return cmd_false;
      }

      ThreadRef thrd = dabc::mgr.CurrentThread();
      if (thrd.null()) {
         DOUT4("Cannot use commands set outside DABC threads, create dummy !!!");
         curr.Start(0, false);
         thrd = ThreadRef(&curr);
      }

      DOUT4("Assign SET to thread %s", thrd()->GetName());

      AssignToThread(ThreadRef(thrd));

      didassign = true;
   }

   DOUT4("Calling AnyCommand");

   fSyncMode = true;

   int res = ExecuteIn(this, Command("AnyCommand").SetTimeout(tmout));

   CancelCommands();

   DOUT1("Calling AnyCommand res = %d", res);

   if (didassign)
      DettachFromThread();

   return res;
}

bool dabc::CommandsSet::SubmitSet(Command rescmd, double tmout)
{
   if (!HasThread()==0) {
      if (dabc::mgr()==0) {
         EOUT("Cannot use CommandsSet without running manager !!!");
         return false;
      }

      ThreadRef thrd = dabc::mgr.CurrentThread();
      if (thrd.null()) thrd = dabc::mgr.thread();
      if (thrd.null()) {
         EOUT("Cannot use commands set outside DABC threads !!!");
         return false;
      }

      DOUT0("Assign SET to thread %s", thrd()->GetName());

      AssignToThread(thrd);
   }

   if (rescmd.null()) rescmd = dabc::Command("AnyCommand");

   fSyncMode = false;

   if (tmout>0) ActivateTimeout(tmout);

   return Submit(rescmd);
}

double dabc::CommandsSet::ProcessTimeout(double last_diff)
{
   EOUT("CommandsSet Timeout !!!!!!!!!!!!!");

   if (!fCompleted) SetCompleted(cmd_timedout);
   fCompleted = true;

   fConfirm.Reply(cmd_timedout);

   if (!fSyncMode) DeleteThis();

   return -1;
}

