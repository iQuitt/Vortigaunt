// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TOKEN_CONTEXT_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class tokenized;
class token_context;

typedef tokenized* token_creation_fn(token_context*               InContext,
                                     granny_data_type_definition* TokenType,
                                     void*                        TokenObject);
typedef tokenized* default_ctor_fn();

struct creator_storage
{
    char const* ClassName;
    char const* TokenName;

    token_creation_fn* CreationFn;
    default_ctor_fn*   CtorFn;

    granny_data_type_definition* TokenType;

    creator_storage*   Next;
};

class token_context
{
public:
    token_context();
    token_context(granny_int32x NumTokenized);
    ~token_context();

    // Only use these if you *know* what you're doing.  It's for the editing functionality.
    static token_context* GetGlobalContext();
    static void DestroyGlobalContext();

    // Note that these are static because we want to be able to use Context==0 to refer to
    // the global (editable) context.
    tokenized* GetProductForToken(void* Token);
    void       SetProductForToken(void* Token, tokenized*);

    granny_uint32 GetContextUID();
    tokenized*    GetProductForUID(granny_uint32 UID);

    // In context may be NULL for creating objects in the global context
    tokenized* CreateFromToken(granny_data_type_definition* TokenType,
                               void*                        TokenObject);

    static tokenized* CreateFromClassName(char const* ClassName);
    static void RegisterTokenCreator(creator_storage* Storage);

    static granny_data_type_definition const* UpdateVariantHandler(granny_data_type_definition const* Type,
                                                                   void const* Object);

    // For capturing and restoring context state.  Note that the state token is *only*
    // valid for the current state of the object/token mapping.  So for instance, if you
    // alter a transition linkage, or change the underlying animations, or otherwise alter
    // the bit pattern that would be saved if you dumped the root token, this will be
    // invalid.
    bool CaptureStateSnapshot(granny_file_writer* Writer);
    bool ResetFromStateSnapshot(granny_file_reader* Reader);

    granny_int32x UniqueCount();

    struct token_contextImpl;
private:
    token_contextImpl* m_Impl;

    token_context(token_context const&);
    token_context& operator=(token_context const&);
};



// Helper macro
#define REGISTER_CREATION(Typename)                                                                 \
    static tokenized*                                                                               \
    Create_ ## Typename(token_context*               InContext,                                     \
                        granny_data_type_definition* TokenType,                                     \
                        void*                        TokenObject)                                   \
    {                                                                                               \
        /* custom, to handle the 3 arg constructor. */                                              \
        Typename* Ptr = GStateAllocStruct(Typename);                                                \
        if (Ptr)                                                                                    \
        {                                                                                           \
            GStateAssert(GrannyDataTypesAreEqualWithNames(TokenType,                                \
                                                          (Typename :: Typename ## TokenType)));    \
            /* Note that we use the internal version of the token type, since the variant that */   \
            /* spawns this object might be going away on FreeFileSection */                         \
            new (Ptr) Typename(InContext, (Typename :: Typename ## TokenType),                      \
                               TokenObject, tokenized::eNotOwned2);                                 \
        }                                                                                           \
        return Ptr;                                                                                 \
    }                                                                                               \
    creator_storage Stor ## Typename = {                                                            \
        #Typename,                                                                                  \
        "ContextUID_" #Typename,                                                                    \
        Create_ ## Typename,                                                                        \
        Typename :: DefaultAsTokenized,                                                             \
        Typename :: Typename ## TokenType,                                                          \
        0                                                                                           \
    };                                                                                              \
    struct Register_ ## Typename {                                                                  \
        Register_ ## Typename ()                                                                    \
        {                                                                                           \
            token_context::RegisterTokenCreator(&(Stor ## Typename));                               \
        }                                                                                           \
    } theRegister_ ## Typename

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TOKEN_CONTEXT_H
#endif /* GSTATE_TOKEN_CONTEXT_H */
