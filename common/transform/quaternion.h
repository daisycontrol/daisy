#pragma once

struct Quaternion{
    float w;
    float x;
    float y;
    float z;
};

struct Quaternion scale_quaternion(struct Quaternion quat, float factor);
struct Quaternion lerp_quaternion(struct Quaternion quat1, struct Quaternion quat2, double factor);
struct Quaternion lerp_quaternion_with_identity(struct Quaternion quat1, double factor);
struct Quaternion invert_quaternion(struct Quaternion quat);
struct Quaternion multiply_quaternion(struct Quaternion quat1, struct Quaternion quat2);
struct Quaternion sum_quaternion(struct Quaternion quat1, struct Quaternion quat2);
struct Quaternion normalize_quaternion(struct Quaternion quat);