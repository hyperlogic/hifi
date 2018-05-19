//
//  AnimTwoBoneIK.cpp
//
//  Created by Anthony J. Thibault on 5/12/18.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimTwoBoneIK.h"

#include <DebugDraw.h>

#include "AnimationLogging.h"
#include "AnimUtil.h"

AnimTwoBoneIK::AnimTwoBoneIK(const QString& id, float alpha, const QString& alphaVar,
                             const QString& baseJointName, const QString& midJointName, const QString& tipJointName,
                             const QString& endEffectorRotationVar, const QString& endEffectorPositionVar) :
    AnimNode(AnimNode::Type::TwoBoneIK, id),
    _alpha(alpha),
    _alphaVar(alphaVar),
    _baseJointName(baseJointName),
    _midJointName(midJointName),
    _tipJointName(tipJointName),
    _endEffectorRotationVar(endEffectorRotationVar),
    _endEffectorPositionVar(endEffectorPositionVar)
{

}


AnimTwoBoneIK::~AnimTwoBoneIK() {

}

const AnimPoseVec& AnimTwoBoneIK::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) {

    assert(_children.size() == 1);
    if (_children.size() != 1) {
        return _poses;
    }

    // evalute underPoses
    AnimPoseVec underPoses = _children[0]->evaluate(animVars, context, dt, triggersOut);

    // if we don't have a skeleton, or jointName lookup failed.
    if (!_skeleton || _baseJointIndex == -1 || _midJointIndex == -1 || _tipJointIndex == -1 || underPoses.size() == 0) {
        // pass underPoses through unmodified.
        _poses = underPoses;
        return _poses;
    }

    // AJT: TODO use alpha.
    float alpha = animVars.lookup(_alphaVar, _alpha);

    // don't perform IK if we have bad indices.
    if (_tipJointIndex == -1 || _midJointIndex == -1 || _baseJointIndex == -1) {
        _poses = underPoses;
        return underPoses;
    }

    // get default tip pose from underPoses (geom space)
    AnimPose absTipUnderPose = _skeleton->getAbsolutePose(_tipJointIndex, underPoses);

    // look up end effector from animVars, make sure to convert into geom space.
    AnimPose absTipPose(animVars.lookupRigToGeometry(_endEffectorRotationVar, absTipUnderPose.rot()),
                        animVars.lookupRigToGeometry(_endEffectorPositionVar, absTipUnderPose.trans()));

    // get default mid and base poses from underPoses (geom space)
    AnimPose absMidPose = _skeleton->getAbsolutePose(_midJointIndex, underPoses);
    AnimPose absBasePose = _skeleton->getAbsolutePose(_baseJointIndex, underPoses);

    float r0 = glm::length(underPoses[_midJointIndex].trans());
    float r1 = glm::length(underPoses[_tipJointIndex].trans());
    float d = glm::length(absTipPose.trans() - absBasePose.trans());

    glm::vec3 newMidPos;
    if (d > r0 + r1) {
        // put midPos on line between base and tip.
        newMidPos = 0.5f * (absTipPose.trans() + absBasePose.trans());
    } else {
        glm::vec3 u, v, w;
        generateBasisVectors(glm::normalize(absTipPose.trans() - absBasePose.trans()),
                             glm::normalize(absMidPose.trans() - absBasePose.trans()), u, v, w);

        // http://mathworld.wolfram.com/Circle-CircleIntersection.html
        // intersection of circles formed by x^2 + y^2 = r0 and (x - d)^2 + y^2 = r1.
        // there are two solutions (x, y) and (x, -y), we pick the positive one.
        float x = (d * d - r1 * r1 + r0 * r0) / (2.0f * d);
        float y = sqrtf((-d + r1 - r0) * (-d - r1 + r0) * (-d + r1 + r0) * (d + r1 + r0)) / (2.0f * d);

        // convert (x, y) back into geom space using the u, v axes.
        newMidPos = u * x + v * y + absBasePose.trans();
    }

    // AJT: TODO: REMOVE: debug draw.
    glm::mat4 geomToWorld = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();
    glm::vec3 basePos = transformPoint(geomToWorld, absBasePose.trans());
    glm::vec3 midPos = transformPoint(geomToWorld, newMidPos);
    glm::vec3 tipPos = transformPoint(geomToWorld, absTipPose.trans());
    DebugDraw::getInstance().drawRay(basePos, midPos, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    DebugDraw::getInstance().drawRay(midPos, tipPos, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    _poses = underPoses;
    return _poses;
}

// for AnimDebugDraw rendering
const AnimPoseVec& AnimTwoBoneIK::getPosesInternal() const {
    return _poses;
}

void AnimTwoBoneIK::setSkeletonInternal(AnimSkeleton::ConstPointer skeleton) {
    AnimNode::setSkeletonInternal(skeleton);
    lookUpIndices();
}

void AnimTwoBoneIK::lookUpIndices() {
    assert(_skeleton);

    // look up bone indices by name in AnimSkeleton
    _baseJointIndex = _skeleton->nameToJointIndex(_baseJointName);
    _midJointIndex = _skeleton->nameToJointIndex(_midJointName);
    _tipJointIndex = _skeleton->nameToJointIndex(_tipJointName);

    // issue a warning if bones are not found in skeleton
    if (_baseJointIndex == -1) {
        qWarning(animation) << "AnimTwoBoneIK(" << _id << "): could not find base-bone with name" << _baseJointName;
    }
    if (_midJointIndex == -1) {
        qWarning(animation) << "AnimTwoBoneIK(" << _id << "): could not find mid-bone with name" << _midJointName;
    }
    if (_tipJointIndex == -1) {
        qWarning(animation) << "AnimTwoBoneIK(" << _id << "): could not find tip-bone with name" << _tipJointName;
    }
}
