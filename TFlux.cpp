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
  fCanvas->Divide(2, 1);
  fCanvas->cd(2)->SetGrid(kTRUE, kTRUE);
  fCanvas->cd();

  fGrWave = new TGraph();
  for (uint iSample = 0; iSample < kNSamples; iSample++)
    fGrWave->SetPoint(iSample, iSample * 2, 8000);  // one sample 2 ns
  fGrWave->SetMaximum(20000);
  fGrWave->SetMinimum(0);

  // auto fServer = new THttpServer("http:8888?monitoring=5000;rw;noglobal");
  fServer = new THttpServer("http:8888");
  // fServer->Register("/", fHisADC);

  PlotAll();
}

TFlux::TFlux(uint linkNumber) : TFlux()
{
  // Digitizer
  fDigitizer = new TPSD(CAEN_DGTZ_USB, linkNumber);
  fDigitizer->SetChMask(fChMask);
  fDigitizer->Initialize();
}

TFlux::~TFlux()
{
  delete fDigitizer;
  delete fMongoInstance;

  // fServer->Unregister(fHisADC);
  // fServer->SetTerminate();
  delete fServer;
  delete fCanvas;
  delete fHisADC;
  delete fGrWave;
}

void TFlux::ReadDigitizer()
{
  while (fAcqFlag) {
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

      memcpy(&data.TimeStamp, &dataArray[index + offset],
             sizeof(data.TimeStamp));
      offset += sizeof(data.TimeStamp);

      memcpy(&data.ADC, &dataArray[index + offset], sizeof(data.ADC));
      offset += sizeof(data.ADC);

      constexpr auto dataSize = sizeof(data.Waveform[0]) * kNSamples;
      memcpy(&data.Waveform[0], &dataArray[index + offset], dataSize);
      offset += dataSize;

      fMutex.lock();
      fQueue.push_back(data);
      fMutex.unlock();
    }
    usleep(1000);
  }
}

void TFlux::FillData()
{
  while (fAcqFlag) {
    while (!fQueue.empty()) {
      auto data = fQueue.front();
      fHisADC->Fill(data.ADC);

      for (uint iSample = 0; iSample < kNSamples; iSample++) {
        fGrWave->SetPoint(iSample, iSample * 2, data.Waveform[iSample]);
      }

      fMutex.lock();
      fQueue.pop_front();
      fMutex.unlock();
    }

    if ((++fFillCounter % 1000) == 0) PlotAll();

    usleep(1000);
  }
}

void TFlux::TimeCheck()
{
  while (fAcqFlag) {
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
    usleep(1000);
  }
}

void TFlux::ReadTextPar()
{
  std::ifstream fin("par.dat");
  std::string key, val;
  fChMask = 0b00000001;
  fCheckCh = 0;
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
}

void TFlux::PlotAll()
{
  fCanvas->cd(1);
  fHisADC->Draw();
  fCanvas->cd(2);
  fGrWave->Draw("AL");
  fCanvas->cd();
  fCanvas->Update();
}

void TFlux::UploadData(double currentTime, double rate)
{
  // mongocxx::client conn{mongocxx::uri{}};
  mongocxx::client conn{mongocxx::uri{"mongodb://192.168.161.73/"}};

  auto collection = conn["node-angular"]["posts"];
  bsoncxx::builder::stream::document buf{};

  buf << "title"
      << "count"
      << "content" << std::to_string(currentTime) << "imagePath"
      << std::to_string(rate);
  collection.insert_one(buf.view());
  buf.clear();
}
