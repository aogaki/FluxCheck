#include <fstream>
#include <iostream>

#include "TFlux.hpp"

TFlux::TFlux()
    : fAcqFlag(true),
      fLastTime(0),
      fLastHit(0),
      fTimeInterval(60),
      fFillCounter(0)
{
  fDigitizer = nullptr;

  fMongoInstance = new mongocxx::instance;

  ReadTextPar();

  // Something from ROOT
  gStyle->SetOptFit(1111);
  fHisADC = new TH1D("hisADC", "Flux check", 33000, 0, 33000);

  fCanvas = new TCanvas("flux", "Flux check", 1600, 600);
  // fCanvas->Divide(2, 1);

  fGrWave = new TGraph();
  for (uint iSample = 0; iSample < kNSamples; iSample++)
    fGrWave->SetPoint(iSample, iSample * 2, 8000);  // one sample 2 ns
  fGrWave->SetMaximum(20000);
  fGrWave->SetMinimum(0);

  auto fServer = new THttpServer("http:8080?monitoring=5000;rw;noglobal");
  fServer->Register("/", fHisADC);

  PlotAll();
}

TFlux::TFlux(uint linkNumber) : TFlux()
{
  // Digitizer
  fDigitizer = new TPSD(CAEN_DGTZ_USB, linkNumber);
  std::cout << "What is this?" << fChMask << std::endl;
  fDigitizer->SetChMask(fChMask);
  fDigitizer->Initialize();
}

TFlux::~TFlux()
{
  delete fDigitizer;
  delete fMongoInstance;

  fServer->Unregister(fHisADC);
  delete fServer;
  delete fCanvas;
  delete fHisADC;
  delete fGrWave;
}

void TFlux::ReadDigitizer()
{
  fDigitizer->ReadEvents();

  auto dataArray = fDigitizer->GetDataArray();
  const int nHit = fDigitizer->GetNEvents();
  // std::cout << nHit << std::endl;
  for (int i = 0; i < nHit; i++) {
    auto index = (i * ONE_HIT_SIZE);
    auto offset = 0;
    SampleData data;

    data.ModNumber = dataArray[index + offset];
    offset += sizeof(data.ModNumber);

    data.ChNumber = dataArray[index + offset];
    offset += sizeof(data.ChNumber);
    if (data.ChNumber != fCheckCh) continue;

    memcpy(&data.TimeStamp, &dataArray[index + offset], sizeof(data.TimeStamp));
    offset += sizeof(data.TimeStamp);

    memcpy(&data.ADC, &dataArray[index + offset], sizeof(data.ADC));
    offset += sizeof(data.ADC);

    for (uint iSample = 0; iSample < kNSamples; iSample++) {
      // Do the memcpy onece, Stupid
      constexpr auto dataSize = sizeof(data.Waveform[0]);
      memcpy(&data.Waveform[iSample], &dataArray[index + offset], dataSize);
      offset += dataSize;
    }

    fQueue.push_back(data);
  }
}

void TFlux::FillData()
{
  while (!fQueue.empty()) {
    auto data = fQueue.front();
    fHisADC->Fill(data.ADC);

    for (uint iSample = 0; iSample < kNSamples; iSample++) {
      fGrWave->SetPoint(iSample, iSample * 2, data.Waveform[iSample]);
    }

    fQueue.pop_front();
  }

  if ((++fFillCounter % 1000) == 0) PlotAll();
}

void TFlux::TimeCheck()
{
  if (fLastTime == 0) fLastTime = time(0);
  auto currentTime = time(0);
  if ((currentTime - fLastTime) >= fTimeInterval) {
    auto hitCounter = fHisADC->Integral(fStartBin, fStopBin) - fLastHit;
    auto hitRate = hitCounter / (currentTime - fLastTime);
    std::cout << currentTime << "\t" << hitRate << " Hz\t" << hitCounter
              << " hit" << std::endl;
    // UploadData(currentTime, hitRate);
    UploadData(currentTime, hitCounter);
    fLastHit += hitCounter;
    fLastTime = currentTime;
  }
}

void TFlux::ReadTextPar()
{
  std::ifstream fin("par.dat");
  std::string key, val;
  uint fChMask = 0b00000001;
  uint fCheckCh = 0;
  while (fin >> key >> val) {
    if (key == "Start")
      fStartBin = std::stoi(val);
    else if (key == "Stop")
      fStopBin = std::stoi(val);
    else if (key == "Channel") {
      fCheckCh = std::stoi(val);
      fChMask = 0b1 << fCheckCh;
    }
  }
  fin.close();
  std::cout << "mask: " << fChMask << std::endl;
}

void TFlux::PlotAll()
{
  // Do I need many Update() ?
  fCanvas->cd(1);
  fHisADC->Draw();
  fCanvas->Update();
  fCanvas->cd(2);
  fGrWave->Draw("AL");
  fCanvas->Update();
  fCanvas->cd();
  fCanvas->Update();
  gSystem->ProcessEvents();
}

void TFlux::UploadData(double currentTime, double rate)
{
  // mongocxx::client conn{mongocxx::uri{}};
  mongocxx::client conn{mongocxx::uri{"mongodb://192.168.161.73/"}};

  auto collection = conn["node-angular"]["posts"];
  bsoncxx::builder::stream::document buf{};

  buf << "title"
      << "count"
      << "content" << currentTime << "imagePath" << rate;
  collection.insert_one(buf.view());
  buf.clear();
}
