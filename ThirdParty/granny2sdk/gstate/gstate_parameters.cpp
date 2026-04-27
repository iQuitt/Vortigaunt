// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_parameters.h"

#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_state_machine.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"
#include "gstate_anim_utils.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <math.h>
#include <string.h>
#include <limits>
#include <float.h>


#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct parameter_spec
{
    void Init()
    {
        MinValue = 0;
        MinRandomValue = 1;
        MaxValue = 1;
        DefaultValue = 0;
        MaxSpeed = 0.0f;
        Accel = 0.0f;
        Random = 0;
        Type = Parameter_Value;
        ClampToInts = false;
        Cyclic = false;
    }

    granny_real32 MinValue;
    granny_real32 MinRandomValue;
    granny_real32 MaxValue;
    granny_real32 DefaultValue;
    granny_real32 MaxSpeed;
    granny_real32 Accel;
    granny_bool32 Random;
    granny_int32  Type;
    granny_bool32 ClampToInts;
    granny_bool32 Cyclic;
};

char const* GSTATE
NameForParameterType(parameter_type Type)
{
    switch (Type)
    {
    case Parameter_Value:    return "Value";
    case Parameter_Random:   return "Random";
    case Parameter_Timer:    return "Timer";
    default:
        GS_InvalidCodePath("Unknown parameter type");
        return "Unknown";
    }
}



struct GSTATE parameters::parametersImpl
{
    granny_int32    ParameterCount;
    parameter_spec* Parameters;

    granny_bool32   ClampToInts;
};

static granny_data_type_definition ParameterSpecType[] =
{
    { GrannyReal32Member, "MinValue" },
    { GrannyReal32Member, "MinRandomValue" },
    { GrannyReal32Member, "MaxValue" },
    { GrannyReal32Member, "DefaultValue" },
    { GrannyReal32Member, "MaxSpeed" },
    { GrannyReal32Member, "Accel" },
    { GrannyBool32Member, "Random" },
    { GrannyInt32Member,  "Type" },
    { GrannyBool32Member, "ClampToInts" },
    { GrannyBool32Member, "Cyclic" },
    { GrannyEndMember }
};

granny_data_type_definition GSTATE
parameters::parametersImplType[] =
{
    { GrannyReferenceToArrayMember, "Parameters", ParameterSpecType },
    { GrannyBool32Member,           "ClampToInts" },
    { GrannyEndMember },
};

// parameters is a concrete class, so we must create a slotted container
struct parameters_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(parameters);
};

granny_data_type_definition parameters::parametersTokenType[] =
{
    DECL_UID_MEMBER(parameters),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(parameters),

    { GrannyEndMember }
};

void GSTATE
parameters::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(parameters);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_parametersToken->Parameters,
                     OldToken->Parameters,
                     m_parametersToken->ParameterCount);
}

void GSTATE
parameters::ReleaseOwnedToken_parameters()
{
    GStateDeallocate(m_parametersToken->Parameters);
}

IMPL_CREATE_NODE_BOILERPLATE(parameters);

