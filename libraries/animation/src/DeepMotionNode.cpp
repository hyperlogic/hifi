#include "DeepMotionNode.h"

#if DM_DLL_EXISTS
    #include "dm_interface.h"
    #include "dm_public/interfaces/i_engine_interface.h"
#else
    #include "dm_interface_stub.h"
#endif
#include "dm_public/interfaces/i_collider_handle.h"
#include "dm_public/commands/core_commands.h"
#include "dm_public/std_vector_array_interface.h"
#include "dm_public/interfaces/i_rigid_body_handle.h"

#include "RotationConstraint.h"
#include "ResourceCache.h"
#include "Profile.h"
#include "PathUtils.h"

#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>

static const float CHARACTER_LOAD_PRIORITY = 10.0f;
const float METERS_TO_CENTIMETERS = 100.0f;

void* Allocate(size_t dataSize)
{
    return malloc(dataSize);
}

void Free(void* data)
{
    free(data);
}

namespace {
    glm::vec3 toVec3(const avatar::Vector3& avatarVec) {
        glm::vec3 vec;
        vec.x = avatarVec.m_V[0];
        vec.y = avatarVec.m_V[1];
        vec.z = avatarVec.m_V[2];
        return vec;
    }

    glm::quat toQuat(const avatar::Quaternion& avatarQuat) {
        glm::quat quat;
        quat.x = avatarQuat.m_V[0];
        quat.y = avatarQuat.m_V[1];
        quat.z = avatarQuat.m_V[2];
        quat.w = avatarQuat.m_V[3];
        return quat;
    }

    AnimPose toAnimPose(const avatar::Transform& avatarTransform) {
        glm::vec3 trans = toVec3(avatarTransform.m_Position);
        glm::quat ori = toQuat(avatarTransform.m_Orientation);

        AnimPose newPose{ ori, trans * METERS_TO_CENTIMETERS };
        return newPose;
    }
} // anon

DeepMotionNode::DeepMotionNode(const QString& id) : 
    AnimNode(AnimNode::Type::DeepMotion, id),
    _engineInterface(avatar::GetEngineInterface()) {
    //const_cast<QLoggingCategory*>(&animation())->setEnabled(QtDebugMsg, true); //uncomment if you wan't to see qCDebug(animation) prints

    avatar::CoreCommands coreCommands { Allocate, Free };
    _engineInterface.RegisterCoreCommands(coreCommands);
    _engineInterface.InitializeRuntime();

    auto characterUrl = PathUtils::resourcesUrl(_characterPath);
    _characterResource = QSharedPointer<Resource>::create(characterUrl);
    _characterResource->setSelf(_characterResource);
    _characterResource->setLoadPriority(this, CHARACTER_LOAD_PRIORITY);
    connect(_characterResource.data(), &Resource::loaded, this, &DeepMotionNode::characterLoaded);
    connect(_characterResource.data(), &Resource::failed, DeepMotionNode::characterFailedToLoad);
    _characterResource->ensureLoading();
}

DeepMotionNode::~DeepMotionNode()
{
    _engineInterface.FinalizeRuntime();
}

void DeepMotionNode::characterLoaded(const QByteArray& data)
{
    qCInfo(animation) << "DeepMotionNode: Loading character";
    _sceneHandle = _engineInterface.CreateNewSceneFromJSON(reinterpret_cast<const uint8_t*>(data.constData()), data.count());
    if (!_sceneHandle) {
        qCCritical(animation) << "DeepMotionNode: Failed to initialize the scene";
        return;
    }  

    std::vector<std::unique_ptr<avatar::ISceneObjectHandle>> sceneObjects;
    avatar::STDVectorArrayInterface<std::unique_ptr<avatar::ISceneObjectHandle>> sceneObjectsInterface(sceneObjects);

    _sceneHandle->GetSceneObjects(sceneObjectsInterface);

    for(auto& object : sceneObjects) {
        const auto obj = object.get();
        const auto multiBody = dynamic_cast<avatar::IMultiBodyHandle*>(obj);
        if (multiBody) {
            _characterHandle = std::unique_ptr<avatar::IMultiBodyHandle>(dynamic_cast<avatar::IMultiBodyHandle*>(object.release()));
            
            avatar::STDVectorArrayInterface<avatar::IMultiBodyHandle::LinkHandle> linksInterface(_characterLinks);
            _characterHandle->GetLinkHandles(linksInterface);
        }
    }
}

