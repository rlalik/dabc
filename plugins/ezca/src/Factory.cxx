#include "ezca/Factory.h"

#include <stdlib.h>

#include "dabc/Command.h"
#include "dabc/logging.h"
#include "dabc/Url.h"
#include "dabc/Manager.h"

#include "ezca/Definitions.h"
#include "ezca/EpicsInput.h"


const char* ezca::xmlEpicsName               = "EpicsIdentifier";
const char* ezca::xmlUpdateFlagRecord        = "EpicsFlagRec";
const char* ezca::xmlEventIDRecord           = "EpicsEventIDRec";
const char* ezca::xmlNumLongRecords          = "EpicsNumLongRecs";
const char* ezca::xmlNameLongRecords         = "EpicsLongRec-";
const char* ezca::xmlNumDoubleRecords        = "EpicsNumDoubleRecs";
const char* ezca::xmlNameDoubleRecords       = "EpicsDoubleRec-";
const char* ezca::xmlTimeout                 = "EpicsPollingTimeout";
const char* ezca::xmlEpicsSubeventId         = "EpicsSubeventId";
const char* ezca::xmlEzcaTimeout             = "EzcaTimeout";
const char* ezca::xmlEzcaRetryCount          = "EzcaRetryCount";
const char* ezca::xmlEzcaDebug               = "EzcaDebug";
const char* ezca::xmlEzcaAutoError           = "EzcaAutoError";


const char* ezca::nameUpdateCommand          =  "ezca::OnUpdate";
const char* ezca::xmlCommandReceiver         = "EpicsDabcCommandReceiver"; // Command receiver on flag change event


dabc::FactoryPlugin epicsfactory(new ezca::Factory("ezca"));


ezca::Factory::Factory(const std::string& name) :
   dabc::Factory(name)
{
}

dabc::DataInput* ezca::Factory::CreateDataInput(const std::string& typ)
{
   dabc::Url url(typ);
   if (url.GetProtocol()=="ezca")
      return new ezca::EpicsInput(url.GetHostName());

   return 0;
}
