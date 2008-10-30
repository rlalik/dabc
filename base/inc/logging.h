#ifndef DABC_logging
#define DABC_logging

#include <stdio.h>

#include <stdint.h>

#ifndef DABC_string
#include "dabc/string.h"
#endif

#ifndef DABC_timing
#include "dabc/timing.h"
#endif


#ifndef DEBUGLEVEL
#define DEBUGLEVEL 1
#endif

namespace dabc {
    
   class LoggerEntry;
   class LoggerLineEntry;
   class Mutex;
   
   class Logger {
      public:
         enum EShowInfo { 
            lPrefix  = 0x0001,  // show configured debug prefix
            lDate    = 0x0002,  // show current date
            lTime    = 0x0004,  // show current time
            lFile    = 0x0008,  // show source file
            lFunc    = 0x0010,  // show source func
            lLine    = 0x0020,  // show line number
            lLevel   = 0x0040,  // show message level
            lMessage = 0x0080,  // show debug message itself
            lNoDrop  = 0x0100,  // disable drop of frequent messages
            lNoPrefix= 0x0200   // disable prefix output (superior to lPrefix)
         };
      
         Logger(bool withmutex = true);
         virtual ~Logger();
         
         // Debug mask sets fields, which are displayed for normal debug messages
         void SetDebugMask(unsigned mask) { fDebugMask = mask; }
         unsigned GetDebugMask() const  { return fDebugMask; }

         // Error mask sets fields, which are displayed for error messages
         void SetErrorMask(unsigned mask) { fErrorMask = mask; }
         unsigned GetErrorMask() const  { return fErrorMask; }

         // File mask can only add extra fields to the output
         void SetFileMask(unsigned mask) { fFileMask = mask; }
         unsigned GetFileMask() const  { return fFileMask; }

         void SetDebugLevel(int level = 0) { fDebugLevel = level; }
         inline int GetDebugLevel() const { return fDebugLevel; }

         void SetPrefix(const char* prefix = 0) { fPrefix = prefix ? prefix : ""; }
         const char* GetPrefix() { return fPrefix.c_str(); }
         
         void LogFile(const char* fname);
         
         void ShowStat();
         
         static inline Logger* Instance() { return gDebug; }

         static inline void Debug(int level, const char* filename, unsigned linenumber, const char* funcname, const char* message)
         {
            if (Instance() && (level <= Instance()->GetDebugLevel()))
               Instance()->DoOutput(level, filename, linenumber, funcname, message);
         }
         
      protected:

         static Logger* gDebug;
         
         virtual void DoOutput(int level, const char* filename, unsigned linenumber, const char* funcname, const char* message);
         
         void _ExtendLines(unsigned max);
         
         void _FillString(String& str, unsigned mask, LoggerEntry* entry);
         
      private:
         Logger*           fPrev;
         LoggerLineEntry **fLines;
         unsigned          fMaxLine;
         Mutex            *fMutex;
         FILE             *fFile;
         unsigned          fDebugMask;   // mask for debug output 
         unsigned          fErrorMask;   // mask for error output
         unsigned          fFileMask;    // mask for file output
         int               fDebugLevel;  // level of debug
         String            fPrefix;      // prefix of all messages
   };
   
   #define DOUT(level, args) \
     dabc::Logger::Debug(level, __FILE__, __LINE__, __func__, dabc::format args .c_str())

   #if DEBUGLEVEL > -2
      #define EOUT(args) DOUT(-1, args)
   #else
      #define EOUT(args)
   #endif 

   #if DEBUGLEVEL > -1
      #define DOUT0(args) DOUT(0, args)
   #else
      #define DOUT0(args)
   #endif   

   #if DEBUGLEVEL > 0
      #define DOUT1(args) DOUT(1, args)
   #else
      #define DOUT1(args)
   #endif   

   #if DEBUGLEVEL > 1
      #define DOUT2(args) DOUT(2, args)
   #else
      #define DOUT2(args)
   #endif   

   #if DEBUGLEVEL > 2
      #define DOUT3(args) DOUT(3, args)
   #else
      #define DOUT3(args)
   #endif   

   #if DEBUGLEVEL > 3
      #define DOUT4(args) DOUT(4, args)
   #else
      #define DOUT4(args)
   #endif   

   #if DEBUGLEVEL > 4
      #define DOUT5(args) DOUT(5, args)
   #else
      #define DOUT5(args)
   #endif   

   #define DBOOL(arg) (arg ? "true" : "false")

   #define DNAME(arg) (arg ? arg->GetName() : "---")

   extern void SetDebugLevel(int level = 0);
   extern void SetDebugPrefix(const char* prefix = 0);

// __________________________________________________________________________

   
   class CpuStatistic {
      public:
         CpuStatistic();
         virtual ~CpuStatistic();
   
         bool Reset();
         bool Measure();
   
         double CPUutil();
   
       protected:
         bool Get(unsigned long &system, unsigned long &user, unsigned long &idle);
   
         unsigned long system1, system2;
         unsigned long user1, user2;
         unsigned long idle1, idle2;
   
         FILE *fStatFp;
   };
   
   class Ratemeter {
     public:
        Ratemeter();
        virtual ~Ratemeter();
   
        void DoMeasure(double interval_sec, long npoints);
   
        void Packet(int size);
        void Reset();
   
        double GetRate();
        double GetTotalTime();
        int AverPacketSize();
   
        int GetNumOper() const { return numoper; }
   
        void SaveInFile(const char* fname);
   
        static void SaveRatesInFile(const char* fname, Ratemeter** rates, int nrates, bool withsum = false);
   
     protected:
        TimeStamp_t firstoper, lastoper;
        int64_t numoper, totalpacketsize;
   
        double fMeasureInterval; // interval between two points
        long fMeasurePoints;
        double* fPoints;
   };

   class Average {
      public:
         Average() { Reset(); }
         virtual ~Average() {}
         void Reset() { num = 0; sum1=0.; sum2=0.; min=0.; max=0.; }
         void Fill(double zn);
         long Number() const { return num; }
         double Mean() const { return num>0 ? sum1/num : 0.; }
         double Max() const { return max; }
         double Min() const { return min; }
         double Dev() const;
         void Show(const char* name, bool showextr = false);
      protected:
         long num;
         double sum1;
         double sum2;
         double min, max;
   };

   long GetProcVirtMem();
};

#endif

