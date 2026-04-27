// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_SCALAR_COMPARE_H)
#include "gstate_header_prefix.h"

#if !defined(GSTATE_CONDITIONAL_H)
#include "gstate_conditional.h"
#endif

BEGIN_GSTATE_NAMESPACE;

// May *not* change
enum scalar_compare_op
{
    ScalarCompare_Less     = 0,
    ScalarCompare_Greater  = 1,
    ScalarCompare_LEqual   = 2,
    ScalarCompare_GEqual   = 3,
    ScalarCompare_Equal    = 4,
    ScalarCompare_NotEqual = 5
};

class scalar_compare : public conditional
{
    typedef conditional parent;

public:
    virtual conditional_type GetType();

    // Awkwardly named.  Means "All must be true" if set, "Any may be true" if clear.
    // That is, (&& or ||)
    bool IsAndCondition();
    void SetIsAndCondition(bool AndCondition);

    int  GetNumCompares();
    bool AddCompare();
    bool RemoveCompare(granny_int32x CompareIndex);

    bool GetCompareSpec(granny_int32x      CompareIndex,
                        node**             OutputNode,
                        granny_int32x*     OutputIdx,
                        scalar_compare_op* Operation,
                        granny_real32*     ReferenceValue);

    bool SetCompareSpec(granny_int32x     CompareIndex,
                        node*             OutputNode,    // null for parent
                        granny_int32x     OutputIdx,
                        scalar_compare_op Operation,
                        granny_real32     ReferenceValue);

    virtual void Note_OutputEdgeDelete(node*         AffectedNode,
                                       granny_int32x RemovedOutput);
    virtual void Note_NodeDelete(node* DeletedNode);

    DECL_CONCRETE_CLASS_TOKEN(scalar_compare);
protected:
    node** m_OutputNodes;
    
    virtual bool IsTrueImpl(granny_real32 AtT, granny_real32 DeltaT);

    virtual bool CaptureNodeLinks();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_SCALAR_COMPARE_H
#endif /* GSTATE_SCALAR_COMPARE_H */
