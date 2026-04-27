// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TOKENIZED_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class token_context;

// Used by Tokenized below...
#define DECL_INIT_FROM_TOKEN                                                \
    protected:                                                              \
    static bool FillDefaultToken(granny_data_type_definition* TokenType,    \
                                 void* TokenObject)

#define PREDECL_CASTABLE_INTERFACE(Typename) class Typename
#define DECL_CASTABLE_INTERFACE(Typename)                           \
    public: static Typename* as_ ## Typename (tokenized* p)         \
    {                                                               \
        if (!p)                                                     \
            return 0;                                               \
        else return p->as_ ## Typename ## _impl ();                 \
    }                                                               \
public: virtual Typename* as_ ## Typename ## _impl() { return 0; }

#define IMPL_CASTABLE_INTERFACE(Typename) public: virtual Typename* as_ ## Typename ## _impl() { return this; }

#define GSTATE_DYNCAST(p, Typename) GSTATE tokenized::as_ ## Typename(p)
#define GSTATE_SLOW_TYPE_CHECK(p, Typename) (Typename *) GSTATE tokenized::supports_interface(p, #Typename)

PREDECL_CASTABLE_INTERFACE(node);
PREDECL_CASTABLE_INTERFACE(transition);
PREDECL_CASTABLE_INTERFACE(parameters);
PREDECL_CASTABLE_INTERFACE(container);
PREDECL_CASTABLE_INTERFACE(state_machine);
PREDECL_CASTABLE_INTERFACE(blend_graph);
PREDECL_CASTABLE_INTERFACE(anim_source);
PREDECL_CASTABLE_INTERFACE(event_source);
PREDECL_CASTABLE_INTERFACE(conditional);
PREDECL_CASTABLE_INTERFACE(tr_dynamic);


// =============================================================================
// Main class for representing classes in the state library that shared immutable data
// between instances.
// =============================================================================

class tokenized
{
public:
    enum token_ownership { eOwned2, eNotOwned2 };
protected:
    granny_data_type_definition* m_RootTokenType;
    void*                        m_RootToken;
    bool                         m_RootTokenOwned;
    bool                         m_TokenSlotsOwned;

    token_context*               m_RootTokenContext;

    bool IsEditable() const { return m_RootTokenContext == 0; }

public:
    virtual ~tokenized() = 0;

    bool           GetTypeAndToken(granny_variant* TypeAndToken);
    token_context* GetTokenContext();
    granny_uint32  GetUID() const;

    virtual char const* GetName() const = 0;
    virtual bool        SetName(char const* NewName) = 0;

    virtual bool CreateSnapshot(granny_file_writer* Writer);
    virtual bool ResetFromSnapshot(granny_file_reader* Reader,
                                   granny_int32 StartPosition);

    // Replacement for dynamic_cast so we can leave RTTI off.
    DECL_CASTABLE_INTERFACE(node);
    DECL_CASTABLE_INTERFACE(transition);
    DECL_CASTABLE_INTERFACE(parameters);
    DECL_CASTABLE_INTERFACE(container);
    DECL_CASTABLE_INTERFACE(state_machine);
    DECL_CASTABLE_INTERFACE(blend_graph);
    DECL_CASTABLE_INTERFACE(anim_source);
    DECL_CASTABLE_INTERFACE(event_source);
    DECL_CASTABLE_INTERFACE(conditional);
    DECL_CASTABLE_INTERFACE(tr_dynamic);
    static tokenized* supports_interface(tokenized* p, char const* IDString);

    virtual char const* GetTypename() { GS_InvalidCodePath("GetTypename on an abstract class"); return ""; }

protected:
    tokenized(token_context*,                                    
              granny_data_type_definition* TokenType,            
              void* TokenObject,                                 
              token_ownership TokenIsOwned);

    bool EditorCreated() const { return m_RootTokenOwned; }
    virtual void TakeTokenOwnership();
    
    static bool FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject);

    GSTATE_DISALLOW_COPY(tokenized);
};


// =============================================================================
// SubClass implementation macros
// =============================================================================

// This is for subclasses, the base class just holds the pointers passed in...
#define DECL_CLASS_TOKEN_COMMON(Typename)                           \
    GSTATE_DISALLOW_COPY(Typename);                                 \
    DECL_INIT_FROM_TOKEN;                                           \
public:  struct Typename ##Impl;                                    \
public:  static granny_data_type_definition Typename ##ImplType[];  \
protected: virtual void TakeTokenOwnership();                       \
private: void ReleaseOwnedToken_ ## Typename ();                    \
public: Typename (token_context* Context,                           \
                  granny_data_type_definition* TokenType,           \
                  void* TokenObject,                                \
                  token_ownership TokenIsOwned);                    \
private: Typename ##Impl* m_ ## Typename ## Token

#define DECL_CLASS_TOKEN(Typename)              \
    DECL_CLASS_TOKEN_COMMON(Typename);          \
public: virtual ~ Typename () = 0;              \
private: Typename ()

