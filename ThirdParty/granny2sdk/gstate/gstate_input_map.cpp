// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_input_map.h"

#include "gstate_base.h"
#include "gstate_quick_vecs.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

void GSTATE
InputMap_CacheNumPoseInputs(node * Node, granny_int32& NumPoseInputs)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    NumPoseInputs = 0;
    for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
    {
        if( Node->GetInputType(Idx) == PoseEdge )
        {
            NumPoseInputs++;
        }
    }
}

void GSTATE
InputMap_TakeTokenOwnership(granny_int32*& InputMap, granny_int32*& OldInputMap, granny_int32& InputMapCount)
{
    GStateCloneArray(InputMap, OldInputMap, InputMapCount);
}

void GSTATE
InputMap_ReleaseOwnedToken(granny_int32* InputMap, granny_int32 InputMapCount)
{
    GStateDeallocate(InputMap);
    InputMapCount = 0;
}


granny_int32 GSTATE
InputMap_GetInputIndex(granny_int32 Index, granny_int32* InputMap, granny_int32 InputMapCount)
{
    //Backwards compat, just pass through if there's no index map. 
    if( InputMapCount == 0 )
    {
        return Index;
    }

    GStateAssert(Index >= 0 && Index < InputMapCount);
    return InputMap[Index];
}

granny_int32 GSTATE
InputMap_GetNthInputIndexForOutput(granny_int32 N, node * Node, granny_int32* InputMap, granny_int32 InputMapCount, granny_int32 OutputIndex)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    //Backwards Compatibility: Fall back on old runtime behavior if there is no InputMap. N+1 skips the parameter input node. 
    if( InputMap == NULL )
    {
        return N+1;
    }

    //Outputs give you POSE MORPH MORPH MORPH
    //Inputs are PARAM POSE MORPH MORPH MORPH POSE MORPH MORPH MORPH ...
    granny_int32 NumOutputs = Node->GetNumOutputs();

    //Non Parameter Inputs should be a multiple of the outputs
    GStateAssert( (Node->GetNumInputs()-1) % NumOutputs == 0 );

    granny_int32 MapIndex = (NumOutputs * N) + OutputIndex + 1; //+1 to add the parameter input back on. 
    granny_int32 InputIndex = InputMap_GetInputIndex(MapIndex, InputMap, InputMapCount);

    return InputIndex;
}

bool GSTATE
InputMap_DeleteInputSet(node * Node, granny_int32 InputIndex, granny_int32*& InputMap, granny_int32& InputMapCount, granny_int32& NumPoseInputs)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    //Invalidate the removed index and shift map indices above the remove-point down
    int PoseMapIndex = -1;
    for( int MapIdx=0; MapIdx< InputMapCount; MapIdx++ )
    {
        if( InputMap[MapIdx] == InputIndex )
        {
            PoseMapIndex = MapIdx;
            InputMap[MapIdx] = -1;
        }

        if( InputMap[MapIdx] > InputIndex )
        {
            InputMap[MapIdx]--;
        }
    }

    //Outputs of the blend node are always the 1 pose + total number of morphs. 
    int NumMorphsToRemove = Node->GetNumOutputs() - 1;

    for( int MapIndexToRemove=PoseMapIndex+1; MapIndexToRemove < PoseMapIndex+1+NumMorphsToRemove; MapIndexToRemove++ )
    {
        GStateAssert(MapIndexToRemove < InputMapCount);

        int InputIndexToRemove = InputMap[MapIndexToRemove];

        //Ignore this index if it's already been cleared. 
        if( InputIndexToRemove == -1 )
        {
            continue;
        }

        GStateAssert(Node->GetInputType(InputIndexToRemove) == MorphEdge || Node->GetInputType(InputIndexToRemove) == ScalarEdge || Node->GetInputType(InputIndexToRemove) == EventEdge);

        if(Node->GetInputType(InputIndexToRemove) == PoseEdge)
        {
            NumPoseInputs--;
        }

        bool DeleteResult = Node->DeleteInput(InputIndexToRemove);

        if( !DeleteResult )
        {
            GStateAssert(true && "This should never happen. Something went horribly wrong.");
            return false;
        }

        //Shift down the higher map indices for every removal. 
        for( int MapIdx=0; MapIdx< InputMapCount; MapIdx++ )
        {
            if( InputMap[MapIdx] == InputIndexToRemove )
            {
                InputMap[MapIdx] = -1;
            }

            if( InputMap[MapIdx] > InputIndexToRemove )
            {
                InputMap[MapIdx]--;
            }
        }
    }

    //Make another pass removing items from the map. 
    for( int MapIdx=0; MapIdx< InputMapCount; MapIdx++ )
    {
        if( InputMap[MapIdx] == -1 )
        {
            QVecRemoveElement(MapIdx, InputMapCount, InputMap);
            MapIdx--;
        }
    }
    InputMap_CacheNumPoseInputs(Node,NumPoseInputs);
    return true;
}

