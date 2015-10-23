#pragma once

#include "hacks/EigenFixup.h"
#include "eigen/Dense"

#include <dake/math/matrix.hpp>

namespace conversion {
	dake::math::vec3 fromEigenToDake(const Eigen::Matrix<double, 3, 1> &vector);
	dake::math::vec<3, double> fromEigenToDakeDouble(const Eigen::Matrix<double, 3, 1> &vector);
	Eigen::Matrix<double, 3, 1> fromDakeToEigen(const dake::math::vec3 &vector);
	Eigen::Matrix<double, 3, 1> fromDakeDoubleToEigen(const dake::math::vec<3, double> &vector);
}
