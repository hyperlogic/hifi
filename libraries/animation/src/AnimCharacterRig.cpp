//
//  AnimCharacterRig.cpp
//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimCharacterRig.h"

#include <GLMHelpers.h>

#include "AnimationLogging.h"
#include "AnimUtil.h"

AnimCharacterRig::AnimCharacterRig(const QString& id) : AnimNode(AnimNode::Type::CharacterRig, id) {
}

AnimCharacterRig::~AnimCharacterRig() {
}

//virtual
const AnimPoseVec& AnimCharacterRig::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimNode::Triggers& triggersOut) {
    // don't call this function, call overlay() instead
    assert(false);
    return _relativePoses;
}

//virtual
const AnimPoseVec& AnimCharacterRig::overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) {
    // TODO
    return _relativePoses;
}

void AnimCharacterRig::setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) {
    AnimNode::setSkeletonInternal(skeleton);
    // TODO initialization...
}
