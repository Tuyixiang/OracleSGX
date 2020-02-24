#ifndef _A_EXECUTOR_PORT_H_
#define _A_EXECUTOR_PORT_H_

#include <boost/shared_ptr.hpp>
#include <string>

class Executor;

void executor_ias_callback(boost::shared_ptr<Executor> p_executor,
                            const std::string& response);

#endif  // _A_EXECUTOR_PORT_H_