#ifndef TFLUX_HPP
#define TFLUX_HPP 1

#include <deque>
#include <vector>

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <THttpServer.h>
#include <TStyle.h>
#include <TSystem.h>

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
  void TimeCheck();

  void FillData();

 private:
  TPSD *fDigitizer;
  std::deque<SampleData> fQueue;

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
