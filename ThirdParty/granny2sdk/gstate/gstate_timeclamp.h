// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE__H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class timeclamp : public node
{
    typedef node parent;

public:
    bool GetRangeRelative() const;
    void SetRangeRelative(bool Relative);
    
    DECL_IO_MANIPULATION_INTERFACE();
    DECL_SAMPLEPOSE_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(timeclamp);

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    virtual void AddInputsForOutput(granny_int32x OutputIndex);
    virtual void DeleteInputsForOutput(granny_int32x OutputIndex);
    virtual void SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName);

private:
    granny_real32 ComputeSampleT(granny_real32 const AtT,
                                 node*               PoseNode,
                                 granny_int32x       ConnectedEdge);
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TIMECLAMP_H
#endif /* GSTATE_TIMECLAMP_H */
