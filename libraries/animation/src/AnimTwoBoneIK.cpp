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
#include "AnimationLogging.h"
#include "AnimUtil.h"

AnimTwoBoneIK::AnimTwoBoneIK(const QString& id, float alpha) :
    AnimNode(AnimNode::Type::TwoBoneIK, id) {

}

AnimTwoBoneIK::~AnimTwoBoneIK() {

}

const AnimPoseVec& AnimTwoBoneIK::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) {

    // evalute underPoses
    AnimPoseVec underPoses = _children[0]->evaluate(animVars, context, dt, triggersOut);

    // don't perform IK if we have bad indices.
    if (_tipBoneIndex == -1 || _midBoneIndex == -1 || _baseBoneIndex == -1) {
        _poses = underPoses;
        return underPoses;
    }

    // get default tip pose from underPoses (geom space)
    AnimPose absTipUnderPose = _skeleton->getAbsolutePose(_tipBoneIndex, underPoses);

    // look up end effector from animVars, make sure to convert into geom space.
    AnimPose absTipPose(animVars.lookupRigToGeometry(_endEffectorRotationVar, absTipUnderPose.rot()),
                        animVars.lookupRigToGeometry(_endEffectorPositionVar, absTipUnderPose.trans()));

    // get default mid and base poses from underPoses (geom space)
    AnimPose absMidPose = _skeleton->getAbsolutePose(_midBoneIndex, underPoses);
    AnimPose absBasePose = _skeleton->getAbsolutePose(_baseBoneIndex, underPoses);

    float r0 = glm::length(underPoses[_baseBoneIndex].trans());
    float r1 = glm::length(underPoses[_midBoneIndex].trans());
    float d = glm::length(absTipPose.trans() - absBasePose.trans());

    glm::vec3 newMidPos;
    if (d > r0 + r1) {
        // put midPos on line between base and tip.
        newMidPos = (absTipPose.trans() - absBasePose.trans()) * (r0 / d);
    } else {
        glm::vec3 u, v, w;
        generateBasisVectors(glm::normalize(absTipPose.trans() - absBasePose.trans()),
                             glm::normalize(absMidPose.trans() - absBasePose.trans()), u, v, w);

        // intersection of circles formed by x^2 + y^2 = r0 and (x - d)^2 + y^2 = r1.
        // there are two solutions (x, y) and (x, -y), we pick the positive one.
        float x = (d * d - r1 * r1 + r0 * r0) / (2.0f * d);
        float y = sqrtf((-d + r1 - r0) * (-d - r1 + r0) * (-d + r1 + r0) * (-d + r1 + r0)) / (2.0f * d);

        // convert (x, y) back into geom space using the u, v axes.
        newMidPos = u * x + v * y + absBasePose.trans();
    }

    // TODO: debug draw.

    return underPoses;
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
    _baseBoneIndex = _skeleton->nameToJointIndex(_baseBoneName);
    _midBoneIndex = _skeleton->nameToJointIndex(_midBoneName);
    _tipBoneIndex = _skeleton->nameToJointIndex(_tipBoneName);

    // issue a warning if bones are not found in skeleton
    if (_baseBoneIndex == -1) {
        qWarning(animation) << "AnimTwoBoneIK(" << _id << "): could not find base-bone with name" << _baseBoneName;
    }
    if (_midBoneIndex == -1) {
        qWarning(animation) << "AnimTwoBoneIK(" << _id << "): could not find mid-bone with name" << _midBoneName;
    }
    if (_tipBoneIndex == -1) {
        qWarning(animation) << "AnimTwoBoneIK(" << _id << "): could not find tip-bone with name" << _tipBoneName;
    }
}
