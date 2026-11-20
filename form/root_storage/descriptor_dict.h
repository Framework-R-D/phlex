#ifndef FORM_ROOT_STORAGE_DESCRIPTOR_DICT_H
#define FORM_ROOT_STORAGE_DESCRIPTOR_DICT_H

// Used only by ROOT dictionary generation:
//   - Parsed by genreflex to generate the dictionary for the descriptor's on-disk type:
//     - std::vector<std::pair<std::string, std::string>>
//   - ROOT carries no compiled collection proxy for that instantiation, so TFile::WriteObject
//     cannot write it without this dictionary.
//   - The generated source is compiled into the root_storage library itself; see CMakeLists.txt.

#include <string>
#include <utility>
#include <vector>

#endif // FORM_ROOT_STORAGE_DESCRIPTOR_DICT_H
