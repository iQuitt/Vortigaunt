// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_token_context.h"

#include "gstate_tokenized.h"

#include "gstate_snapshotutils.h"
#include <string.h>

// Concrete products
// - nodes
#include "gstate_node_header_list.h"

// - conditionals
#include "gstate_event_triggered.h"
#include "gstate_parent_condition.h"
#include "gstate_scalar_compare.h"
#include "gstate_nodestate_condition.h"

// - transitions
#include "gstate_transition_lastresort.h"
#include "gstate_transition_onconditional.h"
#include "gstate_transition_onloop.h"
#include "gstate_transition_onrequest.h"
#include "gstate_transition_onsubloop.h"
#include "gstate_transition_dynamic.h"


#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

typedef granny_pointer_hash TokenMap;
typedef granny_pointer_hash UIDMap;

#define UID_AS_PTR(Product) ((void*)(granny_uintaddrx)Product->GetUID())

// Used to hang onto creation functions until first use.  Prevents allocations before the
// main function.
static creator_storage* s_InitChain = 0;


struct GSTATE
token_context::token_contextImpl
{
    TokenMap*    tokenMapping;
    UIDMap*      uidMapping;

    granny_uint32 NextUID;

    // Disallowed
    token_contextImpl();

    token_contextImpl(granny_int32x NumTokenized)
        : tokenMapping(GrannyNewPointerHashWithSize(NumTokenized)),
          uidMapping(GrannyNewPointerHashWithSize(NumTokenized)),
          NextUID(1)
    {
        //
    }
    ~token_contextImpl()
    {
        GrannyDeletePointerHash(tokenMapping);
        tokenMapping = 0;

        GrannyDeletePointerHash(uidMapping);
        uidMapping = 0;
    }
};

token_context* g_GlobalContext = 0;


// These are simple linear lists of construction functions.  We don't need these to be fast.
struct FactoryEntry   { char const* TokenName;     token_creation_fn* CreationFn;     };
struct CTorEntry      { char const* ClassName;     default_ctor_fn* CTorFn;           };
struct TokenTypeEntry { char const* TypeIndicator; granny_data_type_definition* Type; };

// This is not ideal, but it prevents allocations from hanging around after they are
// required.  The thinking here is that the number of classes that need to be mapped in
// this fashion is always known a priori, so this constant can be adjusted as required.
// The assert below will fire in ALL builds, including final shipping builds, so do please
// make sure the size is correct.

static int const s_FactoryArraySize = 64;

static int s_ValidFactoryEntries = 0;
static FactoryEntry   s_FactoryArray[s_FactoryArraySize]   = { { 0 } };
static CTorEntry      s_CTorArray[s_FactoryArraySize]      = { { 0 } };
static TokenTypeEntry s_TokenTypeArray[s_FactoryArraySize] = { { 0 } };

static void
RegisterFactories()
{
    if (s_ValidFactoryEntries != 0)
        return;

    // @@ Lock

    // Another thread may have set this up while we were locked
    if (s_InitChain)
    {
        GStateAssert(s_ValidFactoryEntries == 0);

        while (s_InitChain)
        {
            GStateAssert(s_ValidFactoryEntries < s_FactoryArraySize);
            s_FactoryArray[s_ValidFactoryEntries].TokenName  = s_InitChain->TokenName;
            s_FactoryArray[s_ValidFactoryEntries].CreationFn = s_InitChain->CreationFn;

            s_CTorArray[s_ValidFactoryEntries].ClassName = s_InitChain->ClassName;
            s_CTorArray[s_ValidFactoryEntries].CTorFn    = s_InitChain->CtorFn;

            s_TokenTypeArray[s_ValidFactoryEntries].TypeIndicator = s_InitChain->TokenType[0].Name;
            s_TokenTypeArray[s_ValidFactoryEntries].Type          = s_InitChain->TokenType;

            s_InitChain = s_InitChain->Next;
            ++s_ValidFactoryEntries;
        }

        // @@ todo: sort for binsearch

        s_InitChain = 0;
    }

    // @@ unlock

    GStateAssert(s_ValidFactoryEntries != 0);
    GStateAssert(s_InitChain == 0);
}

static token_creation_fn*
GetFactoryEntry(char const* TokenName)
{
    RegisterFactories();

    {for (int Idx = 0; Idx < s_ValidFactoryEntries; ++Idx)
    {
        if (strcmp(s_FactoryArray[Idx].TokenName, TokenName) == 0)
            return s_FactoryArray[Idx].CreationFn;
    }}

    return 0;
}

static default_ctor_fn*
GetCTorEntry(char const* ClassName)
{
    RegisterFactories();

    // @@ todo: binsearch
    {for (int Idx = 0; Idx < s_ValidFactoryEntries; ++Idx)
    {
        if (strcmp(s_CTorArray[Idx].ClassName, ClassName) == 0)
            return s_CTorArray[Idx].CTorFn;
    }}

    return 0;
}