static float PARAMETER_UNINIT_FLAG = -FLT_MAX;
GSTATE
parameters::parameters(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_parametersToken(0),
    m_Parameters(0)
{
    IMPL_INIT_FROM_TOKEN(parameters);
    {
        //Upgrade ClampToInts on load by copying it into the individual entries. 
        if (m_parametersToken->ClampToInts)
        {
            for(int Idx=0; Idx<m_parametersToken->ParameterCount; Idx++)
            {
                m_parametersToken->Parameters[Idx].ClampToInts = true;
            }

            //Zero this out so it doesn't affect things in the future. 
            m_parametersToken->ClampToInts = false;
        }

        if (m_parametersToken->ParameterCount)
        {
            m_Parameters = GStateAllocArray(runtime_parameter, m_parametersToken->ParameterCount);
            for (int Idx = 0; Idx < m_parametersToken->ParameterCount; ++Idx)
            {
                m_Parameters[Idx].Value = m_parametersToken->Parameters[Idx].DefaultValue;
                m_Parameters[Idx].TargetValue = m_Parameters[Idx].Value;
                m_Parameters[Idx].ValueTime = PARAMETER_UNINIT_FLAG;
                m_Parameters[Idx].ActivationTime = 0;
                m_Parameters[Idx].ActualTimer = 0;
            }
        }
    }

    if (EditorCreated())
    {
        // Default is one pose output and one input
        // Don't go to parent, addoutput does real work
        AddOutput(ScalarEdge,  "Parameter");
    }
}


GSTATE
parameters::~parameters()
{
    GStateDeallocate(m_Parameters);
    DTOR_RELEASE_TOKEN(parameters);
}


bool GSTATE
parameters::FillDefaultToken(granny_data_type_definition* TokenType,
                             void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares parametersImpl*& Slot = // member
    GET_TOKEN_SLOT(parameters);

    // Our slot in this token should be empty.
    // Create a new parameters Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // These will come through in "addoutput"
    Slot->ParameterCount = 0;
    Slot->Parameters = 0;

    return true;
}

void GSTATE
parameters::SetParameter(granny_int32x OutputIdx, granny_real32 NewVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // SetParameter bypasses min/max.  todo: Yes?
    m_Parameters[OutputIdx].Value = NewVal;
    m_Parameters[OutputIdx].TargetValue = NewVal;
    m_Parameters[OutputIdx].ValueTime = PARAMETER_UNINIT_FLAG;
}

void GSTATE
parameters::RequestParameter(granny_int32x OutputIdx, granny_real32 NewVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    m_Parameters[OutputIdx].TargetValue = NewVal;
}

float GSTATE
parameters::GetRequestedParam(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    return m_Parameters[OutputIdx].TargetValue;
}

void GSTATE
parameters::SetParameterDefault(granny_int32x OutputIdx, granny_real32 NewVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].DefaultValue = NewVal;
}

void GSTATE
parameters::SetClampSliderToInts(granny_int32x OutputIdx, bool Clamp)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].ClampToInts = Clamp ? 1 : 0;
}


bool GSTATE
parameters::GetClampSliderToInts(granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    return m_parametersToken->Parameters[OutputIdx].ClampToInts != 0;
}

void GSTATE
parameters::SetCyclic(granny_int32x OutputIdx, bool Cyclic)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].Cyclic = Cyclic ? 1 : 0;
}


bool GSTATE
parameters::GetCyclic(granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    return m_parametersToken->Parameters[OutputIdx].Cyclic != 0;
}


// Input manipulation disallowed
granny_int32x GSTATE
parameters::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    return -1;
}

bool GSTATE
parameters::DeleteInput(granny_int32x InputIndex)
{
    return false;
}

granny_int32x GSTATE
parameters::AddOutput(node_edge_type EdgeType,
                      char const* EdgeName)
{
    if (EdgeType != ScalarEdge)
        return -1;

    TakeTokenOwnership();

    int NewIndex = parent::AddOutputImpl(EdgeType, EdgeName);
    GStateAssert(NewIndex == m_parametersToken->ParameterCount);
    GStateAssert(NewIndex+1 == GetNumOutputs());

    // Increase these vecs.  They are at GetNumOutputs() - 1, we want them at GetNumOutputs()
    parameter_spec& NewSpec = QVecPushNewElement(m_parametersToken->ParameterCount,
                                                 m_parametersToken->Parameters);
    NewSpec.Init();

    runtime_parameter& NewParam = QVecPushNewElementNoCount(GetNumOutputs() - 1, m_Parameters);

    GStateAssert(m_parametersToken->ParameterCount == GetNumOutputs());

    NewParam.Value = NewSpec.DefaultValue;
    NewParam.ValuePrev = NewParam.Value;
    NewParam.Speed = 0.0f;
    NewParam.TargetValue = NewParam.Value;
    NewParam.ValueTime = PARAMETER_UNINIT_FLAG;
    NewParam.ActivationTime = 0.0f;
    NewParam.ActualTimer = 0.0f;

    return NewIndex;
}

