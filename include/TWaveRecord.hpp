#ifndef TWaveRecord_hpp
#define TWaveRecord_hpp 1

// For the standard fiemware digitizer
// This will be super class of other firmware and model family

#include <string>
#include <vector>

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

#include "TDigitizer.hpp"
#include "TStdData.hpp"

class TWaveRecord : public TDigitizer
{
 public:
  TWaveRecord();
  TWaveRecord(CAEN_DGTZ_ConnectionType type, int link, int node = 0,
              uint32_t VMEadd = 0);
  virtual ~TWaveRecord();

  void Initialize();

  void ReadEvents();

  CAEN_DGTZ_ErrorCode StartAcquisition();
  void StopAcquisition();

  uint32_t GetNEvents() { return fNEvents * fNChs; };

  void SetThreshold(double th) { fVth = th; };

 protected:
  // For event readout
  char *fpReadoutBuffer;
  char *fpEventPtr;
  CAEN_DGTZ_EventInfo_t fEventInfo;
  CAEN_DGTZ_UINT16_EVENT_t *fpEventStd;  // for void **Eve
  uint32_t fMaxBufferSize;
  uint32_t fBufferSize;
  uint32_t fNEvents;
  uint32_t fReadSize;
  uint32_t fBLTEvents;
  uint32_t fRecordLength;
  uint32_t fBaseLine;

  // For trigger setting
  double fVpp;
  double fVth;
  CAEN_DGTZ_TriggerMode_t fTriggerMode;
  CAEN_DGTZ_TriggerPolarity_t fPolarity;
  uint32_t fPostTriggerSize;
  int32_t fGateSize;

  // Data
  uint64_t fTimeOffset;
  uint64_t fPreviousTime;

  void SetParameters();

  void AcquisitionConfig();
  void TriggerConfig();
};

#endif
