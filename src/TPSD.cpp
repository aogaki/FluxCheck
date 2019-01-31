#include <math.h>
#include <string.h>
#include <iostream>

#include "TPSD.hpp"
#include "TStdData.hpp"

template <class T>
void DelPointer(T *&pointer)
{
  delete pointer;
  pointer = nullptr;
}

TPSD::TPSD()
    : TDigitizer(),
      fpReadoutBuffer(nullptr),
      fppPSDEvents(nullptr),
      fpPSDWaveform(nullptr)
{
  SetParameters();
}

TPSD::TPSD(CAEN_DGTZ_ConnectionType type, int link, int node, uint32_t VMEadd)
    : TPSD()
{
  Open(type, link, node, VMEadd);
  Reset();
  GetBoardInfo();

  fTime.resize(fNChs);
  fTimeOffset.resize(fNChs);
  fPreviousTime.resize(fNChs);

  fDataArray = new unsigned char[fBLTEvents * ONE_HIT_SIZE * fNChs];
}

TPSD::~TPSD()
{
  Reset();
  FreeMemory();
  Close();
}

void TPSD::Initialize()
{
  CAEN_DGTZ_ErrorCode err;

  Reset();
  AcquisitionConfig();
  TriggerConfig();

  // err = CAEN_DGTZ_MallocReadoutBuffer(fHandler, &fpReadoutBuffer,
  //                                     &fMaxBufferSize);
  // PrintError(err, "MallocReadoutBuffer");

  // Buffer setting
  err = CAEN_DGTZ_SetNumEventsPerAggregate(fHandler, fBLTEvents);
  PrintError(err, "SetNumEventsPerAggregate");
  // 0 means automatically set
  err = CAEN_DGTZ_SetDPPEventAggregation(fHandler, 0, 0);
  PrintError(err, "SetDPPEventAggregation");

  // Synchronization Mode
  err = CAEN_DGTZ_SetRunSynchronizationMode(fHandler,
                                            CAEN_DGTZ_RUN_SYNC_Disabled);
  PrintError(err, "SetRunSynchronizationMode");

  if (fFirmware == FirmWareCode::DPP_PSD) {
    uint32_t mask = 0xFF;
    err = CAEN_DGTZ_SetDPPParameters(fHandler, mask, &fParPSD);
    PrintError(err, "SetDPPParameters");
  }
  // Following for loop is copied from sample.  Shame on me!!!!!!!
  for (uint32_t i = 0; i < fNChs; i++) {
    // Set the number of samples for each waveform (you can set different RL
    // for different channels)
    err = CAEN_DGTZ_SetRecordLength(fHandler, fRecordLength, i);

    // Set a DC offset to the input signal to adapt it to digitizer's dynamic
    // range
    // err = CAEN_DGTZ_SetChannelDCOffset(fHandler, i, (1 << fNBits) / 2);
    auto fac = (1. - fDCOffset);
    if (fac <= 0. || fac >= 1.) fac = 0.5;
    uint32_t offset = 0xFFFF * fac;
    // offset = 0x8000;
    err = CAEN_DGTZ_SetChannelDCOffset(fHandler, i, offset);

    // Set the Pre-Trigger size (in samples)
    err = CAEN_DGTZ_SetDPPPreTriggerSize(fHandler, i, 80);

    // Set the polarity for the given channel (CAEN_DGTZ_PulsePolarityPositive
    // or CAEN_DGTZ_PulsePolarityNegative)
    err = CAEN_DGTZ_SetChannelPulsePolarity(fHandler, i,
                                            CAEN_DGTZ_PulsePolarityNegative);
  }

  AllocateMemory();

  BoardCalibration();

  // Set register to use extended 47 bit time stamp
  for (uint32_t i = 0; i < fNChs; i++)
    RegisterSetBits(0x1084 + (i << 8), 8, 10, 0);
}

