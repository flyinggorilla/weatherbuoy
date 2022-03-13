#ifndef INCLUDE_GEOUTIL_H
#define INCLUDE_GEOUTIL_H

#include <math.h>
#include <cmath>

// This function converts decimal degrees to radians
inline double deg2rad(double deg)
{
  return (deg * M_PI / 180.0);
}

// Calculate distance of two GPS coordinates (lat/lon in degrees)
// Equirectangular approximation
// returns distance in meters
inline double geoDistance(double lat1, double lon1, double lat2, double lon2)
{
  static const double earthRadius = 6371 * 1000; // radius of the earth in m
  double u = deg2rad(lon2 - lon1) * cos(0.5 * deg2rad(lat2 + lat1));
  double v = deg2rad(lat2 - lat1);
  return earthRadius * sqrt(u * u + v * v);
}

// https://en.wikipedia.org/wiki/Haversine_formula
inline double geoDistanceHaversine(double lat1d, double lon1d, double lat2d, double lon2d)
{
  static const double earthRadius = 6371 * 1000; // meters
  double lat1r, lon1r, lat2r, lon2r, u, v;
  lat1r = deg2rad(lat1d);
  lon1r = deg2rad(lon1d);
  lat2r = deg2rad(lat2d);
  lon2r = deg2rad(lon2d);
  u = sin((lat2r - lat1r) / 2);
  v = sin((lon2r - lon1r) / 2);
  return 2.0 * earthRadius * asin(sqrt(u * u + cos(lat1r) * cos(lat2r) * v * v));
}

#endif