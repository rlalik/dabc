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
#include "saftdabc/Device.h"

#include "dabc/logging.h"
#include "dabc/Command.h"
#include "dabc/DataTransport.h"
//#include "dabc/Manager.h"
//#include "dabc/Application.h"


#include "mbs/MbsTypeDefs.h"

//#include "mbs/Factory.h"

//#include "dabc/MemoryPool.h"
//#include "dabc/Buffer.h"
//#include "dabc/Pointer.h"

//#include "dabc/Port.h"

#include "saftdabc/Definitions.h"
//#include "saftdabc/Factory.h"
#include "saftdabc/Input.h"





saftdabc::Device::Device (const std::string& name, dabc::Command cmd) :
    dabc::Device (name)
{
  Gio::init(); // required at the beginnning ?
  fBoardName = Cfg (saftdabc::xmlDeviceName, cmd).AsStr (name);

  fGlibMainloop= Glib::MainLoop::create(); //Glib::MainLoop::create(true) to run immediately?
  std::map<Glib::ustring, Glib::ustring> devices = saftlib::SAFTd_Proxy::create()->getDevices();

  fTimingReceiver = saftlib::TimingReceiver_Proxy::create(devices[fBoardName]);


// TODO: export some interactive commands for web interface
     CreateCmdDef (saftdabc::commandRunMainloop);
    CreatePar (saftdabc::parEventRate).SetRatemeter (false, 3.).SetUnits ("Events");

    SetDevInfoParName("SaftDeviceInfo");
    CreatePar(fDevInfoName, "info").SetSynchron(true, 2., false).SetDebugLevel(2);
   // PublishPars ("$CONTEXT$/SaftDevice");
// note that access to web parameters is blocked after mainloop is executed in device thread!



  DOUT1("Created SAFTDABC device %s\n",fBoardName.c_str());

}

void saftdabc::Device::OnThreadAssigned()
{
  DOUT1("saftdabc::Device::OnThreadAssigned()...");
  dabc::Device::OnThreadAssigned();

  // we better start main loop in command execution context of device
  // RunGlibMainLoop(); // JAM immediately stay in main loop after thread has been started. does it work?
  //bool res=Submit(dabc::Command(saftdabc::commandRunMainloop)); // postpone mainloop into command execute
  //   DOUT1("saftdabc::Device::OnThreadAssigned() is leaving with mainloop command result=%s!",DBOOL(res));
}


void saftdabc::Device::RunGlibMainLoop()
{
  // TODO: put mainloop into background thread here?


  DOUT1("RunGlibMainLoop starts.");
  fGlibMainloop->run();
  DOUT1("RunGlibMainLoop after fGlibMainloop->run();");
}


saftdabc::Device::~Device ()
{
  DOUT1("DDDDDDDDDDd saftdabc::Device destructor \n");
  // NOTE: all time consuming cleanups and delete board is moved to object cleanup due to dabc shutdown mechanisms
}

bool saftdabc::Device::DestroyByOwnThread ()
{

  DOUT1("DDDDDDDDDDd saftdabc::Device DestroyByOwnThread()was called \n");
  // optionally clenaup something here?
  return dabc::Device::DestroyByOwnThread ();
}




void saftdabc::Device::ObjectCleanup ()
{
  DOUT1("_______saftdabc::Device::ObjectCleanup...");

  ClearConditions();
  fGlibMainloop->quit();
  dabc::Device::ObjectCleanup ();

  DOUT1("_______saftdabc::Device::ObjectCleanup leaving.");
}

int saftdabc::Device::ExecuteCommand (dabc::Command cmd)
{
  // probably we have to define special command to register conditions first and then run
  // main glib context loop here?




  // TODO: implement generic commands for:
  // start stop acquisition
  // open close file -> to readout module
  // enable/disable trigger
  // reset pexor/frontends (with virtual methods for user)
  // init frontends (with virtual methods for user)

  DOUT1 ("saftdabc::Device::ExecuteCommand-  %s", cmd.GetName ());
  bool res = false;
  if (cmd.IsName (saftdabc::commandRunMainloop))
  {
    DOUT1("Executing Command %s  ", saftdabc::commandRunMainloop);
    RunGlibMainLoop();
    return cmd_bool (res);;
  }
//  else if (cmd.IsName (saftdabc::commandStopAcq))
//  {
//    DOUT1("Executing Command %s  ", saftdabc::commandStopAcq);
//    res = StopAcquisition ();
//    return cmd_bool (res);;
//  }
//  else if (cmd.IsName (saftdabc::commandInitAcq))
//  {
//    DOUT1("Executing Command %s  ", saftdabc::commandInitAcq);
//    res = InitDAQ ();
//    return cmd_bool (res == 0);
//  }

//  else
    return dabc::Device::ExecuteCommand (cmd);
}

