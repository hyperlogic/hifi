//
//  AnimTwoBoneIK.h
//
//  Created by Anthony J. Thibault on 5/12/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimTwoBoneIK_h
#define hifi_AnimTwoBoneIK_h

#include "AnimNode.h"

// Simple two bone IK chain
class AnimTwoBoneIK : public AnimNode {
public:
    friend class AnimTests;

    AnimTwoBoneIK(const QString& id, float alpha, const QString& alphaVar,
                  const QString& baseJointName, const QString& midJointName, const QString& tipJointName,
                  const glm::vec3& midHingeAxis, const QString& endEffectorRotationVar, const QString& endEffectorPositionVar);
    virtual ~AnimTwoBoneIK() override;

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) override;

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override;
    virtual void setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) override;

    void lookUpIndices();

    AnimPoseVec _poses;

    float _alpha;
    QString _alphaVar;

    QString _baseJointName;
    QString _midJointName;
    QString _tipJointName;
    glm::vec3 _midHingeAxis;  // in baseJoint relative frame.

    int _baseParentJointIndex { -1 };
    int _baseJointIndex { -1 };
    int _midJointIndex { -1 };
    int _tipJointIndex { -1 };

    QString _endEffectorRotationVar;
    QString _endEffectorPositionVar;

    // no copies
    AnimTwoBoneIK(const AnimTwoBoneIK&) = delete;
    AnimTwoBoneIK& operator=(const AnimTwoBoneIK&) = delete;
};

#endif // hifi_AnimTwoBoneIK_h
