#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <fstream>
#include <iostream>

#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>
#include <THttpServer.h>
#include <TStyle.h>
#include <TSystem.h>

// #include "TDPP.hpp"
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
  // We can make instance only onece
  // Making it at main function is enough
  mongocxx::instance inst{};

  auto timeInterval = 60;
  auto linkNumber = 0;
  for (auto i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "-i") {
      timeInterval = atof(argv[++i]);
    }
    if (std::string(argv[i]) == "-l") {
      linkNumber = atof(argv[++i]);
    }
  }

  TApplication app("testApp", &argc, argv);

  std::ifstream fin("par.dat");
  std::string key, val;
  int startBin, stopBin;
  while (fin >> key >> val) {
    if (key == "Start")
      startBin = std::stoi(val);
    else if (key == "Stop")
      stopBin = std::stoi(val);
  }
  fin.close();

  auto digi = new TPSD(CAEN_DGTZ_USB, linkNumber);
  // auto digi = new TDPP(CAEN_DGTZ_USB, linkNumber);
  digi->Initialize();
  digi->StartAcquisition();

  TH1D *hisCharge = new TH1D("hisCharge", "test", 20000, 0, 20000);
  TCanvas *canvas = new TCanvas();
  TGraph *grWave = new TGraph();
  grWave->SetMaximum(20000);
  grWave->SetMinimum(0);
  TCanvas *canvas2 = new TCanvas();
  canvas->cd();
  hisCharge->Draw();

  gStyle->SetOptFit(1111);
  auto server = new THttpServer("http:8080?monitoring=5000;rw;noglobal");
  server->Register("", hisCharge);

  gSystem->ProcessEvents();

  auto lastTime = time(0);
  auto lastHit = 0;
  for (int loopCounter = 0; true; loopCounter++) {
    //   // if (i > 10) break;
    // std::cout << i << std::endl;

    // for (int j = 0; j < 10; j++) digi->SendSWTrigger();
    digi->ReadEvents();

    auto dataArray = digi->GetDataArray();
    const int nHit = digi->GetNEvents();
    // std::cout << nHit << std::endl;
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
      // std::cout << data.TimeStamp << std::endl;

      memcpy(&data.ADC, &dataArray[index + offset], sizeof(data.ADC));
      offset += sizeof(data.ADC);
      if (data.ChNumber == 0) {
        hisCharge->Fill(data.ADC);

        for (uint iSample = 0; iSample < kNSamples; iSample++) {
          unsigned short pulse;
          memcpy(&pulse, &dataArray[index + offset], sizeof(pulse));
          offset += sizeof(pulse);

          grWave->SetPoint(iSample, iSample * 2, pulse);  // one sample 2 ns
        }
      }
    }

    auto currentTime = time(0);
    if ((currentTime - lastTime) >= timeInterval) {
      auto hitCounter = hisCharge->Integral(startBin, stopBin) - lastHit;
      auto hitRate = hitCounter / (currentTime - lastTime);
      std::cout << currentTime << "\t" << hitRate << " Hz\t" << hitCounter
                << " hit" << std::endl;
      // UploadData(currentTime, hitRate);
      UploadData(currentTime, hitCounter);
      lastHit += hitCounter;
      lastTime = currentTime;
    }

    if ((loopCounter % 1000) == 0) {
      canvas2->cd();
      grWave->Draw("AL");
      canvas2->Update();

      canvas->cd();
      hisCharge->Draw();
      canvas->Update();

      gSystem->ProcessEvents();
    }

    if (kbhit()) break;

    usleep(1000);
  }

  digi->StopAcquisition();

  // app.Run();
  delete digi;

  delete server;

  return 0;
}
