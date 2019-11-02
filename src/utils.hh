#pragma once

#include <string>
#include <iostream>
#include <chrono>

#include "defs.hh"

void setVerboseMode(bool enabled);
std::ostream& logstream(std::ostream& s, std::string label);
std::ostream& err();
std::ostream& warn();
std::ostream& info();
std::ostream& dbg();

void randSetSeed(int seed);
s32 randInt(s32 exclusiveMax);
double randProb();

// Time utilities
using time_point = std::chrono::time_point<std::chrono::system_clock>;
double elapsedSecs(time_point start);
