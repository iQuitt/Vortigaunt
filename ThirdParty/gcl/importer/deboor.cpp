#include "gcl/importer/deboor.h"

namespace GCL::Importer {

// De Boor's algorithm to evaluate a B-spline

vector<float> padded_knots(const vector<float>& knots, unsigned degree)
{
    vector<float> v;

    v.push_back(0);

    for (auto t : knots)
        v.push_back(t);

    for (unsigned i = 1; i <= degree; ++i) {
        v[i] = 0;
        v.push_back(knots.back());
    }

    return v;
}

FbxDouble3 de_boor_position(unsigned degree, float time, const vector<float>& knots, const vector<FbxDouble3>& controls)
{
    auto i = degree;

    while (i < knots.size() - degree - 1 && knots[i] <= time) {
        ++i;
    }

    i = i - 1;

    vector<FbxDouble3> d;

    for (unsigned j = 0; j <= degree; ++j) {
        d.push_back(controls[j + i - degree]);
    }

    for (unsigned r = 1; r <= degree; ++r) {
        for (unsigned j = degree; j >= r; --j) {
            auto alpha = 0.0f;

            if (knots[j + 1 + i - r] != knots[j + i - degree]) {
                alpha = (time - knots[j + i - degree]) / (knots[j + 1 + i - r] - knots[j + i - degree]);
            }

            d[j][0] = d[j - 1][0] * (1 - alpha) + d[j][0] * alpha;
            d[j][1] = d[j - 1][1] * (1 - alpha) + d[j][1] * alpha;
            d[j][2] = d[j - 1][2] * (1 - alpha) + d[j][2] * alpha;
        }
    }

    return d[degree];
}

FbxDouble3 de_boor_position(unsigned degree, float time, vector<float>& knots, vector<FbxDouble3>& controls)
{
    if (controls.size() == 0) {
        return FbxDouble3();
    }

    if (knots.size() == 1 || knots.size() < degree + 1)
        return controls[0];

    return de_boor_position(degree, time, padded_knots(knots, degree), controls);
}

FbxQuaternion de_boor_rotation(unsigned degree, float time, const vector<float>& knots, const vector<FbxQuaternion>& controls)
{
    unsigned i = degree;

    while (i < knots.size() - degree - 1 && knots[i] <= time) {
        ++i;
    }

    i = i - 1;

    vector<FbxQuaternion> points;

    for (unsigned j = 0; j <= degree; ++j) {
        points.push_back(controls[j + i - degree]);
    }

    for (unsigned r = 1; r <= degree; ++r) {
        for (unsigned j = degree; j >= r; --j) {
            auto alpha = 1.0f;

            if (knots[j + 1 + i - r] != knots[j + i - degree]) {
                alpha = (time - knots[j + i - degree]) / (knots[j + 1 + i - r] - knots[j + i - degree]);
            }

            points[j] = points[j - 1].Slerp(points[j], alpha);
        }
    }

    return points[degree];
}

FbxQuaternion de_boor_rotation(unsigned degree, float time, vector<float>& knots, vector<FbxQuaternion>& controls)
{
    if (controls.size() == 0) {
        return FbxQuaternion();
    }

    if (knots.size() == 1 || knots.size() < degree + 1) {
        return controls[0];
    }

    return de_boor_rotation(degree, time, padded_knots(knots, degree), controls);
}

} // namespace GCL::Importer
