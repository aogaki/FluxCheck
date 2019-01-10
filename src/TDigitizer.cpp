#include <iostream>

#include "TDigitizer.hpp"

TDigitizer::TDigitizer()
    : fHandler(-1),
      fModNumber(127),
      fDigitizerModel(0),
      fNChs(0),
      fTSample(0),
      fNBits(0)
{
}

TDigitizer::~TDigitizer(){};

void TDigitizer::PrintError(const CAEN_DGTZ_ErrorCode &err,
                            const std::string &funcName)
{
  if (err < 0) {  // 0 is success
    std::cout << "In " << funcName << ", error code = " << err << std::endl;
    // CAEN_DGTZ_CloseDigitizer(fHandler);
    // throw err;
  }
}

void TDigitizer::Open(CAEN_DGTZ_ConnectionType type, int link, int node,
                      uint32_t VMEadd)
{
  auto err = CAEN_DGTZ_OpenDigitizer(type, link, node, VMEadd, &fHandler);
  PrintError(err, "OpenDigitizer");
  fModNumber = (unsigned int)node;

  // if (node >= 0 && node < 256) fModNumber = (unsigned char)node;
  // else {think it!!}

  if (err != CAEN_DGTZ_Success) {
    std::cout << "Can not open the device!" << std::endl;
    exit(0);
  }
}

void TDigitizer::Close()
{
  auto err = CAEN_DGTZ_CloseDigitizer(fHandler);
  PrintError(err, "CloseDigitizer");
}

void TDigitizer::Reset()
{
  auto err = CAEN_DGTZ_Reset(fHandler);
  PrintError(err, "Reset");
}

void TDigitizer::SendSWTrigger()
{
  auto err = CAEN_DGTZ_SendSWtrigger(fHandler);
  PrintError(err, "SendSWTrigger");
}

void TDigitizer::GetBoardInfo()
{
  CAEN_DGTZ_BoardInfo_t info;
  auto err = CAEN_DGTZ_GetInfo(fHandler, &info);
  PrintError(err, "GetInfo");

  fNChs = info.Channels;

  std::cout << "Model name:\t" << info.ModelName << "\n"
            << "Model number:\t" << info.Model << "\n"
            << "No. channels:\t" << info.Channels << "\n"
            << "Format factor:\t" << info.FormFactor << "\n"
            << "Family code:\t" << info.FamilyCode << "\n"
            << "Firmware revision of the FPGA on the mother board (ROC):\t"
            << info.ROC_FirmwareRel << "\n"
            << "Firmware revision of the FPGA on the daughter board (AMC):\t"
            << info.AMC_FirmwareRel << "\n"
            << "Serial number:\t" << info.SerialNumber << "\n"
            << "PCB revision:\t" << info.PCB_Revision << "\n"
            << "No. bits of the ADC:\t" << info.ADC_NBits << "\n"
            << "Device handler of CAENComm:\t" << info.CommHandle << "\n"
            << "Device handler of CAENVME:\t" << info.VMEHandle << "\n"
            << "License number:\t" << info.License << std::endl;

  // Copy from digites
  if (info.FamilyCode == 5) {
    fDigitizerModel = 751;
    fTSample = 1;
    fNBits = 10;
  } else if (info.FamilyCode == 7) {
    fDigitizerModel = 780;
    fTSample = 10;
    fNBits = 14;
  } else if (info.FamilyCode == 13) {
    fDigitizerModel = 781;
    fTSample = 10;
    fNBits = 14;
  } else if (info.FamilyCode == 0) {
    fDigitizerModel = 724;
    fTSample = 10;
    fNBits = 14;
  } else if (info.FamilyCode == 11) {
    fDigitizerModel = 730;
    fTSample = 2;
    fNBits = 14;
  } else if (info.FamilyCode == 14) {
    fDigitizerModel = 725;
    fTSample = 4;
    fNBits = 14;
  } else if (info.FamilyCode == 3) {
    fDigitizerModel = 720;
    fTSample = 4;
    fNBits = 12;
  } else if (info.FamilyCode == 999) {  // temporary code for Hexagon
    fDigitizerModel = 5000;
    fTSample = 10;
    fNBits = 14;
  } else {
    PrintError(err, "Check Family code @ GetBoardInfo");
  }

  uint32_t majorNumber = atoi(info.AMC_FirmwareRel);
  if (fDigitizerModel == 5000) {        // Hexagon
    fFirmware = FirmWareCode::DPP_PHA;  // It will be never used at ELI?
  } else if (majorNumber == 128) {
    fFirmware = FirmWareCode::DPP_PHA;  // It will be never used at ELI?
  } else if (majorNumber == 130) {
    fFirmware = FirmWareCode::DPP_CI;
  } else if (majorNumber == 131) {
    fFirmware = FirmWareCode::DPP_PSD;
  } else if (majorNumber == 132) {
    fFirmware = FirmWareCode::DPP_PSD;
  } else if (majorNumber == 136) {
    fFirmware = FirmWareCode::DPP_PSD;  // NOTE: valid also for x725
  } else if (majorNumber == 139) {
    fFirmware = FirmWareCode::DPP_PHA;  // NOTE: valid also for x725
  } else {
    fFirmware = FirmWareCode::STD;
  }

  std::cout << "Time sample length:\t" << fTSample << " ns\n"
            << "ADC resolution:\t" << fNBits << " bits\n"
            << "Firmware code:\t" << int(fFirmware) << std::endl;
}

