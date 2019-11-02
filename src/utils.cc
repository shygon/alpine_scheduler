#include "utils.hh"
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/null.hpp>
#include <chrono>
#include <iomanip>
#include <random>

#include "defs.hh"

using namespace std;

// Time utilities
double elapsedSecs(time_point start) {
  return chrono::duration_cast<chrono::milliseconds>(
    chrono::system_clock::now() - start).count() / 1000.0;
}


// Logging utilities
bool verboseMode = false;
boost::iostreams::stream< boost::iostreams::null_sink >
nullOstream( ( boost::iostreams::null_sink() ) );

void setVerboseMode(bool enabled) { verboseMode = enabled; }

std::ostream& logstream(std::ostream& s, std::string label) {
  time_point now = chrono::system_clock::now();
  time_t t = chrono::system_clock::to_time_t(now);
  return s << "[" << put_time(localtime(&t), "%X") << " " << label << "] ";
}
std::ostream& err()  { return logstream(cerr, "ERROR"); }
std::ostream& warn() { return logstream(cerr, "WARNING"); }
std::ostream& info() { return logstream(cerr, "INFO"); }
std::ostream& dbg()  { return verboseMode ? logstream(cerr, "DBG") : nullOstream; }

// Random generator utilities
mt19937 randEngine;
void randSetSeed(int seed) { randEngine.seed(seed); }
s32 randInt(s32 exclusiveMax) { return uniform_int_distribution<s32>(0, exclusiveMax - 1)(randEngine); }
double randProb() { return uniform_real_distribution<double>(0, 1)(randEngine); }
