//
//  AnimCharacterRig.h
//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimCharacterRig_h
#define hifi_AnimCharacterRig_h

#include <string>

#include <map>
#include <vector>

#include "AnimNode.h"
#include "IKTarget.h"

class AnimCharacterRig : public AnimNode {
public:

    explicit AnimCharacterRig(const QString& id);
    virtual ~AnimCharacterRig() override;

    struct ControllerOperationData {
        enum Flags { Rotation = 1, Position = 2 };
        glm::vec3 targetPos;
        glm::quat targetRot;
        int joint;
        uint8_t flags;
    };

    enum class OperationType {
        Controller = 0
    };

    struct Operation {
        OperationType type;
        union {
            ControllerOperationData controller;
        } d;
    };

    void addOperation(const Operation& op);

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimNode::Triggers& triggersOut) override;
    virtual const AnimPoseVec& overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) override;

protected:
    virtual void setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) override;

    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override { return _relativePoses; }

    // no copies
    AnimCharacterRig(const AnimCharacterRig&) = delete;
    AnimCharacterRig& operator=(const AnimCharacterRig&) = delete;

    AnimPoseVec _relativePoses;
};

#endif // hifi_AnimCharacterRig_h
