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

#ifndef DABC_Port
#define DABC_Port

#ifndef DABC_ModuleItem
#include "dabc/ModuleItem.h"
#endif

#ifndef DABC_LocalTransport
#include "dabc/LocalTransport.h"
#endif

#ifndef DABC_ConnectionRequest
#include "dabc/ConnectionRequest.h"
#endif

namespace dabc {

   class PortRef;
   class Port;
   class Module;
   class ModuleAsync;
   class ModuleSync;

   /** \brief Base class for input and output ports
    *
    * \ingroup dabc_core_classes
    * \ingroup dabc_all_classes
    *
    */

   class Port : public ModuleItem {

      friend class Module;
      friend class ModuleAsync;
      friend class ModuleSync;
      friend class PortRef;

      public:
         // TODO: provide meaningful names
         enum EventsProducing {
            SignalNone = 0,   // port will not produce any events
            SignalConfirm,    // next event can be produced when previous event is received, confirmed and
                              // event consumer performed next operation
            SignalOperation,  // next event can be produced when event consumer performs next operation
            SignalEvery       // every operation will produce event
         };

      protected:
         unsigned           fQueueCapacity;
         Parameter          fRate;
         EventsProducing    fSignal;

         LocalTransportRef  fQueue;

         std::string        fBindName; // name of bind port

         std::string        fRateName; // name of rate parameter, which should be assigned to port

         unsigned           fMapLoopLength;

         double             fReconnectPeriod; // defines how often reconnect for port should be tried, -1 disable reconnect
         bool               fDoingReconnect;  // true if reconnection is now active

         /** \brief Inherited method, should cleanup everything */
         virtual void ObjectCleanup();

         virtual void DoStart();
         virtual void DoStop();

         Port(int kind, Reference parent,
                  const std::string& name,
                  unsigned queuesize);
         virtual ~Port();

         void SetQueue(Reference& ref);

         inline void ConfirmEvent()
         { if (SignallingKind() == SignalConfirm) fQueue.ConfirmEvent(GetType()==mitOutPort); }

         /** Return maximum number of events, which could be processed at once.
          * For internal usage */
         int GetMaxLoopLength();

         /** Should be called from constructors of derived classes to read port configuration like queue and so on */
         void ReadPortConfiguration();

         /** Return number of events which should be produced when async module starts */
         virtual unsigned NumStartEvents() { return 0; }

         /** Return reference on existing request object.
          * If object not exists and force flag is specified, new request object will be created */
         ConnectionRequest GetConnReq(bool force = false);

         /** Remove connection request - it does not automatically means that port will be disconnected */
         void RemoveConnReq();

         /** Method returns actual queue capacity of the port, object mutex is used */
         unsigned QueueCapacity() const;

         void Disconnect() { fQueue.Disconnect(IsInput()); }

         void SetBindName(const std::string& name);
         std::string GetBindName() const;

         /** Method can only be used from thread itself */
         bool IsConnected() const { return fQueue.IsConnected(); }

         /** Specify reconnect period or disable reconnection with -1 */
         void SetReconnectPeriod(double p = -1) { fReconnectPeriod = p; if (fReconnectPeriod<=0) SetDoingReconnect(false); }

         double GetReconnectPeriod() const { return fReconnectPeriod; }

         /** Return true if reconnection procedure started for the port */
         bool IsDoingReconnect() const { return fDoingReconnect; }

         void SetDoingReconnect(bool on = true) { fDoingReconnect = on; }

         /** Set port ratemeter - must be used from module thread */
         void SetRateMeter(const Parameter& ref);

         /** Specifies how many events can be processed at once. Could be used to balance load between ports */
         void SetMaxLoopLength(unsigned cnt) { fMapLoopLength = cnt; }

         bool SetSignalling(EventsProducing kind);
         EventsProducing SignallingKind() const { return fSignal; }

      public:

         virtual const char* ClassName() const { return "Port"; }

         virtual bool IsInput() const { return false; }
         virtual bool IsOutput() const { return false; }
   };

   // __________________________________________________________________________


   /** \brief Reference on the \ref dabc::Port class
    *
    * \ingroup dabc_user_classes
    * \ingroup dabc_all_classes
    *
    * Provides interface to the port functionality from outside module.
    */

   class PortRef : public ModuleItemRef {
      DABC_REFERENCE(PortRef, ModuleItemRef, Port)

      bool IsInput() const { return GetObject() ? GetObject()->IsInput() : false; }
      bool IsOutput() const { return GetObject() ? GetObject()->IsOutput() : false; }

      /** Returns queue capacity of the port - thread safe */
      unsigned QueueCapacity() const { return GetObject() ? GetObject()->QueueCapacity() : 0; }

