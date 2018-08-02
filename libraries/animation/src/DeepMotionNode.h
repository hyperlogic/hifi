#ifndef hifi_DeepMotionNode_h
#define hifi_DeepMotionNode_h

//#include "AnimNode.h"

#include "deepMotion_interface.h"

class DeepMotionNode// : public AnimNode //TODO: inherit from AnimNode
{
public:
    explicit DeepMotionNode()// : AnimNode(AnimNode::Type::InverseKinematics, id)
    {
        InitializeIntegration();
    }

    ~DeepMotionNode()
    {
        FinalizeIntegration();
    }
};

#endif // hifi_DeepMotionNode_h
