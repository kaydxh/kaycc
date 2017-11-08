#ifndef KAYCC_BASE_LOG_H
#define KAYCC_BASE_LOG_H

#include <iostream>

#define LOG std::cout <<  __FILE__ << ":" << __LINE__ << ":" << __FUNCTION__ << " "

#endif