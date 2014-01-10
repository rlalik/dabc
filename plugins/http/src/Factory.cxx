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

#include "http/Factory.h"

#include "dabc/Manager.h"
#include "dabc/Configuration.h"
#include "http/Server.h"
#include "http/Mongoose.h"
#include "http/FastCgi.h"

dabc::FactoryPlugin httpfactory(new http::Factory("http"));


void http::Factory::Initialize()
{
   if (dabc::mgr.null()) return;

   // if dabc started without config file, do not automatically start http server
   if ((dabc::mgr()->cfg()==0) || (dabc::mgr()->cfg()->GetVersion()<=0)) return;

   dabc::XMLNodePointer_t node = 0;

   while (dabc::mgr()->cfg()->NextCreationNode(node, "HttpServer", true)) {

      const char* name = dabc::Xml::GetAttr(node, dabc::xmlNameAttr);
      const char* thrdname = dabc::Xml::GetAttr(node, dabc::xmlThreadAttr);

//      DOUT0("Found HttpServer node name = %s!!!", name ? name : "---");

      std::string objname;
      if (name!=0) objname = name;
      if (objname.length()==0) objname = "/http";
      if (objname[0]!='/') objname = std::string("/") + objname;
      if ((thrdname==0) || (*thrdname==0)) thrdname = "HttpThread";

      dabc::WorkerRef serv = new http::Mongoose(objname);
      serv.MakeThreadForWorker(thrdname);
      dabc::mgr.CreatePublisher();
   }

#ifndef DABC_WITHOUT_FASTCGI

   while (dabc::mgr()->cfg()->NextCreationNode(node, "FastCgiServer", true)) {

      const char* name = dabc::Xml::GetAttr(node, dabc::xmlNameAttr);
      const char* thrdname = dabc::Xml::GetAttr(node, dabc::xmlThreadAttr);

//      DOUT0("Found FastCgiServer node name = %s!!!", name ? name : "---");

      std::string objname;
      if (name!=0) objname = name;
      if (objname.length()==0) objname = "/fastcgi";
      if (objname[0]!='/') objname = std::string("/") + objname;
      if ((thrdname==0) || (*thrdname==0)) thrdname = "HttpThread";

      dabc::WorkerRef serv = new http::FastCgi(objname);
      serv.MakeThreadForWorker(thrdname);
      dabc::mgr.CreatePublisher();
   }

#endif

}

dabc::Reference http::Factory::CreateObject(const std::string& classname, const std::string& objname, dabc::Command cmd)
{
   if (classname == "http::Server")
      return new http::Server(objname, cmd);

   if (classname == "http::Mongoose")
      return new http::Mongoose(objname, cmd);

   if (classname == "http::FastCgi")
      return new http::FastCgi(objname, cmd);

   return dabc::Factory::CreateObject(classname, objname, cmd);
}
