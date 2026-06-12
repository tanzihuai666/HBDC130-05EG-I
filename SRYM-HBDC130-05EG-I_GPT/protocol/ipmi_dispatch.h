#ifndef IPMI_DISPATCH_H
#define IPMI_DISPATCH_H

#include "ipmb_frame.h"

void ipmi_init(uint8_t own_addr_8bit, uint8_t ga_id);
void ipmi_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp);

#endif