bool GSTATE
parameters::DeleteOutput(granny_int32x OutputIndex)
{
    GStateAssert(m_parametersToken->ParameterCount == GetNumOutputs());

    if (!parent::DeleteOutputImpl(OutputIndex))
        return false;

    TakeTokenOwnership();

    QVecRemoveElementNoCount(OutputIndex, m_parametersToken->ParameterCount, m_Parameters);
    QVecRemoveElement(OutputIndex, m_parametersToken->ParameterCount, m_parametersToken->Parameters);

    return true;
}

void GSTATE
parameters::Activate(granny_real32 AtT)
{
   for( int i=0; i<m_parametersToken->ParameterCount; i++ )
   {
       m_Parameters[i].ActivationTime = AtT;

       float minV = m_parametersToken->Parameters[i].MinRandomValue;
       float maxV = m_parametersToken->Parameters[i].MaxValue;
       if( minV != maxV )
       {
           granny_real32 u = RandomUnit();
           float d = maxV - minV;
           m_Parameters[i].ActualTimer = Clamp(minV, minV + d * u, maxV);
           m_Parameters[i].ValuePrev = 0.0f;
           m_Parameters[i].Speed = 0.0f;
       }
       else
       {
           m_Parameters[i].ActualTimer = maxV;
           m_Parameters[i].ValuePrev = 0.0f;
           m_Parameters[i].Speed = 0.0f;
       }

       //A little bit silly, but in the special case where someone specifies an integer upper range on a timer, 
       //buffer it above so that some time is spent at the max value. 
       //It's just more intuitive looking that way. 
       if( GetClampSliderToInts(i) )
       {
           m_Parameters[i].ActualTimer += 0.999999f;
       }
   }
}

