#ifndef PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
#define PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/type_id.hpp"

#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  class product_specification {
  public:
    product_specification();
    product_specification(char const* name);
    product_specification(std::string name);
    product_specification(algorithm_name qualifier, std::string name, type_id type);

    std::string full() const;
    algorithm_name const& qualifier() const noexcept { return qualifier_; }
    std::string const& plugin() const noexcept { return qualifier_.plugin(); }
    std::string const& algorithm() const noexcept { return qualifier_.algorithm(); }
    std::string const& name() const noexcept { return name_; }
    type_id type() const noexcept { return type_id_; }

    void set_type(type_id&& type) { type_id_ = std::move(type); }

    bool operator==(product_specification const& other) const;
    bool operator!=(product_specification const& other) const;
    bool operator<(product_specification const& other) const;

    static product_specification create(char const* c);
    static product_specification create(std::string const& s);

  private:
    algorithm_name qualifier_;
    std::string name_;
    type_id type_id_{};
  };

  using product_specifications = std::vector<product_specification>;

  product_specifications to_product_specifications(std::string name,
                                                   std::vector<std::string> output_labels,
                                                   std::vector<type_id> output_types);
}

#endif // PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
