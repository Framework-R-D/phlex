#include "track_start.hpp"

track_start::track_start() : x_(0), y_(0), z_(0), index_(0) {}

track_start::track_start(float x, float y, float z, int index) : x_(x), y_(y), z_(z), index_(index)
{
}

float track_start::get_x() const { return x_; }

float track_start::get_y() const { return y_; }

float track_start::get_z() const { return z_; }

int track_start::get_index() const { return index_; }

void track_start::set_x(float x) { x_ = x; }

void track_start::set_y(float y) { y_ = y; }

void track_start::set_z(float z) { z_ = z; }

void track_start::set_index(int index) { index_ = index; }

track_start track_start::operator+(track_start const& other) const
{
  return {x_ + other.x_, y_ + other.y_, z_ + other.z_, index_ + other.index_};
}

track_start& track_start::operator+=(track_start const& other)
{
  x_ += other.x_;
  y_ += other.y_;
  z_ += other.z_;
  index_ += other.index_;
  return *this;
}

track_start track_start::operator-(track_start const& other) const
{
  return {x_ - other.x_, y_ - other.y_, z_ - other.z_, index_ - other.index_};
}

std::ostream& operator<<(std::ostream& os, track_start const& track)
{
  os << "track_start{" << track.get_x() << ", " << track.get_y() << ", " << track.get_z() << "}";
  return os;
}
