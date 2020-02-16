#ifndef _E_ENCLAVERESULT_H_
#define _E_ENCLAVERESULT_H_

#include "Config.h"
#include "sgx_utils.h"

struct EnclaveResult {
  char data[APP_RESPONSE_BUFFER_SIZE];
  int data_size;
  sgx_report_t report;
};

#endif  // _E_ENCLAVERESULT_H_