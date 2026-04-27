#include "gcl/bindings/animation.h"

namespace GCL::Bindings {

Animation::Animation(granny_animation* data)
    : m_data(data)
    , m_node(nullptr)
{
}

granny_animation* Animation::getData()
{
    return m_data;
}

vector<Track::SharedPtr> Animation::getTracks()
{
    return m_tracks;
}

void Animation::addTrack(Track::SharedPtr track)
{
    m_tracks.push_back(track);
}

} // namespace GCL::Bindings
