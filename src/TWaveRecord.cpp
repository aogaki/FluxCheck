#include <string.h>
#include <iostream>

#include "TWaveRecord.hpp"

TWaveRecord::TWaveRecord()
    : fpReadoutBuffer(nullptr),
      fpEventPtr(nullptr),
      fpEventStd(nullptr),
      fMaxBufferSize(0),
      fBufferSize(0),
      fNEvents(0),
      fReadSize(0),
      fBLTEvents(0),
      fRecordLength(0),
      fBaseLine(0),
      fVpp(0.),
      fVth(0.),
      fTriggerMode(CAEN_DGTZ_TRGMODE_ACQ_ONLY),
      fPolarity(CAEN_DGTZ_TriggerOnRisingEdge),
      fPostTriggerSize(50),
      fGateSize(0),
      fTimeOffset(0),
      fPreviousTime(0)
{
  SetParameters();
}

TWaveRecord::TWaveRecord(CAEN_DGTZ_ConnectionType type, int link, int node,
                         uint32_t VMEadd)
    : TWaveRecord()
{
  Open(type, link, node, VMEadd);
  Reset();
  GetBoardInfo();

  fDataArray = new unsigned char[fBLTEvents * ONE_HIT_SIZE * fNChs];
}

TWaveRecord::~TWaveRecord()
{
  auto err = CAEN_DGTZ_FreeReadoutBuffer(&fpReadoutBuffer);
  PrintError(err, "FreeReadoutBuffer");
  Reset();
  Close();

  delete fDataArray;
}

void TWaveRecord::SetParameters()
{
  // Reading parameter functions should be implemented!!!!!!!
  fRecordLength = kNSamples;
  fBLTEvents = 1024;  // This is the maximum
  fVpp = 2.;
  fVth = -0.5;
  // fVth = -0.001;
  fPolarity = CAEN_DGTZ_TriggerOnFallingEdge;
  // fTriggerMode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
  fPostTriggerSize = 50;
  fGateSize = 600;  // ns
}

void TWaveRecord::Initialize()
{
  CAEN_DGTZ_ErrorCode err;

  Reset();
  AcquisitionConfig();
  TriggerConfig();

  err = CAEN_DGTZ_SetMaxNumEventsBLT(fHandler, fBLTEvents);
  PrintError(err, "SetMaxNEventsBLT");
  err = CAEN_DGTZ_MallocReadoutBuffer(fHandler, &fpReadoutBuffer,
                                      &fMaxBufferSize);
  PrintError(err, "MallocReadoutBuffer");

  BoardCalibration();
}

void TWaveRecord::ReadEvents()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_ReadData(fHandler, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                           fpReadoutBuffer, &fBufferSize);
  PrintError(err, "ReadData");

  err =
      CAEN_DGTZ_GetNumEvents(fHandler, fpReadoutBuffer, fBufferSize, &fNEvents);
  PrintError(err, "GetNumEvents");
  // std::cout << fNEvents << " Events" << std::endl;
  // if (fNEvents > 1) fNEvents = 1;

  // fData->clear();
  TStdData data;
  for (uint32_t iEve = 0; iEve < fNEvents; iEve++) {
    err = CAEN_DGTZ_GetEventInfo(fHandler, fpReadoutBuffer, fBufferSize, iEve,
                                 &fEventInfo, &fpEventPtr);
    PrintError(err, "GetEventInfo");
    // std::cout << "Event number:\t" << iEve << '\n'
    //           << "Event size:\t" << fEventInfo.EventSize << '\n'
    //           << "Board ID:\t" << fEventInfo.BoardId << '\n'
    //           << "Pattern:\t" << fEventInfo.Pattern << '\n'
    //           << "Ch mask:\t" << fEventInfo.ChannelMask << '\n'
    //           << "Event counter:\t" << fEventInfo.EventCounter << '\n'
    //           << "Trigger time tag:\t" << fEventInfo.TriggerTimeTag
    //           << std::endl;

    err = CAEN_DGTZ_DecodeEvent(fHandler, fpEventPtr, (void **)&fpEventStd);
    PrintError(err, "DecodeEvent");

    // It should be for each channel!
    uint64_t timeStamp = (fEventInfo.TriggerTimeTag + fTimeOffset) * fTSample;
    if (timeStamp < fPreviousTime) {
      constexpr uint32_t maxTime = 0xFFFFFFFF / 2;  // Check manual
      timeStamp += maxTime * fTSample;
      fTimeOffset += maxTime;
    }
    fPreviousTime = timeStamp;

    int index = 0;
    auto nLoop = fNChs;
    if (fSingleChFlag) nLoop = 1;
    for (uint32_t iCh = 0; iCh < nLoop; iCh++) {
      double sumCharge = 0.;

      // fBaseLine = (1 << fNBits) * fDCOffset;
      fBaseLine = 0;
      auto baseSample = 100;
      for (uint32_t i = kNSamples - baseSample; i < kNSamples; i++) {
        fBaseLine += fpEventStd->DataChannel[iCh][i];
      }
      fBaseLine /= baseSample;

      for (uint32_t i = 0; i < kNSamples; i++) {
        sumCharge += (fBaseLine - fpEventStd->DataChannel[iCh][i]);
        // std::cout << fBaseLine << "\t" << fpEventStd->DataChannel[iCh][i]
        //           << std::endl;
      }

      // int index = (iEve * (fNChs * ONE_HIT_SIZE)) + (iCh * ONE_HIT_SIZE);
      fDataArray[index++] = fModNumber;  // fModNumber is needed.
      fDataArray[index++] = iCh;         // int to char.  Dangerous

      constexpr auto timeSize = sizeof(timeStamp);
      memcpy(&fDataArray[index], &timeStamp, timeSize);
      index += timeSize;

      // int16_t adc = sumCharge / 1000000;
      // constexpr auto adcSize = sizeof(adc);
      // memcpy(&fDataArray[index], &adc, adcSize);
      // index += adcSize;
      constexpr auto adcSize = sizeof(sumCharge);
      memcpy(&fDataArray[index], &sumCharge, adcSize);
      index += adcSize;

      constexpr auto waveSize =
          sizeof(fpEventStd->DataChannel[0][0]) * kNSamples;
      memcpy(&fDataArray[index], fpEventStd->DataChannel[iCh], waveSize);

      // std::cout << "time:\t" << fEventInfo.TriggerTimeTag << "\t" <<
      // timeStamp
      //<< std::endl;
    }
  }
}

