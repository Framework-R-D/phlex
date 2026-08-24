//A track_start is a 3-vector of position components.
//This is a simple test data product for demonstrating the features of FORM.

#include <iostream>

#ifndef TEST_FORM_DATA_PRODUCTS_EXTRA_MEMBER_TRACK_START_HPP
#define TEST_FORM_DATA_PRODUCTS_EXTRA_MEMBER_TRACK_START_HPP

class track_start {
public:
  track_start();
  track_start(float x, float y, float z, int index);
  ~track_start() = default;

  float get_x() const;
  float get_y() const;
  float get_z() const;
  int get_index() const;

  void set_x(float x);
  void set_y(float y);
  void set_z(float z);
  void set_index(int index);

  track_start operator+(track_start const& other) const;
  track_start& operator+=(track_start const& other);
  track_start operator-(track_start const& other) const;

private:
  float x_;
  float y_;
  float z_;

  int index_;
};

std::ostream& operator<<(std::ostream& os, track_start const& track);

#endif // TEST_FORM_DATA_PRODUCTS_EXTRA_MEMBER_TRACK_START_HPP
