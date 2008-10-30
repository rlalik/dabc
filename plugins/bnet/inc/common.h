#ifndef BNET_common
#define BNET_common

#include <stdint.h>

#include <vector>

namespace bnet {
    
   extern const char* NetDevice; // prefered net device for building network
   extern bool NetBidirectional; // prefered configuration for network
   extern bool UseFileSource;    // configure usage of files as data source insted of generator
    
   enum EBnetBufferTypes {
      mbt_EvInfo    = 201, // events information
      mbt_EvAssign  = 202  // events assignment to builders
   };

   #define ReadoutQueueSize      4
   
   #define SenderInQueueSize     4
   #define SenderQueueSize       5

   #define ReceiverQueueSize     8
   #define ReceiverOutQueueSize  4
   
   #define BuilderInpQueueSize   4
   #define BuilderOutQueueSize   4

   #define FilterInpQueueSize    4
   #define FilterOutQueueSize    4

   #define CtrlInpQueueSize      4
   #define CtrlOutQueueSize      4
   
   #define BnetUseAcknowledge    false

   typedef uint64_t EventId;

#pragma pack(1)

   typedef struct SubEventNetHeader {
      uint64_t evid;
      uint64_t srcnode;
      uint64_t tgtnode;
      uint64_t pktid; 
   };

   typedef struct EventInfoRec {
      EventId  evid;
      int64_t  evsize;
   };

   typedef struct EventAssignRec {
      EventId  evid;
      uint64_t tgtnode;
   };


#pragma pack(0)

   class NodesVector {
      protected:
         std::vector<int> fUsedNodes;
         int fNumNodes;
       
      public:
         NodesVector() : fUsedNodes(), fNumNodes(0) {}
         NodesVector(const char* mask, int numnodes = 0) : 
            fUsedNodes(), 
            fNumNodes(0) {
               Reset(mask, numnodes);
            }
            
         virtual ~NodesVector() {}
         
         int NumNodes() const { return fNumNodes; }
         unsigned size() const { return fUsedNodes.size(); }
         int operator[](unsigned node) { return fUsedNodes[node]; }
         
         void Reset(const char* mask, int numnodes = 0);
         
         bool HasNode(int node) const;
       
   };
   
}

#endif
