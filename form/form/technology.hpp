#ifndef __TECHNOLOGY_HPP__
#define __TECHNOLOGY_HPP__

namespace form {

  // Technology constants - single source of truth
  namespace Technology {
    const int ROOT_TTREE = 1 * 256 + 1;   // ROOT TTree
    const int ROOT_RNTUPLE = 1 * 256 + 2; // ROOT RNtuple
    const int HDF5 = 2 * 256 + 1;         // HDF5

    // Helper constants for factories
    const int ROOT_MAJOR = 1;         // ROOT major technology ID
    const int ROOT_TTREE_MINOR = 1;   // ROOT TTree minor technology ID
    const int ROOT_RNTUPLE_MINOR = 2; // ROOT RNtuple minor technology ID
    const int HDF5_MAJOR = 2;         // HDF5 major technology ID

    // Helper functions
    inline int GetMajor(int tech) { return tech / 256; }
    inline int GetMinor(int tech) { return tech % 256; }
  }

} // namespace form

#endif
