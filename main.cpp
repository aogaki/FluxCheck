#include <fcntl.h>
#include <termios.h>
#include <iostream>
#include <mutex>
#include <thread>

#include <TApplication.h>
#include <TSystem.h>

#include "TFlux.hpp"

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

std::mutex mutexLocker;

int main(int argc, char **argv)
{
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

  auto checker = new TFlux(linkNumber);
  checker->SetTimeInterval(timeInterval);
  checker->StartAcquisition();

  // auto server = new THttpServer("http:8888");

  std::thread readDigitizer(&TFlux::ReadDigitizer, checker);
  // std::thread fillData(&TFlux::FillData, checker);
  std::thread fillData(&TFlux::StoreAndFill, checker);
  // std::thread readDigitizer(&TFlux::ReadADC, checker);
  // std::thread fillData(&TFlux::FillADC, checker);
  std::thread timeCheck(&TFlux::TimeCheck, checker);
  // std::thread trigger(&TFlux::SWTrigger, checker);

  while (true) {
    mutexLocker.lock();
    gSystem->ProcessEvents();  // This should be called at main thread
    mutexLocker.unlock();

    if (kbhit()) {
      checker->Terminate();
      readDigitizer.join();
      fillData.join();
      timeCheck.join();
      // trigger.join();
      break;
    }

    usleep(1000);
  }

  std::cout << "Stop!" << std::endl;
  gSystem->ProcessEvents();
  checker->StopAcquisition();

  std::cout << "Delete" << std::endl;
  // delete server;
  delete checker;
  return 0;
}
