// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_PARAMETERS_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

enum parameter_type
{
    Parameter_Value,   //A single value, defined by the slider (or driven by code)
    Parameter_Random,  //A random value. Different every time, so nodes need to manage only sampling once on activate if necessary
    Parameter_Timer,   //A timer value. Counts down to zero from the max (or a random start point begin minrandom and max)

    NumParameterTypes
};

char const* NameForParameterType(parameter_type);

class parameters : public node
{
    struct runtime_parameter
    {
        runtime_parameter()
            : Value(0.0f),
            ValuePrev(0.0f),
            Speed(0.0f),
            TargetValue(0.0f),
            ValueTime(0.0f),
            ActivationTime(0.0f),
            ActualTimer(0.0f)
        {
        }

        float Value;            //The current value
        float ValuePrev;        //The previous value
        float Speed;            //Speed this parameter can change at
        float TargetValue;      //The value this parameter is moving toward
        float ValueTime;        //When the current value was created
        float ActivationTime;   //When this parameter was activated
        float ActualTimer;      //The actual timer (for a a randomized timer parameter)
    };

    typedef node parent;

public:
    void SetParameter(granny_int32x OutputIdx, granny_real32 NewVal);
    void RequestParameter(granny_int32x OutputIdx, granny_real32 NewVal); //Allows filtering (accel/speed capping) to take place
    float GetRequestedParam(granny_int32x OutputIdx) const;
    void SetParameterDefault(granny_int32x OutputIdx, granny_real32 NewVal);
    void SetScalarOutputRange(granny_int32x OutputIdx, float MinVal, float MaxVal);
    void SetMinRandomValue(granny_int32x OutputIdx, float MinRandomValue);
    float GetMinRandomValue(granny_int32x OutputIdx) const;
    void SetMaxSpeed(granny_int32x OutputIdx, float MaxSpeed);
    float GetMaxSpeed(granny_int32x OutputIdx) const;
    void SetAccel(granny_int32x OutputIdx, float Accel);
    float GetAccel(granny_int32x OutputIdx) const;

    void SetParameterType(granny_int32x OutputIdx, parameter_type Type);
    parameter_type GetParameterType(granny_int32x OutputIdx) const;
    virtual void Activate(granny_real32 AtT);

    // Note that this is "ClampSlider" because it only constrains the *interface*, not the code,
    // except in the case of a randomly generated parameter, which will generate clamped values
    // if the slider is set to clamp to ints
    void SetClampSliderToInts(granny_int32x OutputIdx, bool Clamp);
    bool GetClampSliderToInts(granny_int32x OutputIdx);

    void SetCyclic(granny_int32x OutputIdx, bool Clamp);
    bool GetCyclic(granny_int32x OutputIdx);

    // We track this to let any owning state machines know that we've changed a parameter
    // name...
    virtual bool SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    virtual bool AllowSyncInputsWithParentOutputs() const { return false; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return false; }

    DECL_SAMPLESCALAR_INTERFACE();
    DECL_IO_MANIPULATION_INTERFACE();

    DECL_CONCRETE_NODE_TOKEN(parameters);
    DECL_SNAPPABLE();
    IMPL_CASTABLE_INTERFACE(parameters);

private:
    runtime_parameter* m_Parameters;
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_PARAMETERS_H
#endif /* GSTATE_PARAMETERS_H */
