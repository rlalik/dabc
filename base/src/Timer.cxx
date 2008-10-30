#include "dabc/Timer.h"

#include "dabc/logging.h"

dabc::Timer::Timer(Basic* parent, const char* name, double period, bool synchron) :
   ModuleItem(mitTimer, parent, name),
   fPeriod(period),
   fCounter(0),
   fActive(false),
   fSynhron(synchron),
   fInaccuracy(0.)
{
}

dabc::Timer::~Timer()
{
}

void dabc::Timer::SingleShoot(double delay)
{
   ActivateTimeout(delay);
}

void dabc::Timer::DoStart()
{
   fActive = true;
   
   if (fPeriod>0.)    
      ActivateTimeout(fPeriod);
}

void dabc::Timer::DoStop()
{
   fActive = false;
   
   ActivateTimeout(-1);
}

double dabc::Timer::ProcessTimeout(double last_diff)
{
   if (!fActive) return -1.;
    
   fCounter++; 
   
   ProduceUserEvent(evntTimeout);
   
   // if we have not synchron timer, just fire next event
   if (!fSynhron) return fPeriod;

   fInaccuracy += (last_diff - fPeriod);
   
   if (fInaccuracy>=fPeriod) {
      fInaccuracy -= fPeriod;
      return 0.; 
   }
   
   double res = fPeriod - fInaccuracy;
   fInaccuracy = 0.;
   return res;
}
