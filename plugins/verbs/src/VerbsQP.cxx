#include "dabc/VerbsQP.h"

#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include <infiniband/arch.h>

#include "dabc/VerbsDevice.h"
#include "dabc/VerbsCQ.h"

#include "dabc/logging.h"
#include "dabc/MemoryPool.h"


uint32_t dabc::VerbsQP::fQPCounter = 0;

// static int __qpcnt__ = 0;

dabc::VerbsQP::VerbsQP(VerbsDevice* verbs, ibv_qp_type qp_type,
                       VerbsCQ* send_cq, int send_depth, int max_send_sge,
                       VerbsCQ* recv_cq, int recv_depth, int max_recv_sge) :
   fVerbs(verbs),
   fType(qp_type),
   f_qp(0),
   f_local_psn(0),
   f_remote_lid(0),
   f_remote_qpn(0),
   fNumSendSegs(max_send_sge)
{
   if ((send_cq==0) || (recv_cq==0)) {
      EOUT(("No COMPLETION QUEUE WAS SPECIFIED"));
      return;
   }

   struct ibv_qp_init_attr attr;
   memset(&attr, 0, sizeof(struct ibv_qp_init_attr));
   attr.send_cq = send_cq->cq();
   attr.recv_cq = recv_cq->cq();
//   attr.sq_sig_all = 1; // if signall all operation, set 1 ????

   attr.cap.max_send_wr  = send_depth;
   attr.cap.max_send_sge = max_send_sge;
   
   attr.cap.max_recv_wr  = recv_depth;
   attr.cap.max_recv_sge = max_recv_sge;
   
   attr.cap.max_inline_data = VERBS_MAX_INLINE;
   attr.qp_type = fType;

   f_qp = ibv_create_qp(fVerbs->pd(), &attr);
   if (f_qp==0) {
      EOUT(("Couldn't create queue pair (QP)"));
      return;
   }

   struct ibv_qp_attr qp_attr;
   qp_attr.qp_state        = IBV_QPS_INIT;
   qp_attr.pkey_index      = 0;
   qp_attr.port_num        = verbs->IbPort();
   int res = 0;

   if (qp_type == IBV_QPT_UD) {
      qp_attr.qkey         = VERBS_DEFAULT_QKEY;
      res = ibv_modify_qp(f_qp, &qp_attr, (ibv_qp_attr_mask)
                          (IBV_QP_STATE              |
                          IBV_QP_PKEY_INDEX         |
                          IBV_QP_PORT               |
                          IBV_QP_QKEY));
   } else {
//      qp_attr.qp_access_flags = 0; when no RDMA is required
       
      qp_attr.qp_access_flags = 
         IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | 
                                   IBV_ACCESS_REMOTE_READ;    
      res = ibv_modify_qp(f_qp, &qp_attr, (ibv_qp_attr_mask)
                          (IBV_QP_STATE              |
                          IBV_QP_PKEY_INDEX         |
                          IBV_QP_PORT               |
                          IBV_QP_ACCESS_FLAGS));
   }

   if (res!=0) {
      EOUT(("Failed to modify QP to INIT state"));
      return;
   }

   fQPCounter += 1;
   f_local_psn = fQPCounter;

   DOUT4(("Create VerbsQP %p", this));
}

dabc::VerbsQP::~VerbsQP()
{
   DOUT4(("Destroy VerbsQP %p", this));
   
   ibv_destroy_qp(f_qp);

   DOUT4(("Destroy VerbsQP %p done", this));
}

bool dabc::VerbsQP::InitUD()
{
   struct ibv_qp_attr attr;
   memset(&attr, 0, sizeof attr);

   attr.qp_state       = IBV_QPS_RTR;
   attr.path_mtu       = IBV_MTU_2048;

   switch (fVerbs->mtu()) {
      case 256: attr.path_mtu = IBV_MTU_256; break;
      case 512: attr.path_mtu = IBV_MTU_512; break;
      case 1024: attr.path_mtu = IBV_MTU_1024; break;
      case 2048: attr.path_mtu = IBV_MTU_2048; break;
      case 4096: attr.path_mtu = IBV_MTU_4096; break;
      default: EOUT(("Wrong mtu value %u", fVerbs->mtu()));
   }
   
   attr.path_mtu = IBV_MTU_1024;

   if (ibv_modify_qp(qp(), &attr,
              IBV_QP_STATE )) {
       EOUT(("Failed to modify UD QP to RTR"));
      return false;
   }

   attr.qp_state   = IBV_QPS_RTS;
   attr.sq_psn     = local_psn();
   if (ibv_modify_qp(qp(), &attr, (ibv_qp_attr_mask)
                     (IBV_QP_STATE | IBV_QP_SQ_PSN))) {
      EOUT(("Failed to modify UC/UD QP to RTS"));
      return false;
   }

   f_remote_lid = 0;
   f_remote_qpn = 0;
   f_remote_psn = 0;

   return true;
}

