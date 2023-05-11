#ifndef __INTERFACE_HPP__
#define __INTERFACE_HPP__

#include "configureInterface.hpp"
#include <thread>


namespace micagent{
using namespace std;


class interface{
public:
    static void interface_init(int paramc, char* params[]);
};
}

#endif
