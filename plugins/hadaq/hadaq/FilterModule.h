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

#ifndef HADAQ_FilterModule
#define HADAQ_FilterModule

#ifndef DABC_ModuleAsync
#include "dabc/ModuleAsync.h"
#endif


namespace hadaq {

/** \brief Filter out some HADAQ events
 *
 * Custom code - to be adjusted to some conditions
 */

   class FilterModule : public dabc::ModuleAsync {

      bool retransmit();

      int ExecuteCommand(dabc::Command cmd) override;

   public:
      FilterModule(const std::string &name, dabc::Command cmd = nullptr);

      bool ProcessBuffer(unsigned) override { return retransmit(); }

      bool ProcessRecv(unsigned) override { return retransmit(); }

      bool ProcessSend(unsigned) override { return retransmit(); }

      void ProcessTimerEvent(unsigned) override;

   };

}

#endif