void TPSD::ReadEvents()
{
  fNEvents = 0;  // Event counter

  CAEN_DGTZ_ErrorCode err;

  uint32_t bufferSize;
  err = CAEN_DGTZ_ReadData(fHandler, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                           fpReadoutBuffer, &bufferSize);
  PrintError(err, "ReadData");
  if (bufferSize == 0) return;  // in the case of 0, GetDPPEvents makes crush

  uint32_t nEvents[fNChs];
  err = CAEN_DGTZ_GetDPPEvents(fHandler, fpReadoutBuffer, bufferSize,
                               (void **)fppPSDEvents, nEvents);
  PrintError(err, "GetDPPEvents");

  // for (int i = 0; i < nEvents[0]; i++)
  //   std::cout << (fppPSDEvents[0][i]).ChargeLong << std::endl;

  for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
    for (uint32_t iEve = 0; iEve < nEvents[iCh]; iEve++) {
      err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler, &fppPSDEvents[iCh][iEve],
                                         fpPSDWaveform);
      PrintError(err, "DecodeDPPWaveforms");

      auto tdc =
          fppPSDEvents[iCh][iEve].TimeTag +
          ((uint64_t)((fppPSDEvents[iCh][iEve].Extras >> 16) & 0xFFFF) << 31);
      if (fTSample > 0) tdc *= fTSample;
      fTime[iCh] = tdc;

      auto index = fNEvents * ONE_HIT_SIZE;
      fDataArray[index++] = fModNumber;  // fModNumber is needed.
      fDataArray[index++] = iCh;         // int to char.  Dangerous

      constexpr auto timeSize = sizeof(fTime[0]);
      memcpy(&fDataArray[index], &fTime[iCh], timeSize);
      index += timeSize;

      constexpr auto adcSize = sizeof(fppPSDEvents[0][0].ChargeLong);
      // auto adc = sumCharge;
      memcpy(&fDataArray[index], &fppPSDEvents[iCh][iEve].ChargeLong, adcSize);
      index += adcSize;

      // std::cout << fppPSDEvents[iCh][iEve].ChargeLong << std::endl;

      constexpr auto waveSize = sizeof(fpPSDWaveform->Trace1[0]) * kNSamples;
      memcpy(&fDataArray[index], fpPSDWaveform->Trace1, waveSize);

      fNEvents++;
    }
  }
}

void TPSD::SetParameters()
{
  fRecordLength = kNSamples;
  fVpp = 2.0;
  // fTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
  fTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
  fPostTriggerSize = 80;
  fBLTEvents = 1023;  // It is max, why not 1024?
  fDCOffset = 0.8;

  void SetPSDPar();
}

void TPSD::SetPSDPar()
{  // Copy from sample
  for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
    fParPSD.thr[iCh] = 100;  // Trigger Threshold
    /* The following parameter is used to specifiy the number of samples for the
    baseline averaging: 0 -> absolute Bl 1 -> 4samp 2 -> 8samp 3 -> 16samp 4 ->
    32samp 5 -> 64samp 6 -> 128samp */
    fParPSD.nsbl[iCh] = 2;
    fParPSD.lgate[iCh] = 128;  // Long Gate Width (N*4ns)
    fParPSD.sgate[iCh] = 0;    // Short Gate Width (N*4ns)
    fParPSD.pgate[iCh] = 16;   // Pre Gate Width (N*4ns)
    /* Self Trigger Mode:
    0 -> Disabled
    1 -> Enabled */
    fParPSD.selft[iCh] = 1;
    // Trigger configuration:
    // CAEN_DGTZ_DPP_TriggerConfig_Peak       -> trigger on peak. NOTE: Only for
    // FW <= 13X.5 CAEN_DGTZ_DPP_TriggerConfig_Threshold  -> trigger on
    // threshold */
    // fParPSD.trgc[iCh] = CAEN_DGTZ_DPP_TriggerConfig_Threshold;
    fParPSD.trgc[iCh] = CAEN_DGTZ_DPP_TriggerConfig_Peak;
    /* Trigger Validation Acquisition Window */
    fParPSD.tvaw[iCh] = 50;
    /* Charge sensibility: 0->40fc/LSB; 1->160fc/LSB; 2->640fc/LSB; 3->2,5pc/LSB
     */
    fParPSD.csens[iCh] = 0;
  }
  /* Pile-Up rejection Mode
  CAEN_DGTZ_DPP_PSD_PUR_DetectOnly -> Only Detect Pile-Up
  CAEN_DGTZ_DPP_PSD_PUR_Enabled -> Reject Pile-Up */
  fParPSD.purh = CAEN_DGTZ_DPP_PSD_PUR_DetectOnly;
  fParPSD.purgap = 100;  // Purity Gap
  fParPSD.blthr = 3;     // Baseline Threshold
  fParPSD.bltmo = 100;   // Baseline Timeout
  fParPSD.trgho = 8;     // Trigger HoldOff
}