void GSTATE
InputMap_AddInputSet(node * Node, granny_int32*& InputMap, granny_int32& InputMapCount, granny_int32& NumPoseInputs)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    for(int OutputIdx=0; OutputIdx<Node->GetNumOutputs(); OutputIdx++)
    {
        node_edge_type EdgeType = Node->GetOutputType(OutputIdx);

        const char * EdgeName = "";
        if(EdgeType == PoseEdge)
        {
            NumPoseInputs++;
            EdgeName = "PoseIn";
        }
        else
        {
            EdgeName = Node->GetOutputName(OutputIdx);
        }

        granny_int32x PoseIdx = Node->AddInput(EdgeType, EdgeName);

        QVecPushNewElement(InputMapCount, InputMap);
        InputMap[InputMapCount-1] = PoseIdx;
    }

}

void GSTATE
InputMap_AddInputChannelToEachInputSet(node * Node, granny_int32 OutputIdx, granny_int32*& InputMap, granny_int32& InputMapCount)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    GStateAssert(Node->GetOutputType(OutputIdx) == MorphEdge || Node->GetOutputType(OutputIdx) == ScalarEdge || Node->GetOutputType(OutputIdx) == EventEdge);

    granny_int32 * OldInputMap = InputMap;
    granny_int32 OldInputMapCount = InputMapCount;

    InputMap = 0;
    InputMapCount = 0;

    for(int OldMapIdx=0; OldMapIdx<OldInputMapCount; OldMapIdx++)
    {
        {
            granny_int32& Val = QVecPushNewElement(InputMapCount, InputMap);
            Val = OldInputMap[OldMapIdx];
        }

        if(OldMapIdx > 0 && (OldMapIdx % OutputIdx == 0))
        {
            granny_int32x InputIdx = Node->AddInput(Node->GetOutputType(OutputIdx), Node->GetOutputName(OutputIdx));

            granny_int32& Val = QVecPushNewElement(InputMapCount, InputMap);
            Val = InputIdx;
        }
    }

    GStateDeallocate(OldInputMap);
    OldInputMapCount = 0;
}

void GSTATE
InputMap_SetNameForEachInputInInputSet(node * Node, granny_int32 OutputIdx, char const* NewEdgeName, granny_int32*& InputMap, granny_int32& InputMapCount)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    //Skips the zero item. That is the parameter
    for(int MapIdx=1; MapIdx<InputMapCount; MapIdx++)
    {
        if((MapIdx-1) % Node->GetNumOutputs() == OutputIdx)
        {
            Node->SetInputName(InputMap[MapIdx], NewEdgeName);
        }
    }
}

void GSTATE
InputMap_RemoveInputChannelFromEachInputSet(node * Node, granny_int32 OutputIdx, granny_int32*& InputMap, granny_int32& InputMapCount)
{
    GStateAssert(GSTATE_DYNCAST(Node, container) == NULL); //Containers have internal inputs and outputs that make this not work. 

    GStateAssert(Node->GetOutputType(OutputIdx) == MorphEdge || Node->GetOutputType(OutputIdx) == ScalarEdge || Node->GetOutputType(OutputIdx) == EventEdge);

    int OldIdx = 0;

    //Start at 1 since the first entry is the Parameter
    for(int MapIdx=1; MapIdx<InputMapCount; MapIdx++)
    {
        if(OldIdx % Node->GetNumOutputs() == OutputIdx)
        {
            granny_int32 IndexToDelete = InputMap[MapIdx];
            Node->DeleteInput(IndexToDelete);

            QVecRemoveElement(MapIdx,InputMapCount,InputMap);

            MapIdx--;

            //Adjust anything in the input map above the delete point down by one
            for(int AdjustMapIdx=0; AdjustMapIdx<InputMapCount; AdjustMapIdx++)
            {
                if( InputMap[AdjustMapIdx] >= IndexToDelete )
                {
                    InputMap[AdjustMapIdx]--;
                }
            }
        }

        OldIdx++;
    }
}
