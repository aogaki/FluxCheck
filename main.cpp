#include <fcntl.h>
#include <termios.h>
#include <iostream>

#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>

// #include "TDPP.hpp"
#include "TPSD.hpp"
#include "TWaveRecord.hpp"

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

int main(int argc, char **argv)
{
  TApplication app("testApp", &argc, argv);

  int link = 0;
  auto digi = new TPSD(CAEN_DGTZ_USB, link);
  // auto digi = new TDPP(CAEN_DGTZ_USB, link);
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

  for (int i = 0; true; i++) {
    //   // if (i > 10) break;
    std::cout << i << std::endl;

    // for (int j = 0; j < 10; j++) digi->SendSWTrigger();
    digi->ReadEvents();

    auto dataArray = digi->GetDataArray();
    const int nHit = digi->GetNEvents();
    std::cout << nHit << std::endl;
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

    canvas2->cd();
    grWave->Draw("AL");
    canvas2->Update();

    canvas->cd();
    hisCharge->Draw();
    canvas->Update();

    if (kbhit()) break;

    usleep(1000);
  }

  digi->StopAcquisition();

  // app.Run();
  delete digi;

  return 0;
}
