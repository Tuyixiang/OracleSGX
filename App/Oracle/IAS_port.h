#ifndef _A_IAS_PORT_H_
#define _A_IAS_PORT_H_

#include <boost/shared_ptr.hpp>
class Executor;

void send_ias(boost::shared_ptr<Executor> p_executor,
              const std::string& request);

#endif  // _A_IAS_PORT_H_