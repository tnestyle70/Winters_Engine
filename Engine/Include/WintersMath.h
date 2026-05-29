#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────
//  WintersMath.h  |  DirectXMath 래핑 타입
//
//  Vec3/Vec4/Mat4(XMFLOAT*)는 저장용 타입.
//  XMVECTOR/XMMATRIX는 함수 내부 연산용 임시 타입.
//
//  구조체이므로 memcpy, 배열 저장, 네트워크 전송 모두 안전.
//  XMVECTOR/XMMATRIX를 멤버/배열/패킷에 저장하지 말고
//  XMLoad* -> XM* 연산 -> XMStore* 경계에서만 사용한다.
//  힙이 본질적으로 위험한 것이 아니라, SIMD 저장 타입의
//  16B 정렬/레이아웃 조건을 전역 데이터 구조에서 놓치기 쉽다.
// ─────────────────────────────────────────────────────────────────

using namespace DirectX;

// ── Vec2 ─────────────────────────────────────────────────────────
struct Vec2
{
    float x = 0.f, y = 0.f;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
    Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
};

// ── Vec3 ─────────────────────────────────────────────────────────
struct Vec3
{
    float x = 0.f, y = 0.f, z = 0.f;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    explicit Vec3(const XMFLOAT3& f) : x(f.x), y(f.y), z(f.z) {}

