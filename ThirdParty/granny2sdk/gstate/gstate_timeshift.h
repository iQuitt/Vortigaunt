// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TIMESHIFT_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class timeshift : public node
{
    typedef node parent;

public:
    bool GetOffsetRelative() const;
    void SetOffsetRelative(bool Relative);
    
    // All of the sample interfaces based on t key off the connected pose edge.  They will
    // *fail* unless that edge is connected.
    DECL_SAMPLEPOSE_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();
    DECL_IO_MANIPULATION_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(timeshift);
    DECL_SNAPPABLE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);
public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
    virtual void Activate(granny_real32 AtT);

public:
    void GetScaleOffset(granny_real32 AtT,
                        granny_real32& Scale,
                        granny_real32& Offset);
    granny_real32 ComputeSampleT(granny_real32 const AtT,
                                 granny_real32 const Scale,
                                 granny_real32 const Offset,
                                 node*               PoseNode,
                                 granny_int32x       ConnectedEdge);

    virtual void AddInputsForOutput(granny_int32x OutputIndex);
    virtual void DeleteInputsForOutput(granny_int32x OutputIndex);
    virtual void SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName);

private:

    granny_real32 m_LocalOffset;
    granny_real32 m_LastObservedScale;
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TIMESHIFT_H
#endif /* GSTATE_TIMESHIFT_H */
