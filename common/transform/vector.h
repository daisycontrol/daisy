#pragma  once

struct Vector{
    float x;
    float y;
    float z;
};

float dot_vector(struct Vector v1, struct Vector v2);
struct Vector cross_vector(struct Vector v1, struct Vector v2);
float norm_vector(struct Vector v);

