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
                             const glm::vec3& midHingeAxis, const QString& endEffectorRotationVar, const QString& endEffectorPositionVar) :
    AnimNode(AnimNode::Type::TwoBoneIK, id),
    _alpha(alpha),
    _alphaVar(alphaVar),
    _baseJointName(baseJointName),
    _midJointName(midJointName),
    _tipJointName(tipJointName),
    _midHingeAxis(midHingeAxis),
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

    float alpha = animVars.lookup(_alphaVar, _alpha);

    // don't perform IK if we have bad indices, or alpha is zero
    if (_tipJointIndex == -1 || _midJointIndex == -1 || _baseJointIndex == -1 || alpha == 0.0f) {
        _poses = underPoses;
        return _poses;
    }

    AnimPose baseParentPose = _skeleton->getAbsolutePose(_baseParentJointIndex, underPoses);

    // get tip pose from underPoses (geom space)
    AnimPose tipPose = _skeleton->getAbsolutePose(_tipJointIndex, underPoses);

    // look up end effector from animVars, make sure to convert into geom space.
    AnimPose targetPose(animVars.lookupRigToGeometry(_endEffectorRotationVar, tipPose.rot()),
                        animVars.lookupRigToGeometry(_endEffectorPositionVar, tipPose.trans()));

    // get default mid and base poses from underPoses (geom space)
    AnimPose midPose = _skeleton->getAbsolutePose(_midJointIndex, underPoses);
    AnimPose basePose = _skeleton->getAbsolutePose(_baseJointIndex, underPoses);

    glm::vec3 bicepVector = midPose.trans() - basePose.trans();
    float r0 = glm::length(bicepVector);
    bicepVector = bicepVector / r0;

    glm::vec3 forearmVector = tipPose.trans() - midPose.trans();
    float r1 = glm::length(forearmVector);
    forearmVector = forearmVector / r1;

    float d = glm::length(targetPose.trans() - basePose.trans());

    float midAngle = 0.0f;
    if (d < r0 + r1) {
        float y = sqrtf((-d + r1 - r0) * (-d - r1 + r0) * (-d + r1 + r0) * (d + r1 + r0)) / (2.0f * d);
        midAngle = PI - (acosf(y / r0) + acosf(y / r1));
    }

    AnimPoseVec ikPoses = underPoses;

    // compute midJoint rotation
    ikPoses[_midJointIndex].rot() = glm::angleAxis(midAngle, _midHingeAxis);

    // recompute tip pose after mid joint has been rotated
    AnimPose newTipPose = _skeleton->getAbsolutePose(_tipJointIndex, ikPoses);

    glm::vec3 leverArm = newTipPose.trans() - basePose.trans();
    glm::vec3 targetLine = targetPose.trans() - basePose.trans();

    // compute delta rotation that brings leverArm parallel to targetLine
    glm::vec3 axis = glm::cross(leverArm, targetLine);
    float axisLength = glm::length(axis);
    const float MIN_AXIS_LENGTH = 1.0e-4f;
    if (axisLength > MIN_AXIS_LENGTH) {
        axis /= axisLength;
        float cosAngle = glm::clamp(glm::dot(leverArm, targetLine) / (glm::length(leverArm) * glm::length(targetLine)), -1.0f, 1.0f);
        float angle = acosf(cosAngle);
        glm::quat deltaRot = glm::angleAxis(angle, axis);

        // combine deltaRot with basePose.
        glm::quat absRot = deltaRot * basePose.rot();

        // transform result back into parent relative frame.
        ikPoses[_baseJointIndex].rot() = glm::inverse(baseParentPose.rot()) * absRot;
    }

    // recompute midJoint pose after base has been rotated.
    AnimPose midJointPose = _skeleton->getAbsolutePose(_midJointIndex, ikPoses);

    // transform target rotation in to parent relative frame.
    ikPoses[_tipJointIndex].rot() = glm::inverse(midJointPose.rot()) * targetPose.rot();

    if (alpha == 1.0f) {
        _poses = ikPoses;
    } else {
        // apply alpha blend
        ::blend(_poses.size(), &underPoses[0], &ikPoses[0], alpha, &_poses[0]);
    }

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