void DeepMotionNode::characterFailedToLoad(QNetworkReply::NetworkError error)
{
    qCCritical(animation) << "DeepMotionNode: Failed to load character from resources: " << error;
}

void DeepMotionNode::loadPoses(const AnimPoseVec& poses) {
    assert(_skeleton && ((poses.size() == 0) || (_skeleton->getNumJoints() == (int)poses.size())));
    if (_skeleton->getNumJoints() == (int)poses.size()) {
        _relativePoses = poses;
    } else {
        _relativePoses.clear();
    }
}

//virtual
const AnimPoseVec& DeepMotionNode::evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) {
    // don't call this function, call overlay() instead
    assert(false);
    return _relativePoses;
}

//virtual
const AnimPoseVec& DeepMotionNode::overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut, const AnimPoseVec& underPoses) {
#ifdef Q_OS_ANDROID
    // disable IK on android
    return underPoses;
#endif

    const float MAX_OVERLAY_DT = 1.0f / 60.0f;
    if (dt > MAX_OVERLAY_DT) {
        dt = MAX_OVERLAY_DT;
    }

    if (_relativePoses.size() != underPoses.size()) {
        loadPoses(underPoses);
    } 
    //else {
    //    PROFILE_RANGE_EX(simulation_animation, "dm/relax", 0xffff00ff, 0);
    //
    //    if (!underPoses.empty()) {
    //        // Sometimes the underpose itself can violate the constraints.  Rather than
    //        // clamp the animation we dynamically expand each constraint to accomodate it.
    //        std::map<int, RotationConstraint*>::iterator constraintItr = _constraints.begin();
    //        while (constraintItr != _constraints.end()) {
    //            int index = constraintItr->first;
    //            constraintItr->second->dynamicallyAdjustLimits(underPoses[index].rot());
    //            ++constraintItr;
    //        }
    //    }
    //}

    if (_skeleton && _characterHandle && !_relativePoses.empty()) {
        _engineInterface.TickGeneralPurposeRuntime(dt);

        avatar::Transform characterTransform = _characterHandle->GetTransform();
        applyDMToHFCharacterOffset(characterTransform);
        AnimPose rootPose = toAnimPose(characterTransform);
        _relativePoses[0] = rootPose;

        for (int i = 0; i < (int)_characterLinks.size(); ++i) {
            auto link = _characterLinks[i];
            const auto targetName = _characterHandle->GetTargetBoneName(link);
            const auto boneName = _boneTargetNameToJointName[targetName];
            auto jointIndex = _skeleton->nameToJointIndex(QString(boneName.c_str()));
            
            if (jointIndex > 0) {
                AnimPose absPose = _skeleton->getAbsolutePose(jointIndex, _relativePoses);

                // drive rotations
                avatar::Transform newTransform = _characterHandle->GetLinkTransform(link);
                AnimPose newPose{ toQuat(newTransform.m_Orientation), absPose.trans() };

                AnimPose parentAbsPose = _skeleton->getAbsolutePose(_skeleton->getParentIndex(jointIndex), _relativePoses);
                newPose = parentAbsPose.inverse() * newPose;

                _relativePoses[jointIndex] = newPose;
            }
        }
    }

    return _relativePoses;
}

void DeepMotionNode::applyDMToHFCharacterOffset(avatar::Transform& characterTransform) {
    characterTransform.m_Position.m_V[1] -= 2.0f;
}
