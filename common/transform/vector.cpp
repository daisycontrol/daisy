#include "vector.h"
#include "math.h"

float dot_vector(struct Vector v1, struct Vector v2){
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

struct Vector cross_vector(struct Vector v1, struct Vector v2){
  struct Vector crossed;
  crossed.x = v1.y * v2.z - v1.z * v2.y;
  crossed.y = v1.z * v2.x - v1.x * v2.z;
  crossed.z = v1.x * v2.y - v1.y * v2.x;
  return crossed;
}

float norm_vector(struct Vector v){
  return sqrt(pow(v.x, 2) +
              pow(v.y, 2) +
              pow(v.z, 2));
}