granny_real32 GSTATE
parameters::SampleScalarOutput(granny_int32x OutputIdx,
                               granny_real32 AtT, 
                               gstate_scalar_track_entry * TrackEntry)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    float MinVal, MaxVal;
    GetScalarOutputRange(OutputIdx, &MinVal, &MaxVal);

    if( TrackEntry )
    {
        TrackEntry->TrackBoneIndex = -1;
        TrackEntry->TrackName = "";
        TrackEntry->TrackKey = 0;
        TrackEntry->Weight = 1.0f;
    }

    float CurrVal = 0.0f;
    if( GetParameterType(OutputIdx) == Parameter_Random )
    {
        float Unit = RandomUnit();
        CurrVal = (MaxVal - MinVal) * Unit;
        if( GetClampSliderToInts(OutputIdx) )
        {
            CurrVal += 0.5f;
            CurrVal = floor(CurrVal);
        }

        CurrVal = Clamp(MinVal, CurrVal, MaxVal);
        m_Parameters[OutputIdx].TargetValue = CurrVal;
    }
    else if( GetParameterType(OutputIdx) == Parameter_Timer )
    {
        //A little bit silly, but in the special case where someone specifies an integer upper range, buffer it above so that some time is spent at the max value. 
        //It's just more intuitive looking that way. 
        if( GetClampSliderToInts(OutputIdx) )
        {
            MaxVal += 0.999999f;
        }

        gstate_character_instance* Instance = GetBoundCharacter();
        if( Instance )
        {
            //Warning: this has clock wraparound issues
            float TimeDelta = Instance->CurrentTime - m_Parameters[OutputIdx].ActivationTime;

            CurrVal = m_Parameters[OutputIdx].ActualTimer - TimeDelta;


            if (CurrVal < MinVal)
            {
                if( m_parametersToken->Parameters[OutputIdx].Cyclic )
                {
                    float Overflow = MinVal - CurrVal;
                    Overflow = fmodf(Overflow,MaxVal - MinVal);
                    CurrVal = MaxVal - Overflow;

                }
                else
                {
                    CurrVal = MinVal;
                }
            }
            else if (CurrVal > MaxVal)
            {
                if( m_parametersToken->Parameters[OutputIdx].Cyclic )
                {
                    float Overflow = CurrVal - MaxVal;
                    Overflow = fmodf(Overflow,MaxVal - MinVal);
                    CurrVal = MinVal + Overflow;

                }
                else
                {
                    CurrVal = MaxVal;
                }
            }

            CurrVal = Clamp(MinVal, CurrVal, MaxVal);
            m_Parameters[OutputIdx].TargetValue = CurrVal;
        }
    }

    if( GetParameterType(OutputIdx) != Parameter_Random )
    {
        gstate_character_instance* Instance = GetBoundCharacter();
        if( Instance && m_Parameters[OutputIdx].Value != m_Parameters[OutputIdx].TargetValue )
        {
            if( m_parametersToken->Parameters[OutputIdx].MaxSpeed > 0.0f )
            {
                if( AtT == m_Parameters[OutputIdx].ValueTime )
                {
                    m_Parameters[OutputIdx].Value = Clamp(MinVal, m_Parameters[OutputIdx].Value, MaxVal);
                    CurrVal = m_Parameters[OutputIdx].Value;
                }
                else
                {
                    float dt =  Instance->DeltaT; // m_Parameters[OutputIdx].ValueTime == PARAMETER_UNINIT_FLAG ? 0.0f : AtT - m_Parameters[OutputIdx].ValueTime;
                    float dtSign = 1.0f;
                    if( dt < 0.0f )
                    {
                        dtSign = -1.0f;
                        dt = -dt;
                    }

                    float Target = m_Parameters[OutputIdx].TargetValue;
                    float Current = m_Parameters[OutputIdx].Value;
                    if( m_parametersToken->Parameters[OutputIdx].Cyclic )
                    {
                        //Say: Target: 0.1   Current: .9
                        //Way 1 = 0.1 - 0.9
                        //Way 2 = 1.0f - 0.9 + 0.1
                        //
                        //Say: Target: 0.9   Current: .1
                        //Way 1 = 0.9 - 0.1
                        //Way 2 = 0.0f - 0.1 + 1.0f - 0.9;
                        //

                        //if Way2 < Way1
                        //   Offset = 0.1
                        //   Val += Offset
                        //   Target += Offset
                        //   MinVal += Offset
                        //   MaxVal += Offset
                        //   Result = MATH
                        //   Result -= Offset
                        //   If Result < 0, Result = (real)MaxVal - Offset
                        float NormalWay = Target - Current;

                        //Just always mod the result at the end?
                        if( NormalWay < 0 )
                        {
                            float OtherWay = (MaxVal - MinVal) + NormalWay; //-Current + MaxVal - Target;

                            //float OtherWay = MaxVal - Current + Target;
                            if( fabsf(OtherWay) < fabsf(NormalWay) )
                            {
                                //MaxVal = MaxVal + OtherWay; //Add enough slop so we can fmodf to fix it later. 
                                Target = Current + OtherWay;
                            }
                        }
                        else
                        {
                            float OtherWay = (MaxVal - MinVal) - NormalWay; //-Current + MaxVal - Target;
                            if( fabsf(OtherWay) < fabsf(NormalWay) )
                            {
                                //MinVal = MinVal - OtherWay; //Add enough slop so we can fmodf to fix it later. 
                                Target = Current - OtherWay;
                            }
                        }
                    }

                    float AttemptedDist = Target - Current;
                    float AttemptedDirSign = 1.0f;
                    if( AttemptedDist < 0.0f )
                    {
                        AttemptedDirSign = -1.0f;
                        AttemptedDist = -AttemptedDist;
                    }

                    float AttemptedSpeed = 0.0f;
                    if( dt > 0.0001f )
                    {
                        AttemptedSpeed = AttemptedDist / dt;
                    }

                    if( AttemptedSpeed > m_parametersToken->Parameters[OutputIdx].MaxSpeed )
                    {
                        AttemptedSpeed = m_parametersToken->Parameters[OutputIdx].MaxSpeed;
                    }

                    float SignedSpeed = AttemptedSpeed * AttemptedDirSign;
                    float NewVal = Current + SignedSpeed * dt * dtSign;

                    if( m_parametersToken->Parameters[OutputIdx].Accel > 0)
                    {
                        float LastSpeed = m_Parameters[OutputIdx].Speed;
                        float CurrSpeed = SignedSpeed;
                        float CurrSpeedSign = 1.0f;
                        float CurrSpeedAbs = CurrSpeed;
                        if( CurrSpeed < 0 )
                        {
                            CurrSpeedSign = -1.0f;
                            CurrSpeedAbs = -CurrSpeedAbs;
                        }

                        float LastSpeedSign = 1.0f;
                        if( LastSpeed < 0 )
                            LastSpeedSign = -1.0f;
                        float LastSpeedAbs = LastSpeedSign * LastSpeed;

                        //If we're flipping speeds, just pretend the last speed was zero. Snaps a bit, but prevents wild rubberbanding. 
                        //Also makes math easier since signs are always the same. 
                        if( LastSpeedSign != CurrSpeedSign )
                        {
                            LastSpeed = 0.0f;
                            LastSpeedAbs = 0.0f;
                        }

                        if( CurrSpeedAbs > LastSpeedAbs )
                        {
                            if( LastSpeedAbs > 0 )
                            {
                                int i = 0;
                                i = i;
                            }

                            if( dt > 0.0001f )
                            {
                                float Accel = (CurrSpeedAbs - LastSpeedAbs) / dt;
                                if( Accel > m_parametersToken->Parameters[OutputIdx].Accel )
                                {
                                    Accel = m_parametersToken->Parameters[OutputIdx].Accel * CurrSpeedSign;
                                    float NewSpeed = LastSpeed + Accel * dt;
                                    SignedSpeed = NewSpeed;
                                    NewVal = m_Parameters[OutputIdx].Value + NewSpeed * dt;
                                }
                            }
                        }
                    }


                    if( m_parametersToken->Parameters[OutputIdx].Cyclic )
                    {
                        NewVal = fmodf(NewVal - MinVal + (MaxVal - MinVal),MaxVal - MinVal) + MinVal; //Extra MaxVal - MinVal is just to make sure we get positive numbers out of this, since NewVal could come in negative. 
                    }

                    if( GetClampSliderToInts(OutputIdx) )
                    {
                        CurrVal = floor(CurrVal);
                    }

                    CurrVal = Clamp(MinVal, NewVal, MaxVal);

                    m_Parameters[OutputIdx].ValuePrev = m_Parameters[OutputIdx].Value;
                    m_Parameters[OutputIdx].Speed = SignedSpeed;
                    m_Parameters[OutputIdx].Value = CurrVal;
                    m_Parameters[OutputIdx].ValueTime = AtT;
                }
            }
            else
            {
                if( GetClampSliderToInts(OutputIdx) )
                {
                    m_Parameters[OutputIdx].TargetValue = floor(m_Parameters[OutputIdx].TargetValue);
                }

                CurrVal = Clamp(MinVal, m_Parameters[OutputIdx].TargetValue, MaxVal);

                m_Parameters[OutputIdx].ValuePrev = m_Parameters[OutputIdx].Value;
                m_Parameters[OutputIdx].Speed = 0;
                m_Parameters[OutputIdx].Value = CurrVal;
                m_Parameters[OutputIdx].ValueTime = AtT;
            }
        }
        else
        {
            CurrVal = Clamp(MinVal, m_Parameters[OutputIdx].Value, MaxVal);
            m_Parameters[OutputIdx].ValuePrev = m_Parameters[OutputIdx].Value;
            m_Parameters[OutputIdx].Speed = 0;
            m_Parameters[OutputIdx].Value = CurrVal;
            m_Parameters[OutputIdx].ValueTime = AtT;
        }
    }    


    return CurrVal;
}

