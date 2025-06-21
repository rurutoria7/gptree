#pragma once

float4 qMul(in const float4 a, in const float4 b){
    return float4(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    );
}

float4 qId(){
    return float4(0.,0.,0.,1.);
}

float4 qConj(in const float4 q){
    return float4(-q.xyz, q.w);
}

float3 qTransform(in const float4 q, in const float3 p){
    return qMul(qMul(q, float4(p, 0.)), qConj(q)).xyz;
}

float3 qGetX(in const float4 q){
    return float3(q.w*q.w + q.x*q.x - q.y*q.y - q.z*q.z,
                   2*q.w*q.z + 2*q.x*q.y,
                  -2*q.w*q.y + 2*q.x*q.z);
}

float3 qGetY(in const float4 q){
    return float3(-2*q.w*q.z + 2*q.x*q.y,
                   q.w*q.w - q.x*q.x + q.y*q.y - q.z*q.z,
                   2*q.w*q.x + 2*q.y*q.z);
}

float3 qGetZ(in const float4 q){
    return float3( 2*q.w*q.y + 2*q.x*q.z,
                  -2*q.w*q.x + 2*q.y*q.z,
                  q.w*q.w - q.x*q.x - q.y*q.y + q.z*q.z);
}

float4 qRotateAxisAngle(in const float3 axis, in const float angle){
    float halfAngle = angle * 0.5;
    return float4(normalize(axis) * sin(halfAngle), cos(halfAngle));
}
float4 qRotateX(in const float radians){ return qRotateAxisAngle(float3(1,0,0), radians);}
float4 qRotateY(in const float radians){ return qRotateAxisAngle(float3(0,1,0), radians);}
float4 qRotateZ(in const float radians){ return qRotateAxisAngle(float3(0,0,1), radians);}

float4 qSlerp(in const float4 a, in const float4 b, in const float t) {
    float theta = acos(clamp(dot(a, b), -1.0, 1.0));
    if (abs(theta) < 1e-5) return lerp(a, b, t);
    
    float wa = sin((1.0 - t) * theta);
    float wb = sin(t * theta);
    return (a * wa + b * wb) / sin(theta);
}

float4 qSlerpShortest(in const float4 a, in const float4 b, in const float t) {
    if (dot(a, b) < 0.0) return qSlerp(a, -b, t);        
    return qSlerp(a, b, t);
}

float4 qSlerp_(in const float4 a, in const float4 b, in const float t) {
    static const float h = 0.001;
    return (qSlerp_(a, b, t + h) - qSlerp_(a, b, t - h)) / (2*h);
}

#define FACT(BITS) ((1 << (BITS-1)) - 1)
#define MASK(BITS) ((1 << BITS) - 1)
#define X 10
#define Y 11
#define Z 10

#define MSB (1 << 31)

uint qCompress32(float4 q) {
    // L2 -> L1 norm for xyz
    q.xyz /= abs(q.w) + abs(q.x) + abs(q.y) + abs(q.z);
    
    // Encode x, y, z into 10 bits for y, 11 bits for xz
    int x = int(q.x * FACT(X)) & MASK(X);
    int y = int(q.y * FACT(Y)) & MASK(Y);
    int z = int(q.z * FACT(Z)) & MASK(Z);

    // Pack signed x, y, z into uint
    return uint(x) | (uint(y) << X) | (uint(z) << (X+Y)) | (asuint(q.w) & MSB);
}

float4 qDecompress32(uint encoded) {
    // Extract and sign-extend x, y, z
    int x = int(((encoded >> 0)     & MASK(X)) << (32 - X)) >> (32 - X);
    int y = int(((encoded >> X)     & MASK(Y)) << (32 - Y)) >> (32 - Y);
    int z = int(((encoded >> (X+Y)) & MASK(Z)) << (32 - Z)) >> (32 - Z);

    float4 q;
    q.x = x / float(FACT(X));
    q.y = y / float(FACT(Y));
    q.z = z / float(FACT(Z));

    q.w = 1.0 - abs(q.x) - abs(q.y) - abs(q.z);
    q.w = asfloat(asuint(q.w) | (encoded & MSB));
    return normalize(q);
}

uint64_t qCompress64(float4 q){
    // Ignore sign of real component
    q.xyz = (q.w < 0) ? -q.xyz : q.xyz;
    // L2 -> L1 norm for xyz
    q.xyz /= abs(q.w) + abs(q.x) + abs(q.y) + abs(q.z);
    
    // Encode x, y, z into 21 bits each
    int x = int(q.x * 0xfffff) & 0x1fffff;
    int y = int(q.y * 0xfffff) & 0x1fffff;
    int z = int(q.z * 0xfffff) & 0x1fffff;

    // Pack signed x, y, z into uint
    return uint64_t(x) | (uint64_t(y) << 21) | (uint64_t(z) << 42);
}

float4 qDecompress64(uint64_t encoded){
    // Extract and sign-extend x, y, z
    int x = int((uint(encoded >> 00) & 0x1fffff) << 11) >> 11;
    int y = int((uint(encoded >> 21) & 0x1fffff) << 11) >> 11;
    int z = int((uint(encoded >> 42) & 0x1fffff) << 11) >> 11;

    float4 q;
    q.x = x / float(0xfffff);
    q.y = y / float(0xfffff);
    q.z = z / float(0xfffff);

    q.w = 1.0 - abs(q.x) - abs(q.y) - abs(q.z);
    return normalize(q);
}
