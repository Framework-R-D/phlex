//A data product that the RNTuple spec says cannot be serialized into an RNTuple automatically
#ifndef TEST_FORM_DATA_PRODUCTS_UNSERIALIZABLE_HPP
#define TEST_FORM_DATA_PRODUCTS_UNSERIALIZABLE_HPP

#include <cstddef>

class unserializable
{
  public:
    unserializable(size_t size);
    ~unserializable() = default;

    double* get_x();
    double* get_y();
    double* get_z();
    size_t get_size() const;

  private:
    double* x_;
    double* y_;
    double* z_;
    size_t size_;
};

#endif //TEST_FORM_DATA_PRODUCTS_UNSERIALIZABLE_HPP
