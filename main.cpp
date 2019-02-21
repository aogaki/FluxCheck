#include <fcntl.h>
#include <termios.h>
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

  std::thread readDigitizer(&TFlux::ReadDigitizer, checker);
  std::thread fillData(&TFlux::FillData, checker);
  std::thread timeCheck(&TFlux::TimeCheck, checker);

  while (true) {
    gSystem->ProcessEvents();  // This should be called at main thread

    if (kbhit()) {
      checker->Terminate();
      readDigitizer.join();
      fillData.join();
      timeCheck.join();
      break;
    }

    usleep(1000);
  }

  checker->StopAcquisition();

  delete checker;
  return 0;
}
