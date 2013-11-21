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

#include "dabc_root/BinaryProducer.h"

#include <math.h>
#include <stdlib.h>
#include <fstream>


#include "dabc/logging.h"
#include "dabc/timing.h"

#include "TMemFile.h"
#include "TStreamerInfo.h"
#include "TBufferFile.h"
#include "TROOT.h"
#include "TH1.h"
#include "TGraph.h"
#include "TPad.h"
#include "TImage.h"
#include "TSystem.h"
#include "RVersion.h"


extern "C" void R__zipMultipleAlgorithm(int cxlevel, int *srcsize, char *src, int *tgtsize, char *tgt, int *irep, int compressionAlgorithm);


dabc_root::BinaryProducer::BinaryProducer(const std::string& name, int compr) :
   dabc::Object(0, name),
   fCompression(compr),
   fMemFile(0),
   fSinfoSize(0)
{
}

dabc_root::BinaryProducer::~BinaryProducer()
{
   if (fMemFile!=0) { delete fMemFile; fMemFile = 0; }
}

void dabc_root::BinaryProducer::CreateMemFile()
{
   if (fMemFile!=0) return;

   TDirectory* olddir = gDirectory; gDirectory = 0;
   TFile* oldfile = gFile; gFile = 0;

   fMemFile = new TMemFile("dummy.file", "RECREATE");
   gROOT->GetListOfFiles()->Remove(fMemFile);

   TH1F* d = new TH1F("d","d", 10, 0, 10);
   fMemFile->WriteObject(d, "h1");
   delete d;

   TGraph* gr = new TGraph(10);
   gr->SetName("abc");
   //      // gr->SetDrawOptions("AC*");
   fMemFile->WriteObject(gr, "gr1");
   delete gr;

   fMemFile->WriteStreamerInfo();

   // make primary list of streamer infos
   TList* l = new TList();

   l->Add(gROOT->GetListOfStreamerInfo()->FindObject("TGraph"));
   l->Add(gROOT->GetListOfStreamerInfo()->FindObject("TH1F"));
   l->Add(gROOT->GetListOfStreamerInfo()->FindObject("TH1"));
   l->Add(gROOT->GetListOfStreamerInfo()->FindObject("TNamed"));
   l->Add(gROOT->GetListOfStreamerInfo()->FindObject("TObject"));

   fMemFile->WriteObject(l, "ll");
   delete l;

   fMemFile->WriteStreamerInfo();

   l = fMemFile->GetStreamerInfoList();
   // l->Print("*");
   fSinfoSize = l->GetSize();
   delete l;

   gDirectory = olddir;
   gFile = oldfile;
}


dabc::Buffer dabc_root::BinaryProducer::CreateBindData(TBufferFile* sbuf)
{
   if (sbuf==0) return dabc::Buffer();

   bool with_zip = true;

   const Int_t kMAXBUF = 0xffffff;
   Int_t noutot = 0;
   Int_t fObjlen = sbuf->Length();
   Int_t nbuffers = 1 + (fObjlen - 1)/kMAXBUF;
   Int_t buflen = TMath::Max(512,fObjlen + 9*nbuffers + 28);
   if (buflen<fObjlen) buflen = fObjlen;

   // TODO: in future one could acquire buffers from memory pool here
   dabc::Buffer buf = dabc::Buffer::CreateBuffer(sizeof(dabc::BinDataHeader)+buflen);

   if (buf.null()) {
      EOUT("Cannot request buffer of specified length");
      delete sbuf;
      return buf;
   }

   dabc::BinDataHeader* hdr = (dabc::BinDataHeader*) buf.SegmentPtr();
   hdr->reset();

   char* fBuffer = ((char*) buf.SegmentPtr()) + sizeof(dabc::BinDataHeader);

#if ROOT_VERSION_CODE <= ROOT_VERSION(5,34,7)
   with_zip = false;
#endif

   if (with_zip) {
      Int_t cxAlgorithm = 0;

      char *objbuf = sbuf->Buffer();
      char *bufcur = fBuffer; // start writing from beginning

      Int_t nzip   = 0;
      Int_t bufmax = 0;
      Int_t nout = 0;

      for (Int_t i = 0; i < nbuffers; ++i) {
         if (i == nbuffers - 1) bufmax = fObjlen - nzip;
         else               bufmax = kMAXBUF;
         R__zipMultipleAlgorithm(fCompression, &bufmax, objbuf, &bufmax, bufcur, &nout, cxAlgorithm);
         if (nout == 0 || nout >= fObjlen) { //this happens when the buffer cannot be compressed
            DOUT0("Fail to zip buffer");
            noutot = 0;
            with_zip = false;
            break;
         }
         bufcur += nout;
         noutot += nout;
         objbuf += kMAXBUF;
         nzip   += kMAXBUF;
      }
   }

   if (with_zip) {
      hdr->zipped = (uint32_t) fObjlen;
      hdr->payload = (uint32_t) noutot;
   } else {
      memcpy(fBuffer, sbuf->Buffer(), fObjlen);
      hdr->zipped = 0;
      hdr->payload = (uint32_t) fObjlen;
   }

   buf.SetTotalSize(sizeof(dabc::BinDataHeader) + hdr->payload);

   delete sbuf;

   return buf;
}



