#ifndef PHLEX_MODEL_QUALIFIED_NAME_HPP
#define PHLEX_MODEL_QUALIFIED_NAME_HPP

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/type_id.hpp"

#include <string>
#include <vector>

namespace phlex::experimental {
  class qualified_name {
  public:
    qualified_name();
    qualified_name(char const* name);
    qualified_name(std::string name);
    qualified_name(algorithm_name qualifier, std::string name, type_id type);

    std::string full() const;
    algorithm_name const& qualifier() const noexcept { return qualifier_; }
    std::string const& plugin() const noexcept { return qualifier_.plugin(); }
    std::string const& algorithm() const noexcept { return qualifier_.algorithm(); }
    std::string const& name() const noexcept { return name_; }
    type_id type() const noexcept { return type_id_; }

    void set_type(type_id&& type) { type_id_ = std::move(type); }

    bool operator==(qualified_name const& other) const;
    bool operator!=(qualified_name const& other) const;
    bool operator<(qualified_name const& other) const;

    static qualified_name create(char const* c);
    static qualified_name create(std::string const& s);

  private:
    algorithm_name qualifier_;
    std::string name_;
    type_id type_id_{};
  };

  using qualified_names = std::vector<qualified_name>;

  class to_qualified_name {
  public:
    explicit to_qualified_name(algorithm_name const& qualifier) : qualifier_{qualifier} {}
    qualified_name operator()(std::string const& name, type_id type) const
    {
      return qualified_name{qualifier_, name, type};
    }

  private:
    algorithm_name const& qualifier_;
  };

  qualified_names to_qualified_names(std::string const& name,
                                     std::vector<std::string> output_labels,
                                     std::vector<type_id> output_types);
}

#endif // PHLEX_MODEL_QUALIFIED_NAME_HPP
