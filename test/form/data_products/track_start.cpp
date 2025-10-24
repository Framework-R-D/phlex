#include "track_start.hpp"
#include <ostream>

TrackStart::TrackStart() : m_x(0), m_y(0), m_z(0) {}

TrackStart::TrackStart(float x, float y, float z) : m_x(x), m_y(y), m_z(z) {}

float TrackStart::getX() const { return m_x; }

float TrackStart::getY() const { return m_y; }

float TrackStart::getZ() const { return m_z; }

TrackStart TrackStart::operator+(TrackStart const& other) const
{
  return TrackStart(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z);
}

TrackStart& TrackStart::operator+=(TrackStart const& other)
{
  m_x += other.m_x;
  m_y += other.m_y;
  m_z += other.m_z;
  return *this;
}

TrackStart TrackStart::operator-(TrackStart const& other) const
{
  return TrackStart(m_x - other.m_x, m_y - other.m_y, m_z - other.m_z);
}

std::ostream& operator<<(std::ostream& os, TrackStart const& track)
{
  os << "TrackStart{" << track.getX() << ", " << track.getY() << ", " << track.getZ() << "}";
  return os;
}
