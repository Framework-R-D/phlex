#ifndef __TECHNOLOGY_HPP__
#define __TECHNOLOGY_HPP__

namespace form::config {

  // Technology constants - single source of truth
  namespace Technology {
    const int ROOT_TTREE = 1 * 256 + 1;   // ROOT TTree
    const int ROOT_RNTUPLE = 1 * 256 + 2; // ROOT RNtuple
    const int HDF5 = 2 * 256 + 1;         // HDF5
  }

} // namespace form::config

#endif
