//
//  AnimSplineIK.cpp
//
//  Created by Anthony J. Thibault on 5/12/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimSplineIK.h"
#include "AnimationLogging.h"
#include "AnimUtil.h"

AnimSplineIK::AnimSplineIK(const QString& id, float alpha) :
    AnimNode(AnimNode::Type::SplineIK, id) {

}

AnimSplineIK::~AnimSplineIK() {

}

const AnimPoseVec& AnimSplineIK::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) {
    _poses = _children[0]->evaluate(animVars, context, dt, triggersOut);
    return _poses;
}

// for AnimDebugDraw rendering
const AnimPoseVec& AnimSplineIK::getPosesInternal() const {
    return _poses;
}
