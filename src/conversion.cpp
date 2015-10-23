#include "conversion.hpp"

dake::math::vec3 conversion::fromEigenToDake(const Eigen::Matrix<double, 3, 1> &vector) {
	dake::math::vec3 result;
	result.x() = vector(0);
    result.y() = vector(1);
    result.z() = vector(2);
    return result;
}

dake::math::vec<3, double> conversion::fromEigenToDakeDouble(const Eigen::Matrix<double, 3, 1> &vector) {
	dake::math::vec<3, double> result;
	result.x() = vector(0);
    result.y() = vector(1);
    result.z() = vector(2);
    return result;
}

Eigen::Matrix<double, 3, 1> conversion::fromDakeToEigen(const dake::math::vec3 &vector) {
	Eigen::Matrix<double, 3, 1> result;
	result(0) = vector.x();
	result(1) = vector.y();
	result(2) = vector.z();
	return result;
}

Eigen::Matrix<double, 3, 1> conversion::fromDakeDoubleToEigen(const dake::math::vec<3, double> &vector) {
	Eigen::Matrix<double, 3, 1> result;
	result(0) = vector.x();
	result(1) = vector.y();
	result(2) = vector.z();
	return result;
}
