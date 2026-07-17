#include "quaternion.h"
#include "math.h"

struct Quaternion scale_quaternion(struct Quaternion quat, float factor){
  struct Quaternion scaled;
  scaled.w = quat.w * factor;
  scaled.x = quat.x * factor;
  scaled.y = quat.y * factor;
  scaled.z = quat.z * factor;

  return scaled;
}

struct Quaternion lerp_quaternion(struct Quaternion quat1, struct Quaternion quat2, double factor){
  struct Quaternion result;
  result.w = quat1.w * factor + quat2.w * (1 - factor);
  result.x = quat1.x * factor + quat2.x * (1 - factor);
  result.y = quat1.y * factor + quat2.y * (1 - factor);
  result.z = quat1.z * factor + quat2.z * (1 - factor);

  return result;
}

struct Quaternion lerp_quaternion_with_identity(struct Quaternion quat1, double factor){
  struct Quaternion result;
  result.w = quat1.w * factor + 1.0f * (1 - factor);
  result.x = quat1.x * factor;
  result.y = quat1.y * factor;
  result.z = quat1.z * factor;

  return result;
}

struct Quaternion invert_quaternion(struct Quaternion quat){
  struct Quaternion inverse;
  inverse.w = quat.w;
  inverse.x = - quat.x;
  inverse.y = - quat.y;
  inverse.z =  - quat.z;

  return inverse;
}

struct Quaternion multiply_quaternion(struct Quaternion quat1, struct Quaternion quat2){
  struct Quaternion multiplied;
  multiplied.w = quat1.w*quat2.w - quat1.x*quat2.x - quat1.y*quat2.y - quat1.z*quat2.z;
  multiplied.x = quat1.w*quat2.x + quat1.x*quat2.w + quat1.y*quat2.z - quat1.z*quat2.y;
  multiplied.y = quat1.w*quat2.y + quat1.y*quat2.w + quat1.z*quat2.x - quat1.x*quat2.z;
  multiplied.z = quat1.w*quat2.z + quat1.z*quat2.w + quat1.x*quat2.y - quat1.y*quat2.x;

  return multiplied;
}

struct Quaternion sum_quaternion(struct Quaternion quat1, struct Quaternion quat2){
  struct Quaternion summed;


  summed.w = quat1.w + quat2.w;
  summed.x = quat1.x + quat2.x;
  summed.y = quat1.y + quat2.y;
  summed.z = quat1.z + quat2.z;

  return summed;
}

struct Quaternion normalize_quaternion(struct Quaternion quat){
  double mag = sqrt(pow(quat.x, 2) +
                    pow(quat.y, 2) +
                    pow(quat.z, 2) +
                    pow(quat.w, 2));
                    
  struct Quaternion normalized;

  normalized.w = quat.w / mag;
  normalized.x = quat.x / mag;
  normalized.y = quat.y / mag;
  normalized.z = quat.z / mag;

  return normalized;
} 