dabc::Transport* saftdabc::Device::CreateTransport (dabc::Command cmd, const dabc::Reference& port)
{
  dabc::PortRef portref = port;
  saftdabc::Input* dinput = new saftdabc::Input (this);
  DOUT0(
      "~~~~~~~~~~~~~~~~~ saftdabc::Device::CreateTransport port %s isinp %s", portref.ItemName ().c_str (), DBOOL (portref.IsInput ()));

  if (!dinput->Read_Init(port, cmd)) {
       EOUT("Input object for %s cannot be initialized", portref.ItemName ().c_str ());
       delete dinput;
       return 0;
   }

   dabc::InputTransport* transport =new dabc::InputTransport(cmd, portref, dinput, true);
   dinput->SetTransportRef(transport); // provide back reference for callback mode
  DOUT1("saftdabc::Device::CreateTransport creates new transport instance %p", transport);
  DOUT1 ("Device thread %p, %s\n", thread ().GetObject (), thread ().GetName ());
  return transport;
}





void saftdabc::Device::SetInfo(const std::string& info, bool forceinfo)
{
//   DOUT0("SET INFO: %s", info.c_str());

   dabc::InfoParameter par;

   if (!fDevInfoName.empty()) par = Par(fDevInfoName);

   par.SetValue(info);
   if (forceinfo)
      par.FireModified();
}



bool saftdabc::Device::RegisterInputCondition(saftdabc::Input* receiver, std::string& name)
{
  if(receiver==0) return false;

  try{
    bool found=false;
    const char* ioname=name.c_str(); // JAM need this hop because of map with Glib::ustring? better copy as is...

  std::map< Glib::ustring, Glib::ustring > inputs = fTimingReceiver->getInputs();
  //
  //  // here check if input names from config exist in the input list:
  for (std::map<Glib::ustring,Glib::ustring>::iterator it=inputs.begin(); it!=inputs.end(); ++it)
        {
              if (it->first == ioname)
                {
                  found=true;
                  break;
                }
        }
  if(!found) return false;

  // TODO: fix id definitions for appropriate inputs in dabc data stream!
  // up to now we append descriptions to dabc timing events
   guint64 prefix = ECA_EVENT_ID_LATCH + (fMap_PrefixName.size()*2);
  bool rev =
      RegisterEventCondition (receiver, prefix, -2, IO_CONDITION_OFFSET,
          (SAFT_DABC_ACCEPT_CONFLICT | SAFT_DABC_ACCEPT_DELAYED | SAFT_DABC_ACCEPT_EARLY | SAFT_DABC_ACCEPT_LATE));

  if(rev)
  {
    fMap_PrefixName[ioname] = prefix;  // TODO: instead of variable prefix mappings, usee fix id definitions for dabc data stream!
     DOUT0("Registered input condition for %s with prefix 0x%lx", ioname, prefix);
     SetInfo(dabc::format("Registered input condition for %s with prefix 0x%lx", ioname, prefix));
  }
  else
  {
    DOUT0("Error registering input condition %s ", ioname);
    return false;
  }
  /* Setup the event */
  Glib::RefPtr<saftlib::Input_Proxy> input = saftlib::Input_Proxy::create(inputs[ioname]);
  input->setEventEnable(false);
  input->setEventPrefix(prefix);
  input->setEventEnable(true);

  DOUT0("RegisterInputCondition: Input event is enabled."); // any more infos to show here?
  return true;



}
 catch (const Glib::Error& error)
 {
   /* Catch error(s) */
   EOUT("Glib error %s in RegisterInputCondition  for %s ", error.what().c_str(), name.c_str());
   return false;

 }



}