void TWaveRecord::AcquisitionConfig()
{
  CAEN_DGTZ_ErrorCode err;

  // Eanble all channels
  uint32_t mask = ((1 << fNChs) - 1);
  if (fSingleChFlag) mask = 1;  // Only for ch 0
  err = CAEN_DGTZ_SetChannelEnableMask(fHandler, mask);
  PrintError(err, "SetChannelEnableMask");

  // Set DC offset
  auto fac = (1. - fDCOffset);
  if (fac <= 0. || fac >= 1.) fac = 0.5;
  uint32_t offset = 0xFFFF * fac;
  for (uint32_t iCh = 0; iCh < fNChs; iCh++)
    err = CAEN_DGTZ_SetChannelDCOffset(fHandler, iCh, offset);
  PrintError(err, "SetChannelDCOffset");

  // Set the acquisition mode
  err = CAEN_DGTZ_SetAcquisitionMode(fHandler, CAEN_DGTZ_SW_CONTROLLED);
  PrintError(err, "SetAcquisitionMode");

  // Set record length (length of waveform?);
  err = CAEN_DGTZ_SetRecordLength(fHandler, fRecordLength);
  PrintError(err, "SetRecordLength");
}

void TWaveRecord::TriggerConfig()
{
  CAEN_DGTZ_ErrorCode err;

  // Set the trigger threshold
  // The unit of its are V
  int32_t th = (1 << fNBits) * (fVth / fVpp);
  auto offset = (1 << fNBits) * fDCOffset;
  auto thVal = th + offset;
  std::cout << "Vth:\t" << thVal << "\t" << fVth << std::endl;

  for (uint32_t iCh = 0; iCh < fNChs; iCh++) {
    // Think about multiple channel setting
    err = CAEN_DGTZ_SetChannelTriggerThreshold(fHandler, iCh, thVal);
    PrintError(err, "SetChannelTriggerThreshold");
  }

  // Set the triggermode
  uint32_t mask = ((1 << fNChs) - 1);
  if (fSingleChFlag) mask = 1;  // Only for ch 0
  err = CAEN_DGTZ_SetChannelSelfTrigger(fHandler, fTriggerMode, mask);
  PrintError(err, "SetChannelSelfTrigger");
  // err = CAEN_DGTZ_SetSWTriggerMode(fHandler, fTriggerMode);
  // PrintError(err, "SetSWTriggerMode");

  // Set post trigger size
  err = CAEN_DGTZ_SetPostTriggerSize(fHandler, fPostTriggerSize);
  PrintError(err, "SetPostTriggerSize");

  // Set the triiger polarity
  for (uint32_t iCh = 0; iCh < fNChs; iCh++)
    CAEN_DGTZ_SetTriggerPolarity(fHandler, iCh, fPolarity);

  CAEN_DGTZ_TriggerPolarity_t pol;
  CAEN_DGTZ_GetTriggerPolarity(fHandler, 0, &pol);
  std::cout << "Polarity:\t" << pol << std::endl;
}

CAEN_DGTZ_ErrorCode TWaveRecord::StartAcquisition()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_SWStartAcquisition(fHandler);
  PrintError(err, "StartAcquisition");

  err = CAEN_DGTZ_AllocateEvent(fHandler, (void **)&fpEventStd);
  PrintError(err, "AllocateEvent");

  fTimeOffset = 0;
  fPreviousTime = 0;

  return err;
}

void TWaveRecord::StopAcquisition()
{
  CAEN_DGTZ_ErrorCode err;
  err = CAEN_DGTZ_SWStopAcquisition(fHandler);
  PrintError(err, "StopAcquisition");

  err = CAEN_DGTZ_FreeEvent(fHandler, (void **)&fpEventStd);
  PrintError(err, "FreeEvent");
}
