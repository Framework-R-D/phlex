#include "track_start.hpp"

TrackStart::TrackStart() : m_x(0), m_y(0), m_z(0), m_index(0) {}

TrackStart::TrackStart(float x, float y, float z, int index) :
  m_x(x), m_y(y), m_z(z), m_index(index)
{
}

float TrackStart::getX() const { return m_x; }

float TrackStart::getY() const { return m_y; }

float TrackStart::getZ() const { return m_z; }

int TrackStart::getIndex() const { return m_index; }

void TrackStart::setX(float x) { m_x = x; }

void TrackStart::setY(float y) { m_y = y; }

void TrackStart::setZ(float z) { m_z = z; }

void TrackStart::setIndex(int index) { m_index = index; }

TrackStart TrackStart::operator+(TrackStart const& other) const
{
  return TrackStart(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z, m_index + other.m_index);
}

TrackStart& TrackStart::operator+=(TrackStart const& other)
{
  m_x += other.m_x;
  m_y += other.m_y;
  m_z += other.m_z;
  m_index += other.m_index;
  return *this;
}

TrackStart TrackStart::operator-(TrackStart const& other) const
{
  return TrackStart(m_x - other.m_x, m_y - other.m_y, m_z - other.m_z, m_index - other.m_index);
}

std::ostream& operator<<(std::ostream& os, TrackStart const& track)
{
  os << "TrackStart{" << track.getX() << ", " << track.getY() << ", " << track.getZ() << "}";
  return os;
}