      /** Returns signalling method configured for the port - thread safe */
      int GetSignallingKind();

      /** Returns true if port is connected - thread safe */
      bool IsConnected();

      /** Disconnect port - thread safe  */
      bool Disconnect();

      /** Return reference on the bind port - thread safe */
      PortRef GetBindPort();

      void EnableReconnect(double period = 1.) { if (GetObject()) GetObject()->SetReconnectPeriod(period); }

      void DisableReconnect() { if (GetObject()) GetObject()->SetReconnectPeriod(-1); }

      /** Create connection request to specified url - thread safe.
       * If connection to other dabc port is specified, isserver flag should identify which side is server
       * and which is client during connection establishing
       * TODO: one should try in future avoid isserver flag completely, it can be ruled later by connection manager */
      ConnectionRequest MakeConnReq(const std::string& url, bool isserver = false);
   };


   // =====================================================================================

   /** \brief Input port
    *
    * \ingroup dabc_core_classes
    * \ingroup dabc_all_classes
    */

   class InputPort : public Port {

      friend class Module;
      friend class ModuleAsync;
      friend class ModuleSync;

      private:
         InputPort(Reference parent,
                   const std::string& name,
                   unsigned queuesize);

      protected:

         virtual ~InputPort();

         virtual unsigned NumStartEvents();

         /** Defines how many buffers can be received */
         inline unsigned NumCanRecv() { return fQueue.Size(); }

         /** Returns true, when input queue is full and cannot get more buffers */
         bool QueueFull() { return fQueue.Full(); }

         Buffer Item(unsigned indx) { return fQueue.Item(indx); }

         /** Returns true if user can get (receive) buffer from the port */
         inline bool CanRecv() const { return fQueue.CanRecv(); }

         Buffer Recv() { Buffer buf; fQueue.Recv(buf); fRate.SetDouble(buf.GetTotalSize()/1024./1024.); return buf; }

         /** This method say framework that signal must be issued when queue will be fulled */
         void SignalWhenFull() { fQueue.SignalWhenFull(); }

         /** Remove buffer from the input queue. Return true when specified number of buffers were removed */
         bool SkipBuffers(unsigned cnt=1);

      public:

         virtual const char* ClassName() const { return "InputPort"; }

         virtual bool IsInput() const { return true; }
   };


   // =======================================================================================

   /** \brief Output port
    *
    * \ingroup dabc_core_classes
    * \ingroup dabc_all_classes
    */

   class OutputPort : public Port {

      friend class Module;
      friend class ModuleAsync;
      friend class ModuleSync;

      private:

         bool   fSendallFlag; // flag, used by SendToAllOutputs to mark, which output must be used for sending

         OutputPort(Reference parent,
                   const std::string& name,
                   unsigned queuesize);

      protected:

         virtual ~OutputPort();

         virtual unsigned NumStartEvents();

         /** Returns number of buffer which can be put to the queue */
         unsigned NumCanSend() const { return fQueue.NumCanSend(); }

         /** Returns true if user can send get buffer via the port */
         bool CanSend() const { return fQueue.CanSend(); }

         bool Send(dabc::Buffer& buf) { fRate.SetDouble(buf.GetTotalSize()/1024./1024.); return fQueue.Send(buf); }

      public:

         virtual const char* ClassName() const { return "OutputPort"; }

         virtual bool IsOutput() const { return true; }

   };

   // =======================================================================================

   /** \brief Handle for pool connection
    *
    * \ingroup dabc_core_classes
    * \ingroup dabc_all_classes
    */

   class PoolHandle : public Port {

      friend class Module;
      friend class ModuleAsync;
      friend class ModuleSync;

      private:

         Reference     fPool;

         PoolHandle(Reference parent,
                    Reference pool,
                    const std::string& name,
                    unsigned queuesize);

      protected:


         virtual ~PoolHandle();

         virtual void ObjectCleanup()
         {
            fPool.Release();
            Port::ObjectCleanup();
         }

         virtual unsigned NumStartEvents();

         // inline MemoryPool* Pool() const { return fPool(); }

         inline bool CanTakeBuffer() const
         {
            return (QueueCapacity()==0) ? true : fQueue.CanRecv();
         }

         Buffer TakeEmpty();

         Buffer TakeBuffer(BufferSize_t size = 0);

         unsigned NumRequestedBuffer() const
         {
            return fQueue.Size();
         }

         Buffer TakeRequestedBuffer()
         {
            Buffer buf; fQueue.Recv(buf); return buf;
         }

      public:

         virtual const char* ClassName() const { return "PoolHandle"; }

         virtual bool IsInput() const { return true; }
   };

}

#endif
