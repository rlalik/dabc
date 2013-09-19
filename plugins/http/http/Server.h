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

#ifndef HTTP_Server
#define HTTP_Server

#ifndef DABC_Worker
#include "dabc/Worker.h"
#endif

#ifndef DABC_Hierarchy
#include "dabc/Hierarchy.h"
#endif

#include "mongoose.h"
#include <stdlib.h>

#include <map>

namespace http {

   /** \brief %Server provides http access to DABC
    *
    */

   class Server : public dabc::Worker  {

      struct FileBuf {
         void *ptr;
         int size;

         FileBuf() : ptr(0), size(0) {}
         FileBuf(const FileBuf& src) : ptr(0), size(0) {}
         ~FileBuf() { release(); }

         void release() {
            if (ptr) free(ptr);
            ptr = 0; size = 0;
         }

      };

      typedef std::map<std::string, FileBuf> FilesMap;

      protected:
         bool fEnabled;              ///< is server enabled
         int fHttpPort;              ///< port number for HTTP server

         struct mg_context *   fCtx;
         struct mg_callbacks   fCallbacks;

         FilesMap fFiles;          ///< map with read into memory files
         std::string fHttpSys;     ///< location of http plugin, need to read special files
         std::string fGo4Sys;      ///< location of go4 (if any)
         std::string fRootSys;     ///< location of ROOT (if any)
         std::string fJSRootIOSys; ///< location of JSRootIO (if any)


         virtual void OnThreadAssigned();

         virtual double ProcessTimeout(double last_diff);

         bool ProcessGetItem(struct mg_connection* conn, const std::string& itemname, const char *query, std::string& replybuf);

         bool ProcessGetBin(struct mg_connection* conn, const std::string& itemname, const char *query);

         bool ProcessGetPng(struct mg_connection* conn, const std::string& itemname, const char *query);

         /** \brief Check if file is requested. Can only be from server */
         bool IsFileName(const char* path);

         bool MakeRealFileName(std::string& fname);

      public:
         Server(const std::string& name, dabc::Command cmd = 0);

         virtual ~Server();

         virtual const char* ClassName() const { return "HttpServer"; }

         bool IsEnabled() const { return fEnabled; }

         int begin_request(struct mg_connection *conn);

         const char* open_file(const struct mg_connection* conn,
                               const char *path, size_t *data_len = 0);
   };

}

#endif