void TPSD::AcquisitionConfig()
{
  CAEN_DGTZ_ErrorCode err;

  // Eanble all channels
  uint32_t mask = ((1 << fNChs) - 1);
  err = CAEN_DGTZ_SetChannelEnableMask(fHandler, mask);
  PrintError(err, "SetChannelEnableMask");

  // Set the acquisition mode
  err = CAEN_DGTZ_SetAcquisitionMode(fHandler, CAEN_DGTZ_SW_CONTROLLED);
  PrintError(err, "SetAcquisitionMode");

  // Set record length (length of waveform?);
  err = CAEN_DGTZ_SetRecordLength(fHandler, fRecordLength);
  PrintError(err, "SetRecordLength");

  // Mix means waveform list and energy
  err = CAEN_DGTZ_SetDPPAcquisitionMode(
      fHandler, CAEN_DGTZ_DPP_AcqMode_t::CAEN_DGTZ_DPP_ACQ_MODE_Mixed,
      CAEN_DGTZ_DPP_SaveParam_t::CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime);
  PrintError(err, "SetDPPAcquisitionMode");
}

void TPSD::TriggerConfig()
{
  CAEN_DGTZ_ErrorCode err;

  // Set the trigger threshold
  // The unit of its are V
  int32_t th = (1 << fNBits) * (fVth / fVpp);
  auto offset = (1 << fNBits) * fDCOffset;
  auto thVal = th + offset;
  std::cout << "Vth:\t" << thVal << "\t" << fVth << std::endl;
  for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
    fParPSD.thr[iCh] = thVal;
  }

  // Set the triggermode
  uint32_t mask = ((1 << fNChs) - 1);
  err = CAEN_DGTZ_SetChannelSelfTrigger(fHandler, fTriggerMode, mask);
  PrintError(err, "SetChannelSelfTrigger");

  err = CAEN_DGTZ_SetSWTriggerMode(fHandler, fTriggerMode);
  PrintError(err, "SetSWTriggerMode");

  uint32_t samples = fRecordLength * 0.2;
  for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
    err = CAEN_DGTZ_SetDPPPreTriggerSize(fHandler, iCh, samples);
    PrintError(err, "SetDPPPreTriggerSize");
  }

  CAEN_DGTZ_PulsePolarity_t pol = CAEN_DGTZ_PulsePolarityNegative;
  for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
    err = CAEN_DGTZ_SetChannelPulsePolarity(fHandler, iCh, pol);
    PrintError(err, "CAEN_DGTZ_SetChannelPulsePolarity");
  }

  if (fFirmware == FirmWareCode::DPP_PSD) {
    err = CAEN_DGTZ_SetDPPTriggerMode(
        fHandler,
        CAEN_DGTZ_DPP_TriggerMode_t::CAEN_DGTZ_DPP_TriggerMode_Normal);
    PrintError(err, "SetDPPTriggerMode");
  }
  // // Set post trigger size
  // err = CAEN_DGTZ_SetPostTriggerSize(fHandler, fPostTriggerSize);
  // PrintError(err, "SetPostTriggerSize");

  // // Set the triiger polarity
  // for (uint32_t iCh = 0; iCh < fNChs; iCh++)
  //   CAEN_DGTZ_SetTriggerPolarity(fHandler, iCh, fPolarity);
  //
  // CAEN_DGTZ_TriggerPolarity_t pol;
  // CAEN_DGTZ_GetTriggerPolarity(fHandler, 0, &pol);
  // std::cout << "Polarity:\t" << pol << std::endl;
}

void TPSD::AllocateMemory()
{
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  err = CAEN_DGTZ_MallocReadoutBuffer(fHandler, &fpReadoutBuffer, &size);
  PrintError(err, "MallocReadoutBuffer");

  // CAEN_DGTZ_DPP_PSD_Event_t *events[fNChs];
  fppPSDEvents = new CAEN_DGTZ_DPP_PSD_Event_t *[fNChs];
  err = CAEN_DGTZ_MallocDPPEvents(fHandler, (void **)fppPSDEvents, &size);
  PrintError(err, "MallocDPPEvents");

  err = CAEN_DGTZ_MallocDPPWaveforms(fHandler, (void **)&fpPSDWaveform, &size);
  PrintError(err, "MallocDPPWaveforms");
}