// todo: additional function arguments for condition flags (bool or another flagmask?)
bool saftdabc::Device::RegisterEventCondition (saftdabc::Input* receiver, guint64 id, guint64 mask, gint64 offset,
    unsigned char flags)
{
  if (receiver == 0)
    return false;
  bool acconflict = ((flags & SAFT_DABC_ACCEPT_CONFLICT)== SAFT_DABC_ACCEPT_CONFLICT);
  bool acdelayed = ((flags & SAFT_DABC_ACCEPT_DELAYED)== SAFT_DABC_ACCEPT_DELAYED);
  bool acearly = ((flags & SAFT_DABC_ACCEPT_EARLY)== SAFT_DABC_ACCEPT_EARLY);
  bool aclate = ((flags & SAFT_DABC_ACCEPT_LATE)== SAFT_DABC_ACCEPT_LATE);

  try
  {
    fActionSinks.push_back (saftlib::SoftwareActionSink_Proxy::create (fTimingReceiver->NewSoftwareActionSink ("")));
    fConditionProxies.push_back (
        saftlib::SoftwareCondition_Proxy::create (fActionSinks.back ()->NewCondition (true, id, mask, offset)));
    fConditionProxies.back ()->Action.connect (sigc::mem_fun (receiver, &saftdabc::Input::EventHandler));

    fConditionProxies.back ()->setAcceptConflict (acconflict);
    fConditionProxies.back ()->setAcceptDelayed (acdelayed);
    fConditionProxies.back ()->setAcceptEarly (acearly);
    fConditionProxies.back ()->setAcceptLate (aclate);

    fConditionProxies.back ()->Disown ();    // do we need this here?

    // DOUT0("RegisterEventCondition: Creating condition proxy %s done.",fConditionProxies.back()->get_name().c_str());
    // JAM the above does not work since currently saftlib breaks interface inheritance to Gio::Dbus::Proxy !

    DOUT0(
        "RegisterEventCondition: id=0x%lx , mask=0x%lx , offset=0x%lx , early=%d, late=%d, conflict=%d, delayed=%d", id, mask, offset, acearly, aclate, acconflict, acdelayed);
    // any more infos to show here?
    SetInfo (
        dabc::format (
            "RegisterEventCondition: id=0x%lx , mask=0x%lx , offset=0x%lx , early=%d, late=%d, conflict=%d, delayed=%d",
            id, mask, offset, acearly, aclate, acconflict, acdelayed));
    return true;
  }
  catch (const Glib::Error& error)
  {
    /* Catch error(s) */
    EOUT(
        "Glib error %s in RegisterEventCondition for id=0x%lx , mask=0x%lx , offset=0x%lx , early=%d, late=%d, conflict=%d, delayed=%d", error.what().c_str(), id, mask, offset, acearly, aclate, acconflict, acdelayed);
    return false;

  }

}



const std::string saftdabc::Device::GetInputDescription (guint64 event)
{
  try
  {
    Glib::ustring catched_io = "WR_Event";
    for (std::map<Glib::ustring, guint64>::iterator it = fMap_PrefixName.begin (); it != fMap_PrefixName.end (); ++it)
    {
      if (event == it->second)
      {
        catched_io = it->first;
      }
    } /* Rising */
    for (std::map<Glib::ustring, guint64>::iterator it = fMap_PrefixName.begin (); it != fMap_PrefixName.end (); ++it)
    {
      if (event - 1 == it->second)
      {
        catched_io = it->first;
      }
    } /* Falling */

    if (catched_io != "WR_Event")
    {
      if ((event & 1))
      {
        catched_io.append (" Rising");
      }
      else
      {
        catched_io.append (" Falling");
      }
    }

    return catched_io.raw ();
  }
  catch (const Glib::Error& error)
  {
    /* Catch error(s) */
    EOUT("Glib error %s in GetInputDescription", error.what().c_str());
    return std::string ("NONE");

  }
}

void saftdabc::Device::AddEventStatistics (unsigned numevents)
{
  Par (saftdabc::parEventRate).SetValue (numevents);

}