    XMFLOAT3 ToXMFLOAT3() const { return { x, y, z }; }
    XMVECTOR ToXMVECTOR() const { return XMLoadFloat3(&reinterpret_cast<const XMFLOAT3&>(*this)); }

    Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }

    Vec3 Normalized() const
    {
        float len = Length();
        if (len < 1e-6f) return {};
        return { x / len, y / len, z / len };
    }

    static float Dot(const Vec3& a, const Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

namespace WintersMath
{
    inline constexpr float kEpsilon = 0.000001f;
    inline constexpr float kPi = 3.14159265358979323846f;
    inline constexpr float kTwoPi = 6.28318530717958647692f;

    inline float Clamp01(float value)
    {
        return std::clamp(value, 0.f, 1.f);
    }

    inline float Lerp(float from, float to, float t)
    {
        return from + (to - from) * t;
    }

    inline float LengthSq(const Vec3& v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    inline float LengthSqXZ(const Vec3& v)
    {
        return v.x * v.x + v.z * v.z;
    }

    inline float DistanceSqXZ(const Vec3& lhs, const Vec3& rhs)
    {
        const float dx = rhs.x - lhs.x;
        const float dz = rhs.z - lhs.z;
        return dx * dx + dz * dz;
    }

    inline Vec3 Normalize3D(Vec3 v,
        Vec3 fallback = Vec3{ 0.f, 0.f, 1.f },
        float epsilon = kEpsilon)
    {
        const float lenSq = LengthSq(v);
        if (lenSq <= epsilon)
            return fallback;

        const float invLen = 1.f / std::sqrt(lenSq);
        return Vec3{ v.x * invLen, v.y * invLen, v.z * invLen };
    }

    inline Vec3 NormalizeXZ(Vec3 v,
        Vec3 fallback = Vec3{ 0.f, 0.f, 1.f },
        float epsilon = kEpsilon)
    {
        v.y = 0.f;
        const float lenSq = LengthSqXZ(v);
        if (lenSq <= epsilon)
            return Vec3{ fallback.x, 0.f, fallback.z };

        const float invLen = 1.f / std::sqrt(lenSq);
        return Vec3{ v.x * invLen, 0.f, v.z * invLen };
    }

    inline Vec3 NormalizeXZOrZero(Vec3 v, float epsilon = kEpsilon)
    {
        return NormalizeXZ(v, Vec3{}, epsilon);
    }

    inline Vec3 DirectionXZ(const Vec3& from,
        const Vec3& to,
        Vec3 fallback = Vec3{ 0.f, 0.f, 1.f },
        float epsilon = kEpsilon)
    {
        return NormalizeXZ(Vec3{ to.x - from.x, 0.f, to.z - from.z },
            fallback, epsilon);
    }

    inline Vec3 RotateXZ(const Vec3& v, float radians)
    {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return Vec3{ v.x * c - v.z * s, 0.f, v.x * s + v.z * c };
    }

    inline Vec3 DirectionFromYawXZ(float yaw)
    {
        return Vec3{ std::sin(yaw), 0.f, std::cos(yaw) };
    }

    inline Vec3 VelocityFromDirectionXZ(
        Vec3 direction,
        float speed,
        Vec3 fallback = Vec3{ 0.f, 0.f, 1.f },
        float epsilon = kEpsilon)
    {
        const Vec3 dir = NormalizeXZ(direction, fallback, epsilon);
        return Vec3{ dir.x * speed, 0.f, dir.z * speed };
    }

    inline Vec3 LerpXZ(const Vec3& from, const Vec3& to, float t)
    {
        return Vec3{
            Lerp(from.x, to.x, t),
            from.y,
            Lerp(from.z, to.z, t)
        };
    }

    inline float YawFromDirectionXZ(
        Vec3 direction,
        float yawOffset = 0.f,
        Vec3 fallback = Vec3{ 0.f, 0.f, 1.f },
        float epsilon = kEpsilon)
    {
        const Vec3 dir = NormalizeXZ(direction, fallback, epsilon);
        return static_cast<float>(std::atan2(dir.x, dir.z) + yawOffset);
    }

    inline float NormalizeRadians(float radians)
    {
        if (!std::isfinite(radians))
            return 0.f;

        radians = std::fmod(radians + kPi, kTwoPi);
        if (radians < 0.f)
            radians += kTwoPi;
        return radians - kPi;
    }

    inline float NearestEquivalentRadians(float radians, float referenceRadians)
    {
        if (!std::isfinite(referenceRadians))
            return NormalizeRadians(radians);

        return referenceRadians + NormalizeRadians(radians - referenceRadians);
    }

    inline float DistanceSqPointToSegmentXZ(
        const Vec3& point,
        const Vec3& start,
        const Vec3& end,
        float* pOutT = nullptr,
        float epsilon = kEpsilon)
    {
        const float sx = end.x - start.x;
        const float sz = end.z - start.z;
        const float lenSq = sx * sx + sz * sz;
        if (lenSq <= epsilon)
        {
            if (pOutT)
                *pOutT = 0.f;
            return DistanceSqXZ(point, start);
        }

        const float px = point.x - start.x;
        const float pz = point.z - start.z;
        const float t = std::clamp((px * sx + pz * sz) / lenSq, 0.f, 1.f);
        if (pOutT)
            *pOutT = t;

        const Vec3 closest{ start.x + sx * t, start.y, start.z + sz * t };
        return DistanceSqXZ(point, closest);
    }
}

// ── Vec4 ─────────────────────────────────────────────────────────
struct Vec4
{
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;

    Vec4() = default;
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    XMFLOAT4 ToXMFLOAT4() const { return { x, y, z, w }; }
};

// ── Mat4 ─────────────────────────────────────────────────────────
struct Mat4
{
    XMFLOAT4X4 m = {};

    Mat4() { XMStoreFloat4x4(&m, XMMatrixIdentity()); }
    explicit Mat4(const XMMATRIX& mx) { XMStoreFloat4x4(&m, mx); }
    explicit Mat4(const XMFLOAT4X4& f) : m(f) {}

    XMMATRIX ToXMMATRIX() const { return XMLoadFloat4x4(&m); }

    Mat4 operator*(const Mat4& other) const
    {
        return Mat4(XMMatrixMultiply(ToXMMATRIX(), other.ToXMMATRIX()));
    }

    // ── 팩토리 함수 ──────────────────────────────────────────
    static Mat4 Identity() { return Mat4(XMMatrixIdentity()); }

    static Mat4 Translation(float x, float y, float z)
    {
        return Mat4(XMMatrixTranslation(x, y, z));
    }
    static Mat4 Translation(const Vec3& v) { return Translation(v.x, v.y, v.z); }

    static Mat4 Scale(float x, float y, float z)
    {
        return Mat4(XMMatrixScaling(x, y, z));
    }
    static Mat4 Scale(const Vec3& v) { return Scale(v.x, v.y, v.z); }

    static Mat4 RotationX(float radians) { return Mat4(XMMatrixRotationX(radians)); }
    static Mat4 RotationY(float radians) { return Mat4(XMMatrixRotationY(radians)); }
    static Mat4 RotationZ(float radians) { return Mat4(XMMatrixRotationZ(radians)); }

    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
    {
        return Mat4(XMMatrixLookAtLH(eye.ToXMVECTOR(), target.ToXMVECTOR(), up.ToXMVECTOR()));
    }

    static Mat4 Perspective(float fovY, float aspect, float nearZ, float farZ)
    {
        return Mat4(XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
    }

    static Mat4 Orthographic(float width, float height, float nearZ, float farZ)
    {
        return Mat4(XMMatrixOrthographicLH(width, height, nearZ, farZ));
    }

    // ── 변환 ─────────────────────────────────────────────────
    Vec3 TransformPoint(const Vec3& v) const
    {
        XMVECTOR result = XMVector3TransformCoord(v.ToXMVECTOR(), ToXMMATRIX());
        XMFLOAT3 f;
        XMStoreFloat3(&f, result);
        return Vec3(f);
    }

    Vec3 TransformDirection(const Vec3& v) const
    {
        XMVECTOR result = XMVector3TransformNormal(v.ToXMVECTOR(), ToXMMATRIX());
        XMFLOAT3 f;
        XMStoreFloat3(&f, result);
        return Vec3(f);
    }

    Mat4 Inverse() const
    {
        return Mat4(XMMatrixInverse(nullptr, ToXMMATRIX()));
    }

    Mat4 Transpose() const
    {
        return Mat4(XMMatrixTranspose(ToXMMATRIX()));
    }
};
