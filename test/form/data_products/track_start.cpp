#include "track_start.hpp"
#include <ostream>

TrackStart::TrackStart() : m_x(0), m_y(0), m_z(0) {}

TrackStart::TrackStart(float x, float y, float z) : m_x(x), m_y(y), m_z(z) {}

auto TrackStart::getX() const -> float { return m_x; }

auto TrackStart::getY() const -> float { return m_y; }

auto TrackStart::getZ() const -> float { return m_z; }

auto TrackStart::operator+(TrackStart const& other) const -> TrackStart
{
  return {m_x + other.m_x, m_y + other.m_y, m_z + other.m_z};
}

auto TrackStart::operator+=(TrackStart const& other) -> TrackStart&
{
  m_x += other.m_x;
  m_y += other.m_y;
  m_z += other.m_z;
  return *this;
}

auto TrackStart::operator-(TrackStart const& other) const -> TrackStart
{
  return {m_x - other.m_x, m_y - other.m_y, m_z - other.m_z};
}

auto operator<<(std::ostream& os, TrackStart const& track) -> std::ostream&
{
  os << "TrackStart{" << track.getX() << ", " << track.getY() << ", " << track.getZ() << "}";
  return os;
}
