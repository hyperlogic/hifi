//
//  AnimLowVelocityFilter.cpp
//
//  Created by Anthony J. Thibault on 11/30/17.
//  Copyright (c) 2017 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimLowVelocityFilter.h"

#include <algorithm>

#include "GLMHelpers.h"
#include "AnimationLogging.h"
#include "AnimUtil.h"

AnimLowVelocityFilter::AnimLowVelocityFilter(const QString& id) :
    AnimNode(AnimNode::Type::LowVelocityFilter, id) {

}

AnimLowVelocityFilter::~AnimLowVelocityFilter() {

}

const AnimPoseVec& AnimLowVelocityFilter::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) {
    assert(false);
    return _relativePoses;
}

const AnimPoseVec& AnimLowVelocityFilter::overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) {
    const float LINEAR_SPEED_LIMIT = 0.1f; // meters per second.
    const float EPSILON = 1.0e-4f;

    // convert speed from m/s into geometry frame (usually cm/s)
    const float geomLinearSpeedLimit = LINEAR_SPEED_LIMIT / extractUniformScale(context.getGeometryToRigMatrix());

    if (_relativePoses.size() != underPoses.size()) {
        _relativePoses = underPoses;
    } else {
        for (size_t i = 0; i < underPoses.size(); i++) {
            glm::vec3 linearDelta = underPoses[i].trans() - _relativePoses[i].trans();
            float linearDeltaLength = linearDelta.length();
            if (linearDeltaLength > EPSILON) {
                float linearSpeed = std::min(linearDeltaLength / dt, geomLinearSpeedLimit);
                glm::vec3 clampedLinearDelta = linearDelta * ((linearSpeed * dt) / linearDeltaLength);
                _relativePoses[i].trans() = _relativePoses[i].trans() + clampedLinearDelta;
            } else {
                _relativePoses[i].trans() = underPoses[i].trans();
            }

            // AJT: TODO angular velocity limiting
            _relativePoses[i].rot() = underPoses[i].rot();

            _relativePoses[i].scale() = underPoses[i].scale();
        }
    }
    return _relativePoses;
}

// for AnimDebugDraw rendering
const AnimPoseVec& AnimLowVelocityFilter::getPosesInternal() const {
    return _relativePoses;
}