void TPSD::FreeMemory()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_FreeReadoutBuffer(&fpReadoutBuffer);
  PrintError(err, "FreeReadoutBuffer");
  // DelPointer(fpReadoutBuffer);

  err = CAEN_DGTZ_FreeDPPEvents(fHandler, (void **)fppPSDEvents);
  PrintError(err, "FreeDPPEvents");
  // DelPointer(fppPSDEvents);

  err = CAEN_DGTZ_FreeDPPWaveforms(fHandler, fpPSDWaveform);
  PrintError(err, "FreeDPPWaveforms");
  // DelPointer(fpPSDWaveform);
}

CAEN_DGTZ_ErrorCode TPSD::StartAcquisition()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_SWStartAcquisition(fHandler);
  PrintError(err, "StartAcquisition");

  for (auto &t : fTime) t = 0;
  for (auto &t : fTimeOffset) t = 0;
  for (auto &t : fPreviousTime) t = 0;

  return err;
}

void TPSD::StopAcquisition()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_SWStopAcquisition(fHandler);
  PrintError(err, "StopAcquisition");

  // err = CAEN_DGTZ_FreeEvent(fHandler, (void **)&fpEventStd);
  // PrintError(err, "FreeEvent");
}

void TPSD::SetMaster()
{  // Synchronization Mode
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_SetRunSynchronizationMode(
      fHandler, CAEN_DGTZ_RUN_SYNC_TrgOutTrgInDaisyChain);
  PrintError(err, "SetRunSynchronizationMode");

  err = CAEN_DGTZ_SetAcquisitionMode(fHandler, CAEN_DGTZ_SW_CONTROLLED);
  PrintError(err, "SetAcquisitionMode");

  uint32_t mask = ((1 << fNChs) - 1);
  err = CAEN_DGTZ_SetChannelSelfTrigger(fHandler,
                                        CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT, mask);
  PrintError(err, "SetChannelSelfTrigger");
}

void TPSD::SetSlave()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_SetRunSynchronizationMode(
      fHandler, CAEN_DGTZ_RUN_SYNC_TrgOutTrgInDaisyChain);
  PrintError(err, "SetRunSynchronizationMode");

  err = CAEN_DGTZ_SetAcquisitionMode(fHandler, CAEN_DGTZ_FIRST_TRG_CONTROLLED);
  PrintError(err, "SetAcquisitionMode");

  uint32_t mask = ((1 << fNChs) - 1);
  err = CAEN_DGTZ_SetChannelSelfTrigger(fHandler,
                                        CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT, mask);
  PrintError(err, "SetChannelSelfTrigger");

  err = CAEN_DGTZ_SetExtTriggerInputMode(fHandler,
                                         CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT);
  PrintError(err, "SetExtTriggerInputMode");
}

void TPSD::StartSyncMode(uint32_t nMods)
{
  // copy from digiTes
  // CAEN_DGTZ_ErrorCode err;
  int err{0};
  uint32_t d32;
  constexpr uint32_t RUN_START_ON_TRGIN_RISING_EDGE = 0xE;
  err |= CAEN_DGTZ_ReadRegister(fHandler, CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
  err |= CAEN_DGTZ_WriteRegister(
      fHandler, CAEN_DGTZ_ACQ_CONTROL_ADD,
      (d32 & 0xFFFFFFF0) | RUN_START_ON_TRGIN_RISING_EDGE);  // Arm acquisition
                                                             // (Run will start
                                                             // with 1st
                                                             // trigger)
  // Run Delay to deskew the start of acquisition
  if (fModNumber == 0)
    err |= CAEN_DGTZ_WriteRegister(fHandler, 0x8170, (nMods - 1) * 3 + 1);
  else
    err |=
        CAEN_DGTZ_WriteRegister(fHandler, 0x8170, (nMods - fModNumber - 1) * 3);

  // StartMode 1: use the TRGIN-TRGOUT daisy chain; the 1st trigger starts the
  // acquisition
  uint32_t mask = (fModNumber == 0) ? 0x80000000 : 0x40000000;
  err |=
      CAEN_DGTZ_WriteRegister(fHandler, CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD, mask);
  err |= CAEN_DGTZ_WriteRegister(fHandler, CAEN_DGTZ_FP_TRIGGER_OUT_ENABLE_ADD,
                                 mask);

  PrintError(CAEN_DGTZ_ErrorCode(err), "StartSyncMode");
}
