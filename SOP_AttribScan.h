#pragma once

// Step 1. Include the below header files, write up a class publically inheriting
//         from SOP_Node including the shown member functions. 

#include <SOP/SOP_Node.h>
#include <UT/UT_StringHolder.h>

class SOP_AttribScan : public SOP_Node
{
public:
    static PRM_Template* buildTemplates();
    static OP_Node* myConstructor(OP_Network* net, const char* name, OP_Operator* op)
    {
        return new SOP_AttribScan(net, name, op);
    }

    static const UT_StringHolder theSOPTypeName;

    const SOP_NodeVerb* cookVerb() const override;

protected:
    SOP_AttribScan(OP_Network* net, const char* name, OP_Operator* op)
        : SOP_Node(net, name, op)
    {
        // All verb SOPs must manage data IDs, to track what's changed
        // from cook to cook.
        mySopFlags.setManagesDataIDs(true);
    }

    ~SOP_AttribScan() override {}

    /// Since this SOP implements a verb, cookMySop just delegates to the verb.
    OP_ERROR cookMySop(OP_Context& context) override
    {
        return cookMyselfAsVerb(context);
    }
};