bool saftdabc::Device::ClearConditions ()
{

  try
  {
    // first destroy conditions if possible:
    // we use our existing handles in container:_
    for (std::vector<Glib::RefPtr<saftlib::SoftwareCondition_Proxy> >::iterator cit = fConditionProxies.begin ();
        cit != fConditionProxies.end (); ++cit)
    {
      Glib::RefPtr < saftlib::SoftwareCondition_Proxy > destroy_condition = *cit;
      if (destroy_condition->getDestructible () && (destroy_condition->getOwner () == ""))
      {
        destroy_condition->Destroy ();
        // JAM: why does saft-io-ctl example use here container
        //     std::vector <Glib::RefPtr<saftlib::InputCondition_Proxy> > prox;
        // with statements like:
        //         prox.push_back ( saftlib::InputCondition_Proxy::create(name));
        //         prox.back()->Destroy();
        // non optimized code or mandatory for some reasons?
        //
        DOUT0("ClearConditions destroyed condition of ID:0x%x .", destroy_condition->getID());
      }
      else
      {

        DOUT0(
            "ClearConditions found non destructible condition of ID:0x%x , owner=%s", destroy_condition->getID(), destroy_condition->getOwner().c_str());
      }
    }

   // then clean up action sinks:
   // the remote action sinks in saftd service will have removed their conditions on Destroy()
   // do we need to get rid of the sinks explicitely?
   // for the moment, just remove references, seems that dtor will send appropriate signals to service JAM2016-9
    fActionSinks.clear();

  }
  catch (const Glib::Error& error)
  {
    /* Catch error(s) */
    EOUT("Glib error %s in SetupConditions", error.what().c_str());
    return false;

  }

  return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
//////// following is taken from saftlib Commonfunctions.cpp which is not available in include interface

std::string saftdabc::tr_formatDate(guint64 time, guint32 pmode)
{
  guint64 ns    = time % 1000000000;
  time_t  s     = time / 1000000000;
  struct tm *tm = gmtime(&s);
  char date[40];
  char full[80];
  std::string temp;

  if (pmode & PMODE_VERBOSE) {
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", tm);
    snprintf(full, sizeof(full), "%s.%09ld", date, (long)ns);
  }
  else if (pmode & PMODE_DEC)
    snprintf(full, sizeof(full), "0d%lu.%09ld",s,(long)ns);
  else
    snprintf(full, sizeof(full), "0x%016llx", (unsigned long long)time);

  temp = full;

  return temp;
} //tr_formatDate

/* format EvtID to a string */
std::string saftdabc::tr_formatActionEvent(guint64 id, guint32 pmode)
{
  std::stringstream full;
  std::string fmt = "";
  int width = 0;

  if (pmode & PMODE_HEX) {full << std::hex << std::setfill('0'); width = 16; fmt = "0x";}
  if (pmode & PMODE_DEC) {full << std::dec << std::setfill('0'); width = 20; fmt = "0d";}
  if (pmode & PMODE_VERBOSE) {
    full << " FID: "   << fmt << std::setw(1) << ((id >> 60) & 0xf);
    full << " GID: "   << fmt << std::setw(4) << ((id >> 48) & 0xfff);
    full << " EVTNO: " << fmt << std::setw(4) << ((id >> 36) & 0xfff);
    full << " SID: "   << fmt << std::setw(4) << ((id >> 24) & 0xfff);
    full << " BPID: "  << fmt << std::setw(5) << ((id >> 14) & 0x3fff);
    full << " RES: "   << fmt << std::setw(4) << (id & 0x3ff);
  }
  else full << " EvtID: " << fmt << std::setw(width) << std::setfill('0') << id;

  return full.str();
} //tr_formatActionEvent

std::string saftdabc::tr_formatActionParam(guint64 param, guint32 evtNo, guint32 pmode)
{
  std::stringstream full;
  std::string fmt = "";
  int width = 0;

  if (pmode & PMODE_HEX) {full << std::hex << std::setfill('0'); width = 16; fmt = "0x";}
  if (pmode & PMODE_DEC) {full << std::dec << std::setfill('0'); width = 20; fmt = "0d";}

  switch (evtNo) {
  case 0x0 :
    // add some code
    break;
  case 0x1 :
    // add some code
    break;
  default :
    full << " Param: " << fmt << std::setw(width) << param;
  } // switch evtNo

  return full.str();
} // tr_formatActionParam

std::string saftdabc::tr_formatActionFlags(guint16 flags, guint64 delay, guint32 pmode)
{
  std::stringstream full;

  full << "";

  if (flags) {
    full  << "!";
    if (flags & 1) full << "late (by " << delay << " ns)";
    if (flags & 2) full << "early (by " << -delay << " ns)";
    if (flags & 4) full << "conflict (delayed by " << delay << " ns)";
    if (flags & 8) full << "delayed (by " << delay << " ns)";
  } // if flags

  return full.str();
} // tr_formatActionFlags



