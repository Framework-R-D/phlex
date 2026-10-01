#include "test/form/data_products/unserializable.hpp"

#include <cstddef>

unserializable::unserializable(size_t size) :
  size_(size), x_(new double[size]), y_(new double[size]), z_(new double[size])
{
}

double* unserializable::get_x() { return x_; }
double* unserializable::get_y() { return y_; }
double* unserializable::get_z() { return z_; }
size_t unserializable::get_size() const { return size_; }
