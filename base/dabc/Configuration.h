#ifndef DABC_Configuration
#define DABC_Configuration

#ifndef DABC_ConfigBase
#include "dabc/ConfigBase.h"
#endif

#ifndef DABC_ConfigIO
#include "dabc/ConfigIO.h"
#endif

#include <list>


namespace dabc {

   class Basic;

   class Configuration : public ConfigBase,
                         public ConfigIO {
      protected:
         XMLNodePointer_t  fSelected; // selected context node

         std::list<XMLNodePointer_t> fStoreStack; // stack of nodes during store
         XMLNodePointer_t fStoreLastPop; // last pop-ed item

         XMLNodePointer_t fCurrItem; // currently found item
         XMLNodePointer_t fCurrChld; // selected child in current item
         XMLNodePointer_t fLastTop; // currently found item
         bool             fCurrStrict; // must have strict syntax match

         std::string      fMgrHost;
         std::string      fMgrName;
         int              fMgrNodeId;
         int              fMgrNumNodes;
         std::string      fMgrDimNode;
         int              fMgrDimPort;

         bool XDAQ_LoadLibs();
         bool XDAQ_ReadPars();

         Basic* GetObjParent(Basic* obj, int lvl);

         XMLNodePointer_t FindSubItem(XMLNodePointer_t node, const char* name);

      public:
         Configuration(const char* fname = 0);
         virtual ~Configuration();

         bool SelectContext(unsigned cfgid, unsigned nodeid, unsigned numnodes, const char* dimnode = 0);

         std::string MgrHost() const { return fMgrHost; }
         const char* MgrName() const { return fMgrName.c_str(); }
         int MgrNodeId() const { return fMgrNodeId; }
         int MgrNumNodes() const { return fMgrNumNodes; }
         const char* MgrDimNode() const { return fMgrDimNode.c_str(); }
         int MgrDimPort() const { return fMgrDimPort; }

         std::string InitFuncName();
         std::string RunFuncName();
         int ShowCpuInfo();

         const char* ConetextAppClass();

         bool LoadLibs();

         bool ReadPars();

         virtual bool CreateItem(const char* name, const char* value = 0);
         virtual bool CreateAttr(const char* name, const char* value);
         virtual bool PopItem();
         virtual bool PushLastItem();

         bool StoreObject(const char* fname, Basic* obj);

         std::string GetAttrValue(const char* name);
         bool FindItem(Basic* obj, std::string& res, const char* finditem = 0, const char* findattr = 0);

         virtual bool FindItem(const char* name);
         virtual bool CheckAttr(const char* name, const char* value);

         virtual bool Find(Basic* obj, std::string& value, const char* findattr = 0);
   };


}

#endif
