#pragma once

#include <array>
#include <string>
#include <cassert>
#include <string_view>
#include <sstream>
#include "debugapi.h"
#include <glm.hpp>

namespace quake::util
{
	template <typename... Ts>
	[[nodiscard]] std::string stringCat(const Ts&... xs)
	{
		std::ostringstream oss;
		(oss << ... << xs);
		return oss.str();
	}

	template <typename T, typename... Ts>
	[[nodiscard]] std::string stringCatSeparated(const std::string_view separator, const T& x, const Ts&... xs)
	{
		std::ostringstream oss;
		oss << x;
		((oss << separator << xs), ...);
		return oss.str();
	}

	template <typename... Ts>
	void debugPrint(const Ts&... xs)
	{
		OutputDebugStringA(stringCat(xs...).data());
	}

	template <typename... Ts>
	void debugPrintSeparated(const std::string_view separator, const Ts&... xs)
	{
		OutputDebugStringA(stringCatSeparated(separator, xs...).data());
	}
}
enum RotSeq { zyx, zyz, zxy, zxz, yxz, 
              yxy, yzx, yzy, xyz, xyx, 
              xzy, xzx };

static constexpr std::array<std::array<int, 3>, 12> mappings{
    std::array<int, 3>{2,1,0},
    std::array<int, 3>{2,1,2},
    std::array<int, 3>{2,0,1},
    std::array<int, 3>{2,0,2},
    std::array<int, 3>{1,0,2},

    std::array<int, 3>{1,0,1},
    std::array<int, 3>{1,2,0},
    std::array<int, 3>{1,2,1},
    std::array<int, 3>{0,1,2},
    std::array<int, 3>{0,1,0},

    std::array<int, 3>{0,2,1},
    std::array<int, 3>{0,2,0}
};

void twoaxisrot(double r11, double r12, double r21, double r31, double r32, double res[]) {
    res[0] = atan2(r11, r12);
    res[1] = acos(r21);
    res[2] = atan2(r31, r32);
}

void threeaxisrot(double r11, double r12, double r21, double r31, double r32, double res[]) {
    res[0] = atan2(r31, r32);
    res[1] = asin(r21);
    res[2] = atan2(r11, r12);
}

void quaternion2Euler(const glm::fquat& q, double res[], RotSeq rotSeq)
{
    switch (rotSeq) {
    case zyx:
        threeaxisrot(2 * (q.x * q.y + q.w * q.z),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            -2 * (q.x * q.z - q.w * q.y),
            2 * (q.y * q.z + q.w * q.x),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            res);
        break;

    case zyz:
        twoaxisrot(2 * (q.y * q.z - q.w * q.x),
            2 * (q.x * q.z + q.w * q.y),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            2 * (q.y * q.z + q.w * q.x),
            -2 * (q.x * q.z - q.w * q.y),
            res);
        break;

    case zxy:
        threeaxisrot(-2 * (q.x * q.y - q.w * q.z),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            2 * (q.y * q.z + q.w * q.x),
            -2 * (q.x * q.z - q.w * q.y),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            res);
        break;

    case zxz:
        twoaxisrot(2 * (q.x * q.z + q.w * q.y),
            -2 * (q.y * q.z - q.w * q.x),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            2 * (q.x * q.z - q.w * q.y),
            2 * (q.y * q.z + q.w * q.x),
            res);
        break;

    case yxz:
        threeaxisrot(2 * (q.x * q.z + q.w * q.y),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            -2 * (q.y * q.z - q.w * q.x),
            2 * (q.x * q.y + q.w * q.z),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            res);
        break;

    case yxy:
        twoaxisrot(2 * (q.x * q.y - q.w * q.z),
            2 * (q.y * q.z + q.w * q.x),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            2 * (q.x * q.y + q.w * q.z),
            -2 * (q.y * q.z - q.w * q.x),
            res);
        break;

    case yzx:
        threeaxisrot(-2 * (q.x * q.z - q.w * q.y),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            2 * (q.x * q.y + q.w * q.z),
            -2 * (q.y * q.z - q.w * q.x),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            res);
        break;

    case yzy:
        twoaxisrot(2 * (q.y * q.z + q.w * q.x),
            -2 * (q.x * q.y - q.w * q.z),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            2 * (q.y * q.z - q.w * q.x),
            2 * (q.x * q.y + q.w * q.z),
            res);
        break;

    case xyz:
        threeaxisrot(-2 * (q.y * q.z - q.w * q.x),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            2 * (q.x * q.z + q.w * q.y),
            -2 * (q.x * q.y - q.w * q.z),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            res);
        break;

    case xyx:
        twoaxisrot(2 * (q.x * q.y + q.w * q.z),
            -2 * (q.x * q.z - q.w * q.y),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            2 * (q.x * q.y - q.w * q.z),
            2 * (q.x * q.z + q.w * q.y),
            res);
        break;

    case xzy:
        threeaxisrot(2 * (q.y * q.z + q.w * q.x),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            -2 * (q.x * q.y - q.w * q.z),
            2 * (q.x * q.z + q.w * q.y),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            res);
        break;

    case xzx:
        twoaxisrot(2 * (q.x * q.z - q.w * q.y),
            2 * (q.x * q.y + q.w * q.z),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            2 * (q.x * q.z + q.w * q.y),
            -2 * (q.x * q.y - q.w * q.z),
            res);
        break;
    default:
        assert(false);
        break;
    }
}

glm::vec3 quaternion2Euler(const glm::fquat& q, RotSeq rs)
{
    // zyx {2,1,0},

    const auto& map = mappings[(int)rs];
    double buf[3];
    quaternion2Euler(q, buf, rs);
    glm::vec3 res;
    res[map[0]] = buf[0];
    res[map[1]] = buf[1];
    res[map[2]] = buf[2];
    return res;
}