static granny_data_type_definition*
GetTypeEntry(char const* TypeIndicator)
{
    RegisterFactories();

    {for (int Idx = 0; Idx < s_ValidFactoryEntries; ++Idx)
    {
        if (strcmp(s_TokenTypeArray[Idx].TypeIndicator, TypeIndicator) == 0)
            return s_TokenTypeArray[Idx].Type;
    }}

    return 0;
}


GSTATE
token_context::token_context()
  : m_Impl(GStateNew<token_contextImpl>(0))
{

}

GSTATE
token_context::token_context(granny_int32x NumTokenized)
  : m_Impl(GStateNew<token_contextImpl>(NumTokenized))
{

}

GSTATE
token_context::~token_context()
{
    // TODO: Free all the nodes?
    // TODO: assert empty?

    GStateDelete<token_contextImpl>(m_Impl);
    m_Impl = 0;
}

GSTATE token_context*
token_context::GetGlobalContext()
{
    if (g_GlobalContext == 0)
    {
        g_GlobalContext = GStateNew<token_context>();
    }

    return g_GlobalContext;
}

void GSTATE
token_context::DestroyGlobalContext()
{
    if (g_GlobalContext)
    {
        GStateDelete<token_context>(g_GlobalContext);
        g_GlobalContext = 0;
    }
}

granny_uint32 GSTATE
token_context::GetContextUID()
{
    return m_Impl->NextUID++;
}

tokenized* GSTATE
token_context::GetProductForUID(granny_uint32 UID)
{
    if (UID == 0)
        return 0;

    void* Result = 0;
    GrannyGetHashedPointerData(m_Impl->uidMapping, (void*)(granny_uintaddrx)UID, &Result);

    return (tokenized*)Result;
}

tokenized* GSTATE
token_context::GetProductForToken(void* Token)
{
    if (Token == 0)
        return 0;

    void* Result = 0;
    GrannyGetHashedPointerData(m_Impl->tokenMapping, Token, &Result);

    return (tokenized*)Result;
}

void GSTATE
token_context::SetProductForToken(void*         Token,
                                  tokenized*    Product)
{
    GStateAssert(Token != 0);

    TokenMap* tokenMap = m_Impl->tokenMapping;
    UIDMap*     uidMap = m_Impl->uidMapping;
    if (Product == 0)
    {
        void* Current = 0;
        if (GrannyGetHashedPointerData(tokenMap, Token, &Current))
        {
            tokenized* Tokenized = (tokenized*)Current;
            GrannyRemovePointerFromHash(tokenMap, Token);
            GrannyRemovePointerFromHash(uidMap, UID_AS_PTR(Tokenized));
        }
    }
    else
    {
        // Request to set...
        GStateAssert(!GrannyHashedPointerKeyExists(tokenMap, Token));
        GStateAssert(!GrannyHashedPointerKeyExists(uidMap, UID_AS_PTR(Product)));

        GrannySetHashedPointerData(tokenMap, Token, Product);
        GrannySetHashedPointerData(uidMap, UID_AS_PTR(Product), Product);
    }
}

tokenized* GSTATE
token_context::CreateFromToken(granny_data_type_definition* TokenType,
                               void*                        TokenObject)
{
    GStateAssert(TokenType && TokenObject);
    GStateAssert(GetProductForToken(TokenObject) == 0);

    tokenized* Result = 0;

    token_creation_fn* Fn = GetFactoryEntry(TokenType[0].Name);
    if (Fn == 0)
    {
        GStateError("Unknown class in CreateFromToken: %s\n", TokenType[0].Name);
        return 0;
    }

    // Special case.  If this == &g_GlobalContext, pass null to the context parameter
    token_context* UseContext = this;
    if (UseContext == g_GlobalContext)
        UseContext = 0;

    Result = (*Fn)(UseContext, TokenType, TokenObject);

    // Ensure that our NextUID won't overlap...
    {
        if (Result->GetUID() >= m_Impl->NextUID)
        {
            m_Impl->NextUID = Result->GetUID() + 1;
        }
    }

    return Result;
}

tokenized* GSTATE
token_context::CreateFromClassName(char const* ClassName)
{
    GStateAssert(ClassName);

    default_ctor_fn* Fn = GetCTorEntry(ClassName);
    if (Fn == 0)
    {
        GStateAssert("Unknown class");
        return 0;
    }

    tokenized* Result = (*Fn)();
    return Result;
}


void GSTATE
token_context::RegisterTokenCreator(creator_storage* Storage)
{
    Storage->Next = s_InitChain;
    s_InitChain = Storage;
}

