#ifndef TFLUX_HPP
#define TFLUX_HPP 1

#include <deque>
#include <mutex>
#include <vector>

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <THttpServer.h>
#include <TStyle.h>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

#include "TPSD.hpp"
#include "TStdData.hpp"

class TFlux
{
 public:
  TFlux();
  TFlux(uint linkNumber);
  ~TFlux();

  void StartAcquisition() { fDigitizer->StartAcquisition(); };
  void StopAcquisition() { fDigitizer->StopAcquisition(); };

  void SetTimeInterval(int t) { fTimeInterval = t; };

  void ReadDigitizer();
  void ReadADC();
  void TimeCheck();

  void FillData();
  void FillADC();
  void StoreData();
  void StoreAndFill();

  void SWTrigger();

  void Terminate() { fAcqFlag = false; };

 private:
  TPSD *fDigitizer;

  std::deque<SampleData> fQueue;
  std::deque<uint> fADCQueue;
  std::mutex fMutex;

  mongocxx::instance *fMongoInstance;

  bool fAcqFlag;

  // For check the hit
  void UploadData(double currentTime, double rate);
  int fLastTime;
  int fLastHit;
  int fTimeInterval;

  // Something from ROOT
  void PlotAll();
  int fFillCounter;
  TH1D *fHisADC;
  TH1D *fHisTime;
  TH2D *fHisTimeADC;
  TGraph *fGrWave;
  TCanvas *fCanvas;
  THttpServer *fServer;

  void ReadTextPar();
  uint fChMask;
  uint fCheckCh;
  int fStartBin;
  int fStopBin;
};

#endif
