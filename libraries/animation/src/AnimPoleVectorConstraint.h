//
//  PoleVectorConstraint.h
//
//  Created by Anthony J. Thibault on 5/25/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PoleVectorConstraint_h
#define hifi_PoleVectorConstraint_h

#include "AnimNode.h"

// Three bone IK chain

class PoleVectorConstraint : public AnimNode {
public:
    friend class AnimTests;

    PoleVectorConstraint(const QString& id, bool enabled, glm::vec3 referencePoleVector,
                         const QString& baseJointName, const QString& midJointName, const QString& tipJointName,
                         const QString& enabledVar, const QString& poleVectorVar);
    virtual ~PoleVectorConstraint() override;

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) override;

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override;
    virtual void setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) override;

    void lookUpIndices();

    AnimPoseVec _poses;

    bool _enabled;
    glm::vec3 _referencePoleVector;

    QString _baseJointName;
    QString _midJointName;
    QString _tipJointName;

    QString _enabledVar;
    QString _poleVectorVar;

    int _baseParentJointIndex { -1 };
    int _baseJointIndex { -1 };
    int _midJointIndex { -1 };
    int _tipJointIndex { -1 };

    // no copies
    PoleVectorConstraint(const PoleVectorConstraint&) = delete;
    PoleVectorConstraint& operator=(const PoleVectorConstraint&) = delete;
};

#endif // hifi_PoleVectorConstraint_h
