// $Id$

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

#include <cstdio>
#include <cstring>
#include <string>

#include "dabc/Manager.h"

#include "dabc/api.h"


int usage(const char* errstr = nullptr)
{
   if (errstr)
      printf("Error: %s\n\n", errstr);

   printf("Utility for dogma\n");

   return errstr ? 1 : 0;
}

int main(int argc, char* argv[])
{
   if ((argc < 2) || !strcmp(argv[1], "-help") || !strcmp(argv[1], "?"))
      return usage();

   if (!dabc::CreateManager("shell", 0)) {
      printf("Fail to create manager\n");
      return 1;
   }

   printf("Did create manager\n");

   std::string nodename = dabc::MakeNodeName(argv[1]);

   printf("Build node name %s\n", nodename.c_str());

   if (!dabc::ConnectDabcNode(nodename)) {
      printf("Fail to connect to node %s\n", nodename.c_str());
      return 1;
   }

   printf("Did connect to node %s\n", nodename.c_str());

   std::string module_name = nodename + "/dogma";

   dabc::Command cmd("GenericRead");
   cmd.SetReceiver(module_name);
   cmd.SetInt("Addr", 0x1000);
   cmd.SetTimeout(10);

   int res = dabc::mgr.Execute(cmd);

   printf("Command execution %d value %d\n", res, cmd.GetInt("Value"));

   return 0;
}