bool GSTATE
parameters::GetScalarOutputRange(granny_int32x OutputIdx, float* MinVal, float* MaxVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(MinVal && MaxVal);

    *MinVal = m_parametersToken->Parameters[OutputIdx].MinValue;
    *MaxVal = m_parametersToken->Parameters[OutputIdx].MaxValue;

    return true;
}

void GSTATE
parameters::SetScalarOutputRange(granny_int32x OutputIdx,
                                 float         MinVal,
                                 float         MaxVal)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].MinValue = MinVal;
    m_parametersToken->Parameters[OutputIdx].MaxValue = MaxVal;
}

void GSTATE
parameters::SetMinRandomValue(granny_int32x OutputIdx,
                                 float         MinRandomValue)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].MinRandomValue = MinRandomValue;
}

float GSTATE
parameters::GetMinRandomValue(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    return m_parametersToken->Parameters[OutputIdx].MinRandomValue;
}

void GSTATE
parameters::SetMaxSpeed(granny_int32x OutputIdx, float MaxSpeed)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].MaxSpeed = MaxSpeed;
}

float GSTATE
parameters::GetMaxSpeed(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    return m_parametersToken->Parameters[OutputIdx].MaxSpeed;
}

void GSTATE
parameters::SetAccel(granny_int32x OutputIdx, float Accel)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].Accel = Accel;
}

