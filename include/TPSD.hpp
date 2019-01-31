#ifndef TPSD_hpp
#define TPSD_hpp 1

#include "TDigitizer.hpp"

class TPSD : public TDigitizer
{
 public:
  TPSD();
  TPSD(CAEN_DGTZ_ConnectionType type, int link, int node = 0,
       uint32_t VMEadd = 0);
  ~TPSD();

  void Initialize();

  void ReadEvents();

  CAEN_DGTZ_ErrorCode StartAcquisition();
  void StopAcquisition();

  uint32_t GetNEvents() { return fNEvents; };

  void SetMaster();
  void SetSlave();
  void StartSyncMode(uint32_t nMods);

  void SetThreshold(double th) { fVth = th; };

  void SetDCOffset(double val) { fDCOffset = val; };
  //
  // void SetSingleCh(bool flag) { fSingleChFlag = flag; };

 private:
  virtual void SetParameters();

  virtual void AcquisitionConfig();
  virtual void TriggerConfig();

  void SetPSDPar();
  CAEN_DGTZ_DPP_PSD_Params_t fParPSD;
  CAEN_DGTZ_TriggerMode_t fTriggerMode;
  uint32_t fPostTriggerSize;
  uint32_t fRecordLength;

  double fVpp;
  double fVth;
  uint32_t fBLTEvents;
  double fDCOffset;  // 0 to 1

  // Memory
  void AllocateMemory();
  void FreeMemory();
  char *fpReadoutBuffer;                         // readout buffer
  CAEN_DGTZ_DPP_PSD_Event_t **fppPSDEvents;      // events buffer
  CAEN_DGTZ_DPP_PSD_Waveforms_t *fpPSDWaveform;  // waveforms buffer

  // Data
  std::vector<uint64_t> fTimeOffset;
  std::vector<uint64_t> fPreviousTime;
  std::vector<uint64_t> fTime;

  uint32_t fNEvents;
};

#endif
