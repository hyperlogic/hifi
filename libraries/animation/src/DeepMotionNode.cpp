#include "DeepMotionNode.h"

#include "RotationConstraint.h"
#include "Profile.h"

void DeepMotionNode::loadPoses(const AnimPoseVec& poses) {
    assert(_skeleton && ((poses.size() == 0) || (_skeleton->getNumJoints() == (int)poses.size())));
    if (_skeleton->getNumJoints() == (int)poses.size()) {
        _relativePoses = poses;
    } else {
        _relativePoses.clear();
    }
}

void DeepMotionNode::setTargetVars(const QString& jointName, const QString& positionVar, const QString& rotationVar,
    const QString& typeVar, const QString& weightVar, float weight, const std::vector<float>& flexCoefficients,
    const QString& poleVectorEnabledVar, const QString& poleReferenceVectorVar, const QString& poleVectorVar) {
    //IKTargetVar targetVar(jointName, positionVar, rotationVar, typeVar, weightVar, weight, flexCoefficients, poleVectorEnabledVar, poleReferenceVectorVar, poleVectorVar);

    // if there are dups, last one wins.
    //bool found = false;
    //for (auto& targetVarIter : _targetVarVec) {
    //    if (targetVarIter.jointName == jointName) {
    //        targetVarIter = targetVar;
    //        found = true;
    //        break;
    //    }
    //}
    //if (!found) {
    //    // create a new entry
    //    _targetVarVec.push_back(targetVar);
    //}
}

//virtual
const AnimPoseVec& DeepMotionNode::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimNode::Triggers& triggersOut) {
    // don't call this function, call overlay() instead
    //assert(false);
    return _relativePoses;
}

//virtual
const AnimPoseVec& DeepMotionNode::overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) {
#ifdef Q_OS_ANDROID
    // disable IK on android
    return underPoses;
#endif

    // allows solutionSource to be overridden by an animVar
    auto solutionSource = animVars.lookup(_solutionSourceVar, (int)_solutionSource);

    const float MAX_OVERLAY_DT = 1.0f / 30.0f; // what to clamp delta-time to in AnimInverseKinematics::overlay
    if (dt > MAX_OVERLAY_DT) {
        dt = MAX_OVERLAY_DT;
    }

    if (_relativePoses.size() != underPoses.size()) {
        loadPoses(underPoses);
    } else {
        PROFILE_RANGE_EX(simulation_animation, "dm/relax", 0xffff00ff, 0);

        if (!underPoses.empty()) {
            // Sometimes the underpose itself can violate the constraints.  Rather than
            // clamp the animation we dynamically expand each constraint to accomodate it.
            std::map<int, RotationConstraint*>::iterator constraintItr = _constraints.begin();
            while (constraintItr != _constraints.end()) {
                int index = constraintItr->first;
                constraintItr->second->dynamicallyAdjustLimits(underPoses[index].rot());
                ++constraintItr;
            }
        }
    }

    if (i++ % 1000 == 0)
        qCInfo(animation) << "Tick";
    TickIntegration(dt);

    return _relativePoses;
}
