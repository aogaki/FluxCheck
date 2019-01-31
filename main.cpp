#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <iostream>
#include <random>
#include <string>

#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>

#include "TPSD.hpp"
#include "TWaveRecord.hpp"

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

int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if (ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

void UploadData(double currentTime, double rate)
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

int main(int argc, char **argv)
{
  std::random_device seed_gen;
  std::default_random_engine engine(seed_gen());
  std::poisson_distribution<> dist(100.);

  // We can make instance only onece
  // Making it at main function is enough
  mongocxx::instance inst{};

  auto SWtrigger = false;
  auto vTh = -0.1;
  auto DCOffset = 0.8;
  auto timeInterval = 60;
  // auto singleCh = true;

  for (auto i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "-s") {
      SWtrigger = true;
    } else if (std::string(argv[i]) == "-t") {
      vTh = atof(argv[++i]);
    } else if (std::string(argv[i]) == "-i") {
      timeInterval = atof(argv[++i]);
    } else if (std::string(argv[i]) == "-a") {
      // singleCh = false;
    }
  }

  TApplication app("testApp", &argc, argv);

  int link = 0;
  // auto digi = new TWaveRecord(CAEN_DGTZ_USB, link);
  auto digi = new TPSD(CAEN_DGTZ_USB, link);
  digi->SetThreshold(vTh);
  digi->SetDCOffset(DCOffset);
  // digi->SetSingleCh(singleCh);

  digi->Initialize();

  digi->StartAcquisition();

  TH1D *hisCharge = new TH1D("hisCharge", "test", 5000, -1000, 49000);
  TGraph *grWave = new TGraph();
  // grWave->SetMaximum(14000);
  // grWave->SetMinimum(12000);
  grWave->SetMaximum(20000);
  grWave->SetMinimum(0);
  for (uint iSample = 0; iSample < kNSamples; iSample++) {
    grWave->SetPoint(iSample, iSample * 2, 8000.);  // one sample 2 ns
  }

  TCanvas *canvas = new TCanvas();
  TCanvas *canvas2 = new TCanvas();
  canvas->cd();
  hisCharge->Draw();

  auto lastTime = time(0);
  auto hitCounter = 0;

  for (auto loopCounter = 0; true; loopCounter++) {
    digi->ReadEvents();

    const int nHit = digi->GetNEvents();

    auto dataArray = digi->GetDataArray();
    for (int i = 0; i < nHit; i++) {
      auto index = (i * ONE_HIT_SIZE);
      auto offset = 0;
      SampleData data;

      data.ModNumber = dataArray[index + offset];
      offset += sizeof(data.ModNumber);

      data.ChNumber = dataArray[index + offset];
      offset += sizeof(data.ChNumber);

      memcpy(&data.TimeStamp, &dataArray[index + offset],
             sizeof(data.TimeStamp));
      offset += sizeof(data.TimeStamp);

      memcpy(&data.ADC, &dataArray[index + offset], sizeof(data.ADC));
      offset += sizeof(data.ADC);

      unsigned short pulse[kNSamples];
      memcpy(pulse, &dataArray[index + offset], sizeof(pulse));
      offset += sizeof(pulse);

      if (data.ChNumber == 0) {
        hitCounter++;
        hisCharge->Fill(data.ADC);

        auto arrayY = grWave->GetY();
        // memcpy(arrayY, pulse, sizeof(pulse));
        for (uint iSample = 0; iSample < kNSamples; iSample++)
          arrayY[iSample] = pulse[iSample];
      }
    }

    auto currentTime = time(0);
    if ((currentTime - lastTime) >= timeInterval) {
      double hitRate = double(hitCounter) / (currentTime - lastTime);
      lastTime = currentTime;
      std::cout << currentTime << "\t" << hitRate << " Hz\t" << hitCounter
                << " hit" << std::endl;
      // UploadData(currentTime, hitRate);
      UploadData(currentTime, hitCounter);
      hitCounter = 0;
    }

    if ((loopCounter % 100) == 0) {
      canvas2->cd();
      grWave->Draw("AL");
      canvas2->Update();

      canvas->cd();
      hisCharge->Draw();
      canvas->Update();
    }

    if (kbhit()) {
      break;
    } else {
      if (SWtrigger) {
        auto hit = dist(engine);
        hit = 1;
        for (auto j = 0; j < hit; j++) digi->SendSWTrigger();
      }
      usleep(1000);
    }
  }

  digi->StopAcquisition();

  // app.Run();
  delete digi;

  // All ROOT classes are deleted by ROOT.

  return 0;
}
