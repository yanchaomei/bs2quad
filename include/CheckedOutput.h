#ifndef BS2QUAD_CHECKED_OUTPUT_H
#define BS2QUAD_CHECKED_OUTPUT_H
#include <fstream>
#include <stdexcept>
#include <string>

namespace BS2Quad {
// Buffered streams can appear healthy until flush/close. A saved report is a
// required output, so its final write failure must affect the process status.
inline void FinishOutput(std::ofstream& file,const std::string& label) {
    file.flush();
    if(!file) throw std::runtime_error("OUTPUT_WRITE_FAILED: "+label);
    file.close();
    if(!file) throw std::runtime_error("OUTPUT_CLOSE_FAILED: "+label);
}
}
#endif
