// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_tokenized.h"

#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

//const granny_uint32 s_InvalidUID = granny_uint32(-1);

GSTATE
tokenized::tokenized(token_context*               Context,
                     granny_data_type_definition* TokenType,            
                     void*                        TokenObject,                                 
                     token_ownership              TokenIsOwned)
  : m_RootTokenContext(Context)
{
    m_RootTokenType   = TokenType;
    m_RootToken       = TokenObject;
    m_RootTokenOwned  = TokenIsOwned == eOwned2;
    m_TokenSlotsOwned = TokenIsOwned == eOwned2;

    // The context UID is actually the first member of the root token type, assuming
    // everything is going correctly...
    GStateAssert(strncmp(TokenType[0].Name, "ContextUID", 10) == 0);
    GStateAssert(TokenType[0].Type == GrannyUInt32Member);

    GStateAssert(GetTokenContext()->GetProductForToken(TokenObject) == 0);
    GetTokenContext()->SetProductForToken(TokenObject, this);
}


GSTATE
tokenized::~tokenized()
{
    if (m_RootToken)
    {
        GStateAssert(GetTokenContext()->GetProductForToken(m_RootToken) == this);
        GetTokenContext()->SetProductForToken(m_RootToken, 0);
    }

    if (m_RootTokenOwned)
        GStateDeallocate(m_RootToken);

    m_RootTokenType    = 0;
    m_RootToken        = 0;
    m_RootTokenOwned   = false;
    m_TokenSlotsOwned  = false;
    m_RootTokenContext = 0;
}


bool GSTATE
tokenized::GetTypeAndToken(granny_variant* TypeAndToken)
{
    GStateAssert(TypeAndToken);

    TypeAndToken->Type   = m_RootTokenType;
    TypeAndToken->Object = m_RootToken;
    return (m_RootToken != 0);
}

token_context* GSTATE
tokenized::GetTokenContext()
{
    if (m_RootTokenContext == 0)
        return token_context::GetGlobalContext();

    return m_RootTokenContext;
}

granny_uint32 GSTATE
tokenized::GetUID() const
{
    //GStateAssert(IsValid());

    return *((granny_uint32*)m_RootToken);
}

CREATE_SNAPSHOT(tokenized)
{
    return true;
}

RESET_FROMSNAPSHOT(tokenized)
{
    return true;
}


void GSTATE
tokenized::TakeTokenOwnership()
{
    // Ignore, except to mark the slots as owned.  Should never reach this point unless
    // the slots are *NOT* owned.

    GStateAssert(IsEditable());
    GStateAssert(m_TokenSlotsOwned == false);
    m_TokenSlotsOwned = true;
}

bool GSTATE
tokenized::FillDefaultToken(granny_data_type_definition* TokenType,
                            void* TokenObject)
{
    GStateAssert(strncmp(TokenType[0].Name, "ContextUID", 10) == 0);
    GStateAssert(TokenType[0].Type == GrannyUInt32Member);

    // Fill in the UID, and we're set.  Tokenized doesn't have a slot
    *((granny_uint32*)TokenObject) = token_context::GetGlobalContext()->GetContextUID();

    return true;
}

tokenized* GSTATE
tokenized::supports_interface(tokenized* p, char const* IDString)
{
    if (!p)
        return 0;
    if (!IDString || !IDString[0])
        return 0;

    granny_variant TypeAndToken;
    granny_data_type_definition* Type;
    if (!p->GetTypeAndToken(&TypeAndToken))
        return 0;
    Type  = TypeAndToken.Type;

    size_t IDStringLen = strlen(IDString);

    granny_data_type_definition* Walk = Type;
    while (Walk->Type != GrannyEndMember)
    {
        if (Walk->Type == GrannyReferenceMember)
        {
            if (strncmp(Walk->Name, IDString, IDStringLen) == 0)
            {
                // Check that it ends with "Token"
                if ((strlen(Walk->Name) == IDStringLen + 5) &&
                    strcmp((Walk->Name + IDStringLen), "Token") == 0)
                {
                    // yay!
                    return p;
                }
            }
        }

        ++Walk;
    }

    // Doesn't support that interface
    return 0;
}
