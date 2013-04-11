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

#ifndef MBS_LmdOutput
#define MBS_LmdOutput

#ifndef DABC_DataIO
#include "dabc/DataIO.h"
#endif

#ifndef MBS_LmdFile
#include "mbs/LmdFile.h"
#endif

namespace mbs {

   class LmdOutput : public dabc::FileOutput {
      protected:

         mbs::LmdFile        fFile;

         bool CloseFile();
         bool StartNewFile();

      public:

         LmdOutput(const dabc::Url& url);
         virtual ~LmdOutput();

         virtual bool Write_Init(const dabc::WorkerRef& wrk, const dabc::Command& cmd);

         virtual unsigned Write_Buffer(dabc::Buffer& buf);
   };
}

#endif
