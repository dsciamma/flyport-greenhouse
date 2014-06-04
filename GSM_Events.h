#include "GenericTypeDefs.h"
#include "HWlib.h"
#include "HILOlib.h"

void OnRing(char* phoneNumber);
void OnNoCarrier(char* phoneNumber);
void OnBusy(char* phoneNumber);
void OnSMSReceived(BYTE memtype, int index);
void OnSMSSentReport(int msgreference, int msgreport);
void OnError(int error, int errorNumber);
void OnRegistration(BYTE Status);
void OnLowPowerDisabled();
