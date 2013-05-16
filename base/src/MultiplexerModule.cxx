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

#include "dabc/MultiplexerModule.h"

#include "dabc/Manager.h"

dabc::MultiplexerModule::MultiplexerModule(const std::string& name, dabc::Command cmd) :
   dabc::ModuleAsync(name, cmd),
   fQueue(100),
   fDataRateName()
{
   fDataRateName = Cfg("DataRateName", cmd).AsStdStr();

   if (!fDataRateName.empty())
      CreatePar(fDataRateName).SetRatemeter(false, 3.).SetUnits("MB");

   for (unsigned n=0;n<NumInputs();n++) {
      SetPortSignalling(InputName(n), dabc::Port::SignalEvery);
      if (!fDataRateName.empty())
         SetPortRatemeter(InputName(n), Par(fDataRateName));
   }

   DOUT0("Create multiplexer module %s with %u inputs and %u outputs", name.c_str(), NumInputs(), NumOutputs());
}

void dabc::MultiplexerModule::ProcessInputEvent(unsigned port)
{
   fQueue.Push(port);

//   DOUT0("MultiplexerModule %s process inp event from input %u", GetName(), id);

   CheckDataSending();
}

void dabc::MultiplexerModule::ProcessOutputEvent(unsigned indx)
{
   CheckDataSending();
}

void dabc::MultiplexerModule::CheckDataSending()
{
//   DOUT0("MultiplexerModule %s CheckDataSending queue %u canallsend:%s numout %u",
//         GetName(), fQueue.Size(), DBOOL(CanSendToAllOutputs()), NumOutputs());

   while (CanSendToAllOutputs() && (fQueue.Size()>0)) {
      unsigned id = fQueue.Pop();

      dabc::Buffer buf = Recv(id);

      if (buf.null()) EOUT("Fail to get buffer from input %u", id);

      SendToAllOutputs(buf);
   }
}
