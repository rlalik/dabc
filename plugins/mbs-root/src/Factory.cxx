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
#include "mbs_root/Factory.h"

#include "dabc/string.h"
#include "dabc/logging.h"
#include "dabc/Command.h"
#include "dabc/Manager.h"

#include "mbs_root/RootTreeOutput.h"

dabc::FactoryPlugin mbsrootfactory(new mbs_root::Factory("mbs-root"));

dabc::DataInput* mbs_root::Factory::CreateDataInput(const char* typ)
{
   return 0;
}


dabc::DataOutput* mbs_root::Factory::CreateDataOutput(const char* typ)
{

   DOUT3(("mbs_root::Factory::CreateDataOutput typ:%s", typ));

   if (strcmp(typ, "RootTree")==0) {
      return new mbs_root::RootTreeOutput();
   }

   return 0;
}
