#include "track_start.hpp"

track_start::track_start() : x_(0), y_(0), z_(0) {}

track_start::track_start(float x, float y, float z) : x_(x), y_(y), z_(z) {}

float track_start::get_x() const { return x_; }

float track_start::get_y() const { return y_; }

float track_start::get_z() const { return z_; }

track_start track_start::operator+(track_start const& other) const
{
  return {x_ + other.x_, y_ + other.y_, z_ + other.z_};
}

track_start& track_start::operator+=(track_start const& other)
{
  x_ += other.x_;
  y_ += other.y_;
  z_ += other.z_;
  return *this;
}

track_start track_start::operator-(track_start const& other) const
{
  return {x_ - other.x_, y_ - other.y_, z_ - other.z_};
}

std::ostream& operator<<(std::ostream& os, track_start const& track)
{
  os << "track_start{" << track.get_x() << ", " << track.get_y() << ", " << track.get_z() << "}";
  return os;
}
