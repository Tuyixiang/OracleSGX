#ifndef _A_ORACLE_PORT_H_
#define _A_ORACLE_PORT_H_

#include <boost/asio/io_context.hpp>

boost::asio::io_context& oracle_global_ctx();

#endif  // _A_ORACLE_PORT_H_