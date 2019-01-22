#ifndef TStdData_hpp
#define TStdData_hpp 1

#include <CAENDigitizerType.h>

// constexpr uint32_t kNSamples = 256;
// constexpr uint32_t kNSamples = 1024 + 512;
constexpr uint32_t kNSamples = 1024;

class TStdData
{
 public:
  TStdData()
  {
    ModNumber = 0;
    ChNumber = 0;
    TimeStamp = 0;
    ADC = 0;
    for (uint32_t i = 0; i < kNSamples; i++) Waveform[i] = 0;
  };

  ~TStdData(){};

  uint8_t ModNumber;
  uint8_t ChNumber;
  uint64_t TimeStamp;
  int16_t ADC;
  uint16_t Waveform[kNSamples];
};

struct SampleData {
  unsigned char ModNumber;
  unsigned char ChNumber;
  unsigned long TimeStamp;
  double ADC;
  unsigned short Waveform[kNSamples];
};

constexpr int ONE_HIT_SIZE = sizeof(unsigned char) + sizeof(unsigned char) +
                             sizeof(unsigned long) + sizeof(short) +
                             (sizeof(unsigned short) * kNSamples);

constexpr int kMaxPacketSize = 2000000 - ONE_HIT_SIZE;

// For data transfer buffer
// constexpr int BUFFER_SIZE = 1024 * 1024 * 2 * 16;
constexpr int BUFFER_SIZE = 1024 * 2 * 16;

#endif
