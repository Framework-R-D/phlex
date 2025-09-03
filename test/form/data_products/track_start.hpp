//A TrackStart is a 3-vector of position components.
//This is a simple test data product for demonstrating the features of FORM.

#include <iostream>

#ifndef __TRACK_START_HPP__
#define __TRACK_START_HPP__

class TrackStart {
public:
  TrackStart();
  TrackStart(float x, float y, float z);
  ~TrackStart() = default;

  float getX() const;
  float getY() const;
  float getZ() const;

  TrackStart operator+(const TrackStart& other) const;
  TrackStart& operator+=(const TrackStart& other);
  TrackStart operator-(const TrackStart& other) const;

private:
  float m_x;
  float m_y;
  float m_z;
};

std::ostream& operator<<(std::ostream& os, const TrackStart& track);

#endif //__TRACK_START_HPP__