void TDigitizer::BoardCalibration()
{
  if ((fDigitizerModel == 730) || (fDigitizerModel == 725)) {
    // Copy from digiTes
    // Honestly, I do not know what is this.
    uint32_t lock, ctrl;
    int ret = 0;
    for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
      // enter engineering functions
      ret |= WriteSPIRegister(iCh, 0x7A, 0x59);
      ret |= WriteSPIRegister(iCh, 0x7A, 0x1A);
      ret |= WriteSPIRegister(iCh, 0x7A, 0x11);
      ret |= WriteSPIRegister(iCh, 0x7A, 0xAC);

      // read lock value
      ret |= ReadSPIRegister(iCh, 0xA7, lock);
      // write lock value
      ret |= WriteSPIRegister(iCh, 0xA5, lock);

      // enable lock
      ret |= ReadSPIRegister(iCh, 0xA4, ctrl);
      ctrl |= 0x4;  // set bit 2
      ret |= WriteSPIRegister(iCh, 0xA4, ctrl);

      ret |= ReadSPIRegister(iCh, 0xA4, ctrl);
      // return ret;
    }
  } else if (fDigitizerModel == 751) {
    auto err = CAEN_DGTZ_Calibrate(fHandler);
    PrintError(err, "Calibrate");
  }
}

CAEN_DGTZ_ErrorCode TDigitizer::WriteSPIRegister(uint32_t ch, uint32_t address,
                                                 uint32_t value)
{
  uint32_t SPIBusy = 1;
  int32_t ret = CAEN_DGTZ_Success;

  uint32_t SPIBusyAddr = 0x1088 + (ch << 8);
  uint32_t addressingRegAddr = 0x80B4;
  uint32_t valueRegAddr = 0x10B8 + (ch << 8);

  while (SPIBusy) {
    if ((ret = CAEN_DGTZ_ReadRegister(fHandler, SPIBusyAddr, &SPIBusy)) !=
        CAEN_DGTZ_Success)
      return CAEN_DGTZ_CommError;
    SPIBusy = (SPIBusy >> 2) & 0x1;
    if (!SPIBusy) {
      if ((ret = CAEN_DGTZ_WriteRegister(fHandler, addressingRegAddr,
                                         address)) != CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
      if ((ret = CAEN_DGTZ_WriteRegister(fHandler, valueRegAddr, value)) !=
          CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
    }
    usleep(1000);
  }
  return CAEN_DGTZ_Success;
}

CAEN_DGTZ_ErrorCode TDigitizer::ReadSPIRegister(uint32_t ch, uint32_t address,
                                                uint32_t &value)
{  // Copy from digiTES
  uint32_t SPIBusy = 1;
  int32_t ret = CAEN_DGTZ_Success;
  uint32_t SPIBusyAddr = 0x1088 + (ch << 8);
  uint32_t addressingRegAddr = 0x80B4;
  uint32_t valueRegAddr = 0x10B8 + (ch << 8);

  while (SPIBusy) {
    if ((ret = CAEN_DGTZ_ReadRegister(fHandler, SPIBusyAddr, &SPIBusy)) !=
        CAEN_DGTZ_Success)
      return CAEN_DGTZ_CommError;
    SPIBusy = (SPIBusy >> 2) & 0x1;
    if (!SPIBusy) {
      if ((ret = CAEN_DGTZ_WriteRegister(fHandler, addressingRegAddr,
                                         address)) != CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
      if ((ret = CAEN_DGTZ_ReadRegister(fHandler, valueRegAddr, &value)) !=
          CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
    }
    usleep(1000);
  }
  return CAEN_DGTZ_Success;
}

int TDigitizer::RegisterSetBits(uint16_t addr, int start_bit, int end_bit,
                                int val)
{  // copy from digiTes
  uint32_t mask = 0, reg;
  int ret;
  int i;

  if (((addr & 0xFF00) == 0x8000) && (addr != 0x8000) && (addr != 0x8004) &&
      (addr != 0x8008)) {  // broadcast access to channel individual registers
    // (loop over channels)
    uint16_t ch;
    for (ch = 0; ch < fNChs; ch++) {
      ret = CAEN_DGTZ_ReadRegister(fHandler, 0x1000 | (addr & 0xFF) | (ch << 8),
                                   &reg);
      for (i = start_bit; i <= end_bit; i++) mask |= 1 << i;
      reg = (reg & ~mask) | ((val << start_bit) & mask);
      ret |= CAEN_DGTZ_WriteRegister(fHandler,
                                     0x1000 | (addr & 0xFF) | (ch << 8), reg);
    }
  } else {  // access to channel individual register or mother board register
    ret = CAEN_DGTZ_ReadRegister(fHandler, addr, &reg);
    for (i = start_bit; i <= end_bit; i++) mask |= 1 << i;
    reg = (reg & ~mask) | ((val << start_bit) & mask);
    ret |= CAEN_DGTZ_WriteRegister(fHandler, addr, reg);
  }
  return ret;
}
