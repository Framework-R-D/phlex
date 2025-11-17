#include "phlex/model/type_id.hpp"

#include <array>
#include <vector>

using namespace phlex::experimental;

int main() {
  static_assert(make_type_id<bool>().fundamental() == type_id::builtin::bool_v);
  static_assert(make_type_id<char>().fundamental() == type_id::builtin::char_v);
  static_assert(make_type_id<int>().fundamental() == type_id::builtin::int_v);
  static_assert(make_type_id<double>().fundamental() == type_id::builtin::double_v);
  static_assert(make_type_id<long>().fundamental() == type_id::builtin::long_v);
  static_assert(make_type_id<long long>().fundamental() == type_id::builtin::long_long_v);

  static_assert(not make_type_id<int>().is_unsigned());
  static_assert(make_type_id<unsigned int>().is_unsigned());
  
  static_assert(not make_type_id<char>().is_list());
  static_assert(not make_type_id<int>().is_list());
  static_assert(not make_type_id<float>().is_list());
  
  static_assert(make_type_id<std::vector<char>>().is_list());
  static_assert(make_type_id<std::vector<int>>().is_list());
  static_assert(make_type_id<std::vector<float>>().is_list());

  static_assert(make_type_id<std::array<unsigned long, 5>>().is_unsigned());
  static_assert(make_type_id<std::array<unsigned long, 5>>().is_list());
}
