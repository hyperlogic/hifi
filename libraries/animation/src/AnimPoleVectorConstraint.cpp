//
//  PoleVectorConstraint.cpp
//
//  Created by Anthony J. Thibault on 5/12/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimPoleVectorConstraint.h"
#include "AnimationLogging.h"
#include "AnimUtil.h"

PoleVectorConstraint::PoleVectorConstraint(const QString& id, bool enabled, glm::vec3 referencePoleVector,
                                           const QString& baseJointName, const QString& midJointName, const QString& tipJointName,
                                           const QString& enabledVar, const QString& poleVectorVar) :
    AnimNode(AnimNode::Type::PoleVectorConstraint, id),
    _enabled(enabled),
    _referencePoleVector(referencePoleVector),
    _baseJointName(baseJointName),
    _midJointName(midJointName),
    _tipJointName(tipJointName),
    _enabledVar(enabledVar),
    _poleVectorVar(poleVectorVar) {

}

PoleVectorConstraint::~PoleVectorConstraint() {

}

const AnimPoseVec& PoleVectorConstraint::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) {
    _poses = _children[0]->evaluate(animVars, context, dt, triggersOut);

    processOutputJoints(triggersOut);

    return _poses;
}

// for AnimDebugDraw rendering
const AnimPoseVec& PoleVectorConstraint::getPosesInternal() const {
    return _poses;
}

void PoleVectorConstraint::setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) {
    AnimNode::setSkeletonInternal(skeleton);
    lookUpIndices();

    // AJT: BIND
}

void PoleVectorConstraint::lookUpIndices() {
    assert(_skeleton);

    // look up bone indices by name
    std::vector<int> indices = _skeleton->lookUpJointIndices({_baseJointName, _midJointName, _tipJointName});

    // cache the results
    _baseJointIndex = indices[0];
    _midJointIndex = indices[1];
    _tipJointIndex = indices[2];

    if (_baseJointIndex != -1) {
        _baseParentJointIndex = _skeleton->getParentIndex(_baseJointIndex);
    }
}
