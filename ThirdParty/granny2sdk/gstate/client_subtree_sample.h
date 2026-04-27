#if !defined(CLIENT_SUBTREE_SAMPLE_H)
#include "gstate_header_prefix.h"
// ========================================================================
// $Notice: $
// ========================================================================

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

// 

class subtree_sample : public node
{
    // required parent typedef
    typedef node parent;

    // Node interface details
public:
    // Allows us to intercept the AddOutput/DeleteOutput (and *Input) functions
    DECL_IO_MANIPULATION_INTERFACE();

    // We need to intercept this, since it affects our output...
    virtual bool   SetInputName(granny_int32x InputIdx, char const* NewEdgeName);

    
    // This is a node that yields a local_pose result
    DECL_SAMPLEPOSE_INTERFACE();

    // Marks the node as a leaf class, i.e, one that will be instantiated in the character
    // instance, rather than an interface
    DECL_CONCRETE_NODE_TOKEN(subtree_sample);

    // So we can map our input names to specific model bones
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();
    
    // Our interface details
public:
    bool InputNameValid(granny_int32x InputIdx);

    // And our storage for the replacement bitfields
private:
    granny_int32   m_BitfieldCount; // NumInputs - 1 after binding
    granny_uint8** m_Bitfields;
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define CLIENT_SUBTREE_SAMPLE_H
#endif /* CLIENT_SUBTREE_SAMPLE_H */3
