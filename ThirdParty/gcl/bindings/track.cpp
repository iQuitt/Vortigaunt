#include "gcl/bindings/track.h"

namespace GCL::Bindings {

Track::Track(granny_transform_track data)
    : m_data(data)
    , m_node(nullptr)
{
}

string Track::getName()
{
    return m_name;
}

void Track::setName(string name)
{
    m_name = name;
}

vector<CurvePositionKey> Track::getPositionKeys()
{
    return m_positionKeys;
}

vector<CurveRotationKey> Track::getRotationKeys()
{
    return m_rotationKeys;
}

vector<CurveScaleKey> Track::getScaleKeys()
{
    return m_scaleKeys;
}

void Track::addPositionKey(CurvePositionKey key)
{
    m_positionKeys.push_back(key);
}

void Track::addRotationKey(CurveRotationKey key)
{
    m_rotationKeys.push_back(key);
}

void Track::addScaleKey(CurveScaleKey key)
{
    m_scaleKeys.push_back(key);
}

} // namespace GCL::Bindings
