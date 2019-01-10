#ifndef TDigitizer_hpp
#define TDigitizer_hpp 1

// For the standard fiemware digitizer
// This will be super class of other firmware and model family

#include <string>
#include <vector>

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

enum class FirmWareCode {
  // I need only theese
  // Its are not same as CAENDigitizerType.h
  DPP_PSD,
  DPP_PHA,
  DPP_CI,
  STD,
};

class TDigitizer
{
 public:
  TDigitizer();
  virtual ~TDigitizer();

  void SendSWTrigger();
  void BoardCalibration();

  virtual void Initialize() = 0;

  virtual void ReadEvents() = 0;

  // When the FW is wxpired.  Configuration can be done.
  // But, StartAcquisition can not be done.
  virtual CAEN_DGTZ_ErrorCode StartAcquisition() = 0;
  virtual void StopAcquisition() = 0;

  virtual uint32_t GetNEvents() = 0;
  unsigned char *GetDataArray() { return fDataArray; };

  void SetModNumber(unsigned char n) { fModNumber = n; };

 protected:
  int fHandler;
  unsigned char fModNumber;
  void Open(CAEN_DGTZ_ConnectionType type, int link, int node, uint32_t VMEadd);
  void Close();
  void Reset();

  // For board information
  void GetBoardInfo();
  int fDigitizerModel;
  FirmWareCode fFirmware;
  uint32_t fNChs;
  int fTSample;  // This means time step length in ns
  int fNBits;    // ADC, Waveform resolution

  void PrintError(const CAEN_DGTZ_ErrorCode &err, const std::string &funcName);

  unsigned char *fDataArray;

  int RegisterSetBits(uint16_t addr, int start_bit, int end_bit, int val);

 private:
  // Those SPI register functions are copy from digiTES
  CAEN_DGTZ_ErrorCode ReadSPIRegister(uint32_t ch, uint32_t address,
                                      uint32_t &data);

  CAEN_DGTZ_ErrorCode WriteSPIRegister(uint32_t ch, uint32_t address,
                                       uint32_t data);
};

#endif