float GSTATE
parameters::GetAccel(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    return m_parametersToken->Parameters[OutputIdx].Accel;
}

void GSTATE
parameters::SetParameterType(granny_int32x OutputIdx, parameter_type Type)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    if( Type < 0 || Type >= NumParameterTypes )
    {
        GS_InvalidCodePath("Invalid Value Type seen, converting to Value parameter type");
        Type = Parameter_Value;
    }

    GStateAssert(Type >= 0 && Type < NumParameterTypes);

    TakeTokenOwnership();

    m_parametersToken->Parameters[OutputIdx].Type = Type;
}

parameter_type GSTATE
parameters::GetParameterType(granny_int32x OutputIdx) const
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    granny_int32 TypeAsInt = m_parametersToken->Parameters[OutputIdx].Type;
    if( TypeAsInt < 0 || TypeAsInt >= NumParameterTypes )
    {
        GS_InvalidCodePath("Invalid Value Type seen, converting to Value parameter type");
        m_parametersToken->Parameters[OutputIdx].Type = Parameter_Value;
        return Parameter_Value;
    }
    else
    {
        return (parameter_type)TypeAsInt;
    }
}

bool GSTATE
parameters::SetOutputName(granny_int32x OutputIdx,
                          char const* NewEdgeName)
{
    if (!parent::SetOutputName(OutputIdx, NewEdgeName))
        return false;

    state_machine* ParentSM = GSTATE_DYNCAST(GetParent(), state_machine);
    if (ParentSM)
        ParentSM->NoteParameterNameChange(this, OutputIdx);

    return true;
}


// Snapshot creation.  Let's have a talk about this in the parameter context.  We need to
// dump the m_Parameters, and then read it back in, pushing it into the token iff we own
// it.  That's a little weird, obv.
CREATE_SNAPSHOT(parameters)
{
    granny_int32 NumParameters = m_parametersToken->ParameterCount;
    CREATE_WRITE_INT32(NumParameters);

    if (NumParameters != 0)
    {
        GStateAssert(m_Parameters != 0);
        CREATE_WRITE_INT32_ARRAY(m_Parameters, NumParameters);
    }

    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(parameters)
{
    RESET_OFFSET_TRACKING();

    granny_int32 NumParameters = 0;
    RESET_READ_INT32(NumParameters);

    if (NumParameters != m_parametersToken->ParameterCount)
    {
        GStateError("Invalid paramater count found in Reset");
        return false;
    }

    if (NumParameters != 0)
    {
        GStateAssert(m_Parameters != 0);
        RESET_READ_INT32_ARRAY(m_Parameters, NumParameters);
    }

    RESET_PASS_TO_PARENT();
}


