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

#ifndef ROC_Board
#define ROC_Board

#include <stdint.h>

#include "nxyter/Data.h"


namespace roc {

   enum BoardRole { roleNone, roleObserver, roleMaster, roleDAQ };

   class Device;

   class Board {
      protected:
         Device   *fDev;
         BoardRole fRole;

         Board(Device* dev = 0, BoardRole role = roleNone);

      public:
         virtual ~Board();

         BoardRole Role() const { return fRole; }

         int errno() const;

         bool poke(uint32_t addr, uint32_t value, double tmout = 5.);
         uint32_t peek(uint32_t addr, double tmout = 5.);

         bool startDaq();
         bool suspendDaq();
         bool stopDaq();

         bool getNextData(nxyter::Data& data, double tmout = 1.);


         static Board* Connect(const char* name, BoardRole role = roleObserver);
   };

}

#endif
