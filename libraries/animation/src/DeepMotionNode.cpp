#include "DeepMotionNode.h"

#include "dm_public/types.h"

#include "RotationConstraint.h"
#include "ResourceCache.h"
#include "Profile.h"
#include "PathUtils.h"

#include <fstream>
#include <string>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(deepMotion)

static const float CHARACTER_LOAD_PRIORITY = 10.0f;

DeepMotionNode::DeepMotionNode(const QString& id) : AnimNode(AnimNode::Type::InverseKinematics, id) {
    InitializeIntegration();

    auto characterUrl = PathUtils::resourcesUrl(_characterPath);
    _characterResource = QSharedPointer<Resource>::create(characterUrl);
    _characterResource->setSelf(_characterResource);
    _characterResource->setLoadPriority(this, CHARACTER_LOAD_PRIORITY);
    connect(_characterResource.data(), &Resource::loaded, this, &DeepMotionNode::characterLoaded);
    connect(_characterResource.data(), &Resource::failed, this, &DeepMotionNode::characterFailedToLoad);
    _characterResource->ensureLoading();
}

void DeepMotionNode::characterLoaded(const QByteArray data)
{
    qCInfo(deepMotion) << "Loading character";
    _sceneHandle = LoadCharacterOnScene(reinterpret_cast<const uint8_t*>(data.constData()));
}

void DeepMotionNode::characterFailedToLoad(QNetworkReply::NetworkError error)
{
    qCCritical(deepMotion) << "Failed to load character from resources: " << error;
}

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
const AnimPoseVec& DeepMotionNode::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) {
    // don't call this function, call overlay() instead
    //assert(false);
    return _relativePoses;
}

//virtual
const AnimPoseVec& DeepMotionNode::overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut, const AnimPoseVec& underPoses) {
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

    TickIntegration(dt);

    return _relativePoses;
}