granny_data_type_definition const*
token_context::UpdateVariantHandler(granny_data_type_definition const* Type,
                                    void const* Object)
{
    if (Type == 0)
        return 0;

    // Search the registration list for a match
    return GetTypeEntry(Type[0].Name);
}

bool GSTATE
token_context::CaptureStateSnapshot(granny_file_writer* Writer)
{
    GStateAssert(Writer);

    granny_pointer_hash_iterator* Itr = GrannyPointerHashBegin(m_Impl->uidMapping);
    while (GrannyPointerHashIteratorValid(m_Impl->uidMapping, Itr))
    {
        granny_uint32 ID = (granny_uint32)(granny_uintaddrx)GrannyPointerHashIteratorKey(Itr);
        tokenized* Tokenized = (tokenized*)GrannyPointerHashIteratorData(Itr);
        {
            granny_int32 UIDPos  = GrannyGetWriterPosition(Writer);

            GStateAssert(ID == Tokenized->GetUID());
            CREATE_WRITE_DWORD(ID);

            granny_int32 SizePos = GrannyGetWriterPosition(Writer);

            // Placeholder for the size
            granny_uint32 SnapshotSize = 0;
            CREATE_WRITE_DWORD(SnapshotSize);

            // Write the snapshot
            granny_int32 SnapStart  = GrannyGetWriterPosition(Writer);
            bool const   SnapResult = Tokenized->CreateSnapshot(Writer);
            granny_int32 SnapEnd    = GrannyGetWriterPosition(Writer);

            if (SnapResult == false || SnapEnd == SnapStart)
            {
                // This happens when the class has NO snapshot.  Rewind the stream to the
                // UID variable, and leave the size set to zero.  Note that this can leave
                // a spurious zero at the very *end* of the stream, which we handle
                // below...
                GrannySeekWriterFromStart(Writer, UIDPos);
            }
            else
            {
                // Fill in the size variable
                GrannySeekWriterFromStart(Writer, SizePos);
                SnapshotSize = SnapEnd - SnapStart;
                CREATE_WRITE_DWORD(SnapshotSize);

                GrannySeekWriterFromStart(Writer, SnapEnd);
            }
        }
        GrannyPointerHashIteratorNext(m_Impl->uidMapping, Itr);
    }
    GrannyDeletePointerHashIterator(m_Impl->uidMapping, Itr);

    return true;
}

bool GSTATE
token_context::ResetFromStateSnapshot(granny_file_reader* Reader)
{
    GStateAssert(Reader);

    granny_int32 TotalSize;
    GrannyGetReaderSize(Reader, &TotalSize);
    granny_int32 const StartPosition = 0;
    granny_int32 ReaderOffset = 0;
    while (ReaderOffset < TotalSize)
    {
        granny_uint32 UID;
        RESET_READ_DWORD(UID);

        granny_uint32 SnapshotSize;
        RESET_READ_DWORD(SnapshotSize);

        // Note that in the case that UID is at the *end* of the iteration, and is a class
        // with no snapshot entry, snapshot size might be 0.  Do all the work after we
        // know that there is something there.  Could assert that if (SnapshotSize == 0),
        // the stream is at its end.
        if (SnapshotSize != 0)
        {
            tokenized* Tokenized = GetProductForUID(UID);
            if (Tokenized == 0)
                return false;

            if (!Tokenized->ResetFromSnapshot(Reader, ReaderOffset))
                return false;
        }

        ReaderOffset += SnapshotSize;
    }

    return true;
}

granny_int32x GSTATE
token_context::UniqueCount()
{
    granny_int32x NumUnique = 0;

    granny_pointer_hash_iterator* Itr = GrannyPointerHashBegin(m_Impl->tokenMapping);
    while (GrannyPointerHashIteratorValid(m_Impl->tokenMapping, Itr))
    {
        ++NumUnique;
        GrannyPointerHashIteratorNext(m_Impl->tokenMapping, Itr);
    }
    GrannyDeletePointerHashIterator(m_Impl->tokenMapping, Itr);

    return NumUnique;
}


#define TAKE_ACTION_NODE(x)

#define TAKE_ACTION(x) REGISTER_CREATION(x);
#include "gstate_node_type_list.h"
#undef TAKE_ACTION
#undef TAKE_ACTION_NODE

REGISTER_CREATION(tr_lastresort);
REGISTER_CREATION(tr_onrequest);
REGISTER_CREATION(tr_onloop);
REGISTER_CREATION(tr_onsubloop);
REGISTER_CREATION(tr_onconditional);
REGISTER_CREATION(tr_dynamic);

REGISTER_CREATION(scalar_compare);
REGISTER_CREATION(event_triggered);
REGISTER_CREATION(parent_condition);
REGISTER_CREATION(nodestate_condition);

#include "gstate_user_register_create.inl"  // for user registrations, don't remove