#define DECL_CONCRETE_CLASS_TOKEN(Typename)                                     \
    protected:                                                                  \
    virtual char const* GetTypename() { return #Typename; }                     \
    DECL_CLASS_TOKEN_COMMON(Typename);                                          \
    friend class node_visitor;                                                  \
public: static Typename* DefaultInstance();                                     \
public: static tokenized* DefaultAsTokenized() { return DefaultInstance(); }    \
public: static granny_data_type_definition Typename ## TokenType[];             \
public: virtual ~ Typename ()

#define DECL_CONCRETE_NODE_TOKEN(Typename) \
    DECL_CONCRETE_CLASS_TOKEN(Typename); \
public: virtual void AcceptNodeVisitor(node_visitor*)

#define DECL_SNAPPABLE()                                                    \
    public: virtual bool CreateSnapshot(granny_file_writer* Writer);        \
public: virtual bool ResetFromSnapshot(granny_file_reader* Reader,          \
                                       granny_int32        StartPosition)

#define DEFAULT_TAKE_TOKENOWNERSHIP(Typename)       \
    void GSTATE                                     \
    Typename :: TakeTokenOwnership()                \
    {                                               \
        TAKE_TOKEN_OWNERSHIP(Typename);             \
        OldToken = OldToken;                        \
    }                                               \
                                                    \
    void GSTATE                                     \
    Typename ::ReleaseOwnedToken_ ## Typename ()    \
    {                                               \
    } typedef int __require_semi


// Handy macro for constructing concrete slotted tokens
#define DECL_UID() granny_uint32 ContextUID

#define DECL_OPAQUE_TOKEN_SLOT(Typename) void*                       Typename ## Token
#define DECL_TOKEN_SLOT(Typename)        Typename::Typename ## Impl* Typename ## Token

#define DECL_UID_MEMBER(Typename)      { GrannyUInt32Member, "ContextUID_" #Typename }
#define DECL_TOKEN_MEMBER(Typename)      { GrannyReferenceMember, #Typename "Token", Typename::Typename ## ImplType }

#define IMPL_INIT_FROM_TOKEN(Typename)                                 \
    {                                                                       \
        GET_ROOT_TOKEN_SLOT(Typename); /* todo: version? */                 \
        GStateAssert(Slot != 0);        /* should not be empty */           \
        m_ ## Typename ## Token     = Slot;                                 \
    } typedef int __require_semi

#define DTOR_RELEASE_TOKEN(Typename)                \
    if (m_TokenSlotsOwned)                          \
    {                                               \
        ReleaseOwnedToken_ ## Typename();           \
        GStateAssert(m_## Typename ##Token);        \
        GStateDeallocate(m_## Typename ##Token);    \
    }                                               \
    m_## Typename ##Token = 0

#define IMPL_CREATE_DEFAULT(Typename)                                       \
    Typename* GSTATE                                                        \
    Typename :: DefaultInstance()                                           \
    {                                                                       \
        Typename ## _token* Token = 0;                                      \
        GStateAllocZeroedStruct(Token);                                     \
        Typename :: FillDefaultToken(Typename ## TokenType, Token);         \
        Typename* Ptr = GStateAllocStruct(Typename);                        \
        if (Ptr)                                                            \
        {                                                                   \
            new (Ptr) Typename(0, Typename ## TokenType, Token, eOwned2);   \
        }                                                                   \
        return Ptr;                                                         \
    } typedef int __require_semi

#define IMPL_CREATE_NODE_BOILERPLATE(Typename)                              \
    Typename* GSTATE                                                        \
    Typename :: DefaultInstance()                                           \
    {                                                                       \
        Typename ## _token* Token = 0;                                      \
        GStateAllocZeroedStruct(Token);                                     \
        Typename :: FillDefaultToken(Typename ## TokenType, Token);         \
        Typename* Ptr = GStateAllocStruct(Typename);                        \
        if (Ptr)                                                            \
        {                                                                   \
            new (Ptr) Typename(0, Typename ## TokenType, Token, eOwned2);   \
        }                                                                   \
        return Ptr;                                                         \
    }                                                                       \
    void GSTATE                                                             \
    Typename :: AcceptNodeVisitor(node_visitor* Visitor)                    \
    {                                                                       \
        Visitor->VisitNode(this);                                           \
    } typedef int __require_semi


#define GET_TOKEN_SLOT_I(Typename, ttype, tobj)                                         \
    Typename ## Impl** SlotPointer = 0;                                                 \
    {                                                                                   \
        granny_variant Result;                                                          \
        if (!GrannyFindMatchingMember((ttype), (tobj), (#Typename "Token"), &Result))   \
        {                                                                               \
            GS_InvalidCodePath("should not be missing this member");                    \
        }                                                                               \
                                                                                        \
        SlotPointer = (Typename ## Impl**)Result.Object;                                \
    }                                                                                   \
    Typename ## Impl*& Slot = *SlotPointer

#define GET_TOKEN_SLOT(Typename)      GET_TOKEN_SLOT_I(Typename, TokenType, TokenObject)
#define GET_ROOT_TOKEN_SLOT(Typename) GET_TOKEN_SLOT_I(Typename, m_RootTokenType, m_RootToken)

#define TAKE_TOKEN_OWNERSHIP(Typename)                                                      \
    GStateAssert(sizeof(Typename##Impl) == GrannyGetTotalObjectSize(Typename##ImplType));   \
    if (m_TokenSlotsOwned) return;                                                          \
    parent::TakeTokenOwnership();                                                           \
    Typename##Impl* OldToken = 0;                                                           \
    do {                                                                                    \
        GET_ROOT_TOKEN_SLOT(Typename);                                                      \
        Typename##Impl* NewToken = GStateAllocStruct(Typename##Impl);                       \
        memcpy(NewToken, Slot, sizeof(Typename##Impl));                                     \
                                                                                            \
        OldToken = Slot;                                                                    \
        Slot     = NewToken;                                                                \
        m_##Typename##Token     = NewToken;                                                 \
    } while (false)


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TOKENIZED_H
#endif /* GSTATE_TOKENIZED_H */