bool dabc::VerbsQP::Connect(uint16_t dest_lid, uint32_t dest_qpn, uint32_t dest_psn)
{
   if (qp_type() == IBV_QPT_UD) {
      EOUT(("VerbsQP::Connect not supported for unreliable datagramm connection. Use InitUD() instead"));
      return false;
   }
    
    
   struct ibv_qp_attr attr;
   memset(&attr, 0, sizeof attr);

   attr.qp_state       = IBV_QPS_RTR;
   attr.path_mtu       = IBV_MTU_2048;

   switch (fVerbs->mtu()) {
      case 256: attr.path_mtu = IBV_MTU_256; break;
      case 512: attr.path_mtu = IBV_MTU_512; break;
      case 1024: attr.path_mtu = IBV_MTU_1024; break;
      case 2048: attr.path_mtu = IBV_MTU_2048; break;
      case 4096: attr.path_mtu = IBV_MTU_4096; break;
      default: EOUT(("Wrong mtu value %u", fVerbs->mtu()));
   }
   
   attr.path_mtu = IBV_MTU_1024;

   attr.dest_qp_num  = dest_qpn;
   attr.rq_psn       = dest_psn;
   if (qp_type() == IBV_QPT_RC) {
      attr.max_dest_rd_atomic     = 1;
      attr.min_rnr_timer          = 12;
   }

   attr.ah_attr.is_global  = 0;
   attr.ah_attr.dlid       = dest_lid;
   attr.ah_attr.sl         = 0;
   attr.ah_attr.src_path_bits = 0;
   attr.ah_attr.port_num   = fVerbs->IbPort(); // !!!!!!!  probably, here should be destination port

   if (qp_type() == IBV_QPT_RC) {
      if (ibv_modify_qp(qp(), &attr, (ibv_qp_attr_mask)
             (IBV_QP_STATE              |
              IBV_QP_AV                 |
              IBV_QP_PATH_MTU           |
              IBV_QP_DEST_QPN           |
              IBV_QP_RQ_PSN             |
              IBV_QP_MIN_RNR_TIMER      |
              IBV_QP_MAX_DEST_RD_ATOMIC))) {
         EOUT(("Failed here to modify RC QP to RTR lid: %x, qpn: %x, psn:%x", dest_lid, dest_qpn, dest_psn));
         return false;
      }
   } else

   if (qp_type() == IBV_QPT_UC) {
      if (ibv_modify_qp(qp(), &attr, (ibv_qp_attr_mask)
             (IBV_QP_STATE              |
              IBV_QP_AV                 |
              IBV_QP_PATH_MTU           |
              IBV_QP_DEST_QPN           |
              IBV_QP_RQ_PSN))) {
         EOUT(("Failed to modify UC QP to RTR"));
         return false;
      }
   } else

   if (qp_type() == IBV_QPT_UD) {
      if (ibv_modify_qp(qp(), &attr,
              IBV_QP_STATE )) {
         EOUT(("Failed to modify UD QP to RTR"));
         return false;
      }
   }

   attr.qp_state   = IBV_QPS_RTS;
   attr.sq_psn     = local_psn();
   if (qp_type() == IBV_QPT_RC) {
      attr.timeout       = 14;
      attr.retry_cnt     = 7;
      attr.rnr_retry     = 7;
      attr.max_rd_atomic = 1;
      if (ibv_modify_qp(qp(), &attr, (ibv_qp_attr_mask)
             (IBV_QP_STATE              |
              IBV_QP_SQ_PSN             |
              IBV_QP_TIMEOUT            |
              IBV_QP_RETRY_CNT          |
              IBV_QP_RNR_RETRY          |
              IBV_QP_MAX_QP_RD_ATOMIC))) {
         EOUT(("Failed to modify RC QP to RTS"));
         return false;
      }
   } else { /*both UC and UD */
      if (ibv_modify_qp(qp(), &attr, (ibv_qp_attr_mask)
             (IBV_QP_STATE              |
              IBV_QP_SQ_PSN))) {
         EOUT(("Failed to modify UC/UD QP to RTS"));
         return false;
      }
   }

//   if (qp_type() == IBV_QPT_UD) {
//      f_ah = ibv_create_ah(fVerbs->pd(), &(attr.ah_attr));
//      if (f_ah==0) {
//         EOUT(("Failed to create AH for UD"));
//         return false;
//      }
//   }

   f_remote_lid = dest_lid;
   f_remote_qpn = dest_qpn;
   f_remote_psn = dest_psn;
   
//   if (qp_type()!= IBV_QPT_UD)
//      DOUT1(("DO CONNECT local_qpn=%x remote_qpn=%x", qp_num(), dest_qpn));

   return true;
}

bool dabc::VerbsQP::Post_Send(struct ibv_send_wr* swr)
{
   struct ibv_send_wr* bad_swr = 0;

   if (ibv_post_send(qp(), swr, &bad_swr)) {
      EOUT(("ibv_post_send fails arg %lx", bad_swr->wr_id));
      return false;
   }
   return true;
}

bool dabc::VerbsQP::Post_Recv(struct ibv_recv_wr* rwr)
{
   struct ibv_recv_wr* bad_rwr = 0;

   if (ibv_post_recv(qp(), rwr, &bad_rwr)) {
      EOUT(("ibv_post_recv fails arg = %lx", bad_rwr->wr_id));
      return false;
   }

   return true;
}

bool dabc::VerbsQP::AttachMcast(ibv_gid* mgid, uint16_t mlid)
{
   return ibv_attach_mcast(qp(), mgid, mlid) == 0;
}

bool dabc::VerbsQP::DetachMcast(ibv_gid* mgid, uint16_t mlid)
{
  return ibv_detach_mcast(qp(), mgid, mlid) == 0;
}