dabc::Buffer dabc_root::BinaryProducer::GetStreamerInfoBinary()
{
   CreateMemFile();

   TDirectory* olddir = gDirectory; gDirectory = 0;
   TFile* oldfile = gFile; gFile = 0;

   fMemFile->WriteStreamerInfo();
   TList* l = fMemFile->GetStreamerInfoList();
   //l->Print("*");

   fSinfoSize = l->GetSize();

   // TODO: one could reuse memory from dabc::MemoryPool here
   //       now keep as it is and copy data at least once
   TBufferFile* sbuf = new TBufferFile(TBuffer::kWrite, 100000);
   sbuf->SetParent(fMemFile);
   sbuf->MapObject(l);
   l->Streamer(*sbuf);

   delete l;

   gDirectory = olddir;
   gFile = oldfile;

   return CreateBindData(sbuf);
}

#ifdef DABC_ROOT_ASIMAGE

#include "TASImage.h"

dabc::Buffer dabc_root::BinaryProducer::CreateImage(TObject* obj)
{
   DOUT2("BinaryProducer::GetBinary process with image");

   if (!obj->InheritsFrom(TPad::Class())) return 0;

   TImage* img = TImage::Create();
   if (img==0) return 0;

   DOUT2("Crate IMAGE from canvas");
   img->FromPad((TPad*) obj);

   TASImage* tasImage = new TASImage();
   tasImage->Append(img);

   char* png_buffer(0);
   int size(0);

   tasImage->GetImageBuffer(&png_buffer, &size, TImage::kPng);

   dabc::Buffer buf;

   if ((png_buffer!=0) && (size>0)) {

      buf = dabc::Buffer::CreateBuffer(sizeof(dabc::BinDataHeader) + size);
      dabc::BinDataHeader* hdr = (dabc::BinDataHeader*) buf.SegmentPtr();
      hdr->reset();
      memcpy(((char*)buf.SegmentPtr()) + sizeof(dabc::BinDataHeader), png_buffer, size);
   }

   delete [] png_buffer;
   delete tasImage;
   // delete img;

   DOUT0("Create image in memory %d", size);

   return buf;
}

#else

dabc::Buffer dabc_root::BinaryProducer::CreateImage(TObject* obj)
{
   DOUT2("BinaryProducer::GetBinary process with image");

   if (!obj->InheritsFrom(TPad::Class())) return 0;

   TImage* img = TImage::Create();
   if (img==0) return 0;

   static int imgcnt = 0;
   std::string fname = dabc::format("/tmp/test%d.png", imgcnt++);

   DOUT2("Crate IMAGE from canvas");

   img->FromPad((TPad*) obj);

   DOUT0("Store IMAGE as %s file", fname.c_str());

   img->WriteImage(fname.c_str());
   delete img;

   std::ifstream is(fname.c_str());
   if (!is) return 0;
   is.seekg (0, is.end);
   long length = is.tellg();
   is.seekg (0, is.beg);

   dabc::Buffer buf = dabc::Buffer::CreateBuffer(sizeof(dabc::BinDataHeader) + length);

   dabc::BinDataHeader* hdr = (dabc::BinDataHeader*) buf.SegmentPtr();
   hdr->reset();

   is.read(((char*)buf.SegmentPtr()) + sizeof(dabc::BinDataHeader), length);

   is.close();

   gSystem->Unlink(fname.c_str());

   DOUT0("Build image in file %s length %ld", fname.c_str(), length);

   return buf;
}

#endif

std::string dabc_root::BinaryProducer::GetObjectHash(TObject* obj)
{
   if ((obj!=0) && (obj->InheritsFrom(TH1::Class())))
      return dabc::format("%g", ((TH1*)obj)->GetEntries());

   return "";
}


dabc::Buffer dabc_root::BinaryProducer::GetBinary(TObject* obj, bool asimage)
{
   if (asimage) return CreateImage(obj);

   CreateMemFile();

   TDirectory* olddir = gDirectory; gDirectory = 0;
   TFile* oldfile = gFile; gFile = 0;

   TList* l1 = fMemFile->GetStreamerInfoList();

   // TODO: one could reuse memory from dabc::MemoryPool here
   //       now keep as it is and copy data at least once
   TBufferFile* sbuf = new TBufferFile(TBuffer::kWrite, 100000);
   sbuf->SetParent(fMemFile);
   sbuf->MapObject(obj);
   obj->Streamer(*sbuf);

   bool believe_not_changed = false;

   if ((fMemFile->GetClassIndex()==0) || (fMemFile->GetClassIndex()->fArray[0] == 0)) {
      DOUT2("SEEMS to be, streamer info was not changed");
      believe_not_changed = true;
   }

   fMemFile->WriteStreamerInfo();
   TList* l2 = fMemFile->GetStreamerInfoList();

   DOUT2("STREAM LIST Before %d After %d", l1->GetSize(), l2->GetSize());

   if (believe_not_changed && (l1->GetSize() != l2->GetSize())) {
      EOUT("StreamerInfo changed when we were expecting no changes!!!!!!!!!");
      exit(444);
   }

   dabc::DateTime tm;
   tm.GetNow();
   char sbuf2[100];
   tm.AsString(sbuf2, sizeof(sbuf2), 3);

   if (believe_not_changed && (l1->GetSize() == l2->GetSize())) {
      DOUT2("+++ STREAMER INFO NOT CHANGED AS EXPECTED tm:%s  +++ ", sbuf2);
   }

   fSinfoSize = l2->GetSize();

   delete l1; delete l2;

   gDirectory = olddir;
   gFile = oldfile;

   return CreateBindData(sbuf);
}

std::string dabc_root::BinaryProducer::GetStreamerInfoHash() const
{
   return dabc::format("%d", fSinfoSize);
}
