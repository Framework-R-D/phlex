#ifndef FORM_FORM_TECHNOLOGY_HPP
#define FORM_FORM_TECHNOLOGY_HPP

namespace form {
  namespace technology {
    // Helper constants - make these constexpr too
    constexpr int ROOT_MAJOR = 1;
    constexpr int ROOT_TTREE_MINOR = 1;
    constexpr int ROOT_RNTUPLE_MINOR = 2;
    constexpr int HDF5_MAJOR = 2;

    // Helper function for combining major/minor
    constexpr int Combine(int major, int minor) { return major * 256 + minor; }

    // Technology constants using the helper
    constexpr int ROOT_TTREE = Combine(ROOT_MAJOR, ROOT_TTREE_MINOR);
    constexpr int ROOT_RNTUPLE = Combine(ROOT_MAJOR, ROOT_RNTUPLE_MINOR);
    constexpr int HDF5 = Combine(HDF5_MAJOR, 1);

    // Helper functions
    inline int GetMajor(int tech) { return tech / 256; }
    inline int GetMinor(int tech) { return tech % 256; }
  }

} // namespace form

#endif // FORM_FORM_TECHNOLOGY_HPP
