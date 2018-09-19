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
#ifdef DLL_WITH_DEBUG_VISU
    #include "dm_public/object_definitions/rigid_body_definition.h"
    #include "dm_public/object_definitions/collider_definitions.h"
#endif

#include "RotationConstraint.h"
#include "ResourceCache.h"
#include "Profile.h"
#include "PathUtils.h"

#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>

#include "DebugDraw.h"

static const float CHARACTER_LOAD_PRIORITY = 10.0f;
static const int ROOT_LINK_INDEX = 0;

void* Allocate(size_t dataSize)
{
    return malloc(dataSize);
}

void Free(void* data)
{
    free(data);
}

namespace
{
    std::string fixed(std::string text, int length = 20)
    {
        const int max_width = length;
        std::string part = text.substr(0, max_width);
        while (part.length() < max_width)
        {
            part += " ";
        }
        return part;
    }

    std::string to_string(const float val)
    {
        char buf[18];
        std::sprintf(buf, "% 15.7f", val);
        return std::string(buf);
    }

    std::string to_string(const avatar::Vector3 v3)
    {
        return "[ " + to_string(v3.m_V[0]) + "\t" + to_string(v3.m_V[1]) + "\t" + to_string(v3.m_V[2]) + " ]";
    }

    std::string to_string(const avatar::Quaternion v4)
    {
        return "[ " + to_string(v4.m_V[0]) + "\t" + to_string(v4.m_V[1]) + "\t" + to_string(v4.m_V[2]) + "\t" + to_string(v4.m_V[3]) + " ]";
    }

    std::string to_string(const avatar::Transform transform)
    {
        return to_string(transform.m_Position) + "\t" + to_string(transform.m_Orientation) + "\t" + to_string(transform.m_Scale);
    }

    std::string to_string(const glm::vec3 v3)
    {
        return "[ " + to_string(v3.x) + "\t" + to_string(v3.y) + "\t" + to_string(v3.z) + " ]";
    }

    std::string to_string(const glm::quat v4)
    {
        return "[ " + to_string(v4.x) + "\t" + to_string(v4.y) + "\t" + to_string(v4.z) + "\t" + to_string(v4.w) + " ]";
    }

    std::string to_string(AnimPose pose)
    {
        return to_string(pose.trans()) + "\t" + to_string(pose.rot()) + "\t" + to_string(pose.scale());
    }

    glm::vec3 toVec3_noScaling(const avatar::Vector3& avatarVec) {
        glm::vec3 vec;
        vec.x = avatarVec.m_V[0];
        vec.y = avatarVec.m_V[1];
        vec.z = avatarVec.m_V[2];
        return vec;
    }

    avatar::Vector3 toAvtVec3_noScaling(const glm::vec3& glmVec) {
        return { glmVec.x, glmVec.y, glmVec.z };
    }

    glm::vec3 toVec3(const avatar::Vector3& avatarVec) {
        return toVec3_noScaling(avatarVec) * METERS_TO_CENTIMETERS / AVATAR_SCALE;
    }

    avatar::Vector3 toAvtVec3(const glm::vec3& glmVec) {
        glm::vec3 vec = glmVec / METERS_TO_CENTIMETERS * AVATAR_SCALE;
        return toAvtVec3_noScaling(vec);
    }

    glm::quat toQuat(const avatar::Quaternion& avatarQuat) {
        glm::quat quat;
        quat.x = avatarQuat.m_V[0];
        quat.y = avatarQuat.m_V[1];
        quat.z = avatarQuat.m_V[2];
        quat.w = avatarQuat.m_V[3];
        return quat;
    }

    avatar::Quaternion toAvtQuat(const glm::quat& glmQuat) {
        return { glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w };
    }

    AnimPose toAnimPose(const avatar::Transform& avatarTransform) {
        glm::vec3 trans = toVec3(avatarTransform.m_Position);
        glm::quat ori = toQuat(avatarTransform.m_Orientation);
        glm::vec3 scale = toVec3_noScaling(avatarTransform.m_Scale);

        AnimPose newPose{ scale, ori, trans };
        return newPose;
    }

    avatar::Transform toAvtTransform(const AnimPose& animPose) {
        avatar::Transform trans;
        trans.m_Position = toAvtVec3(animPose.trans());
        trans.m_Orientation = toAvtQuat(animPose.rot());
        trans.m_Scale = toAvtVec3_noScaling(animPose.scale());
        return trans;
    }

    avatar::IHumanoidControllerHandle::BoneTarget toControllerBoneTarget(const QString& controllerBoneTargetName) {
            if(controllerBoneTargetName.compare("Head") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Head;
            else if(controllerBoneTargetName.compare("Left_Hand") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Left_Hand;
            else if(controllerBoneTargetName.compare("Right_Hand") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Right_Hand;
            else if(controllerBoneTargetName.compare("Left_Foot") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Left_Foot;
            else if(controllerBoneTargetName.compare("Right_Foot") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Right_Foot;
            else if(controllerBoneTargetName.compare("Root") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Root;
            else if(controllerBoneTargetName.compare("Left_Elbow") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Left_Elbow;
            else if(controllerBoneTargetName.compare("Right_Elbow") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Right_Elbow;
            else if(controllerBoneTargetName.compare("Left_Shoulder") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Left_Shoulder;
            else if(controllerBoneTargetName.compare("Right_Shoulder") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Right_Shoulder;
            else if(controllerBoneTargetName.compare("Left_Knee") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Left_Knee;
            else if(controllerBoneTargetName.compare("Right_Knee") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Right_Knee;
            else if(controllerBoneTargetName.compare("Left_Hip") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Left_Hip;
            else if(controllerBoneTargetName.compare("Right_Hip") == 0)
                return avatar::IHumanoidControllerHandle::BoneTarget::Right_Hip;
            else {
                qCCritical(animation) << "Bad controllerBoneTarget name: " << controllerBoneTargetName;
                return avatar::IHumanoidControllerHandle::BoneTarget::Head;
            }
    }

    std::unordered_map<std::string, avatar::Vector3> linkToFbxJointTransform = {
        { "root"             , {-0.000359838f , -0.01583385f , -0.005984783f } },
        { "pelvis_lowerback" , { 0.003543873f , -0.0486927f  , -0.008932948f } },
        { "lowerback_torso"  , { 0.003712396f , -0.1127622f  ,  0.02840054f  } },
        { "torso_head"       , { 0.0009440518f, -0.03898144f , -0.0004016161f} },
        { "head"             , { 0.005072041f , -0.2198207f  , -0.02136278f  } },
        { "backpack"         , { 0.0f         ,  0.0f        ,  0.0f         } },
        { "lHip"             , {-0.002366312f ,  0.2312979f  ,  0.01390105f  } },
        { "lKnee"            , { 0.006621502f ,  0.2065052f  ,  0.04739026f  } },
        { "lAnkle"           , {-0.01445057f  ,  0.06185609f , -0.01608679f  } },
        { "lToe"             , {-0.007985473f , -0.0392971f  , -0.01618613f  } },
        { "rHip"             , { 0.002366304f ,  0.2312979f  ,  0.01390105f  } },
        { "rKnee"            , {-0.007434346f ,  0.2063918f  ,  0.04195724f  } },
        { "rAnkle"           , { 0.01106738f  ,  0.06277531f , -0.0280784f   } },
        { "rToe"             , { 0.007094949f , -0.04029393f , -0.02847863f  } },
        { "lClav"            , {-0.04634323f  ,  0.01708317f , -0.005042613f } },
        { "lShoulder"        , {-0.1400821f   ,  0.02479744f , -0.0009180307f} },
        { "lElbow"           , {-0.1195495f   ,  0.02081084f , -0.01671298f  } },
        { "lWrist"           , {-0.04874176f  ,  0.008770943f,  0.02434396f  } },
        { "lFinger01"        , {-0.03893751f  ,  0.01506829f ,  0.04148491f  } },
        { "lFinger02"        , {-0.02153414f  ,  0.009898186f,  0.04781658f  } },
        { "rClav"            , { 0.04634323f  ,  0.01708317f , -0.005042613f } },
        { "rShoulder"        , { 0.1400821f   ,  0.02479744f , -0.0009180307f} },
        { "rElbow"           , { 0.119549f    ,  0.02081704f , -0.0167146f   } },
        { "rWrist"           , { 0.04874116f  ,  0.008776665f,  0.02434108f  } },
        { "rFinger01"        , { 0.03893763f  ,  0.0150733f  ,  0.0414814f   } },
        { "rFinger02"        , { 0.02153325f  ,  0.009903431f,  0.04781274f  } }
    };
} // anon

DeepMotionNode::IKTargetVar::IKTargetVar(
    const QString& jointNameIn, const QString& controllerBoneTargetIn, 
    const QString& targetLinkName,
    const QString& positionVar, const QString& rotationVar, 
    bool trackPosition, bool trackRotation, const QString& typeVar) :
    jointName(jointNameIn),
    controllerBoneTargetName(controllerBoneTargetIn),
    targetLinkName(targetLinkName),
    positionVar(positionVar),
    rotationVar(rotationVar),
    trackPosition(trackPosition),
    trackRotation(trackRotation),
    typeVar(typeVar) {
}

DeepMotionNode::IKTargetVar& DeepMotionNode::IKTargetVar::operator=(const IKTargetVar& other) {
    jointName = other.jointName;
    controllerBoneTargetName = other.controllerBoneTargetName;
    targetLinkName = other.targetLinkName;
    positionVar = other.positionVar;
    rotationVar = other.rotationVar;
    trackPosition = other.trackPosition;
    trackRotation = other.trackRotation;
    typeVar = other.typeVar;

    return *this;
}

DeepMotionNode::IKTarget::IKTarget(const IKTargetVar& targetVar) :
    controllerBoneTarget(toControllerBoneTarget(targetVar.controllerBoneTargetName)),
    trackPosition(targetVar.trackPosition),
    trackRotation(targetVar.trackRotation),
    jointIndex(targetVar.jointIndex) {
}

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
    for (auto& targetVar : _targetVarVec) {
        DebugDraw::getInstance().removeMyAvatarMarker("tracker_" + targetVar.controllerBoneTargetName);
#ifdef DLL_WITH_DEBUG_VISU
        if (targetVar.debugBody && _sceneHandle) {
            _sceneHandle->DeleteSceneObject(targetVar.debugBody);
            targetVar.debugBody = nullptr;
        }
#endif
    }

    _targetVarVec.clear();

    _engineInterface.FinalizeRuntime();
}

void DeepMotionNode::characterLoaded(const QByteArray& data)
{
    _sceneHandle = _engineInterface.CreateNewSceneFromJSON(reinterpret_cast<const uint8_t*>(data.constData()), data.count());
    if (!_sceneHandle) {
        qCCritical(animation) << "DeepMotionNode: Failed to initialize the scene";
        return;
    }  

    std::vector<avatar::ISceneObjectHandle*> sceneObjects;
    avatar::STDVectorArrayInterface<avatar::ISceneObjectHandle*> sceneObjectsInterface(sceneObjects);

    _sceneHandle->GetSceneObjects(sceneObjectsInterface);

    for(auto& sceneObject : sceneObjects) {
        if (sceneObject->GetType() == avatar::ISceneObjectHandle::ObjectType::MultiBody) {
            _characterHandle = static_cast<avatar::IMultiBodyHandle*>(sceneObject);

            std::vector<avatar::IMultiBodyHandle::LinkHandle> characterLinks;
            avatar::STDVectorArrayInterface<avatar::IMultiBodyHandle::LinkHandle> linksInterface(characterLinks);
            _characterHandle->GetLinkHandles(linksInterface);

            _characterLinks.reserve(characterLinks.size());
            _linkNameToIndex.reserve(characterLinks.size());
            for (int linkIndex = 0; linkIndex < characterLinks.size(); ++linkIndex) {
                auto& link = characterLinks[linkIndex];
                std::string linkName = _characterHandle->GetLinkName(link);
                _characterLinks.emplace_back(characterLinks[linkIndex], linkName, linkToFbxJointTransform.at(linkName));
                _linkNameToIndex.insert({ linkName, linkIndex });
            }
        } else if (sceneObject->GetType() == avatar::ISceneObjectHandle::ObjectType::Controller) {
            if (static_cast<avatar::IControllerHandle*>(sceneObject)->GetControllerType() == avatar::IControllerHandle::ControllerType::Biped)
                _characterController = static_cast<avatar::IHumanoidControllerHandle*>(sceneObject);
        } else if (sceneObject->GetType() == avatar::ISceneObjectHandle::ObjectType::RigidBody) {
            _groundHandle = static_cast<avatar::IRigidBodyHandle*>(sceneObject);
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
        // override Hips rotation to eliminate flying pose
        int hipsIndex = _skeleton->nameToJointIndex("Hips");
        _relativePoses[hipsIndex].rot() = Quaternions::IDENTITY;

        // cache target joints indices
        for (int linkIndex = 0; linkIndex < _characterLinks.size(); ++linkIndex) {
            _characterLinks[linkIndex].targetJointIndex = getTargetJointIndex(linkIndex);
            std::string boneName = _skeleton->getJointName(_characterLinks[linkIndex].targetJointIndex).toStdString();
            getAdditionalTargetJointIndices(boneName, _characterLinks[linkIndex].additionalTargetJointsIndices);
        }
    } else {
        _relativePoses.clear();
    }
}

void DeepMotionNode::setTargetVars(const QString& jointName, const QString& controllerBoneTarget, const QString& targetLinkName, 
                                   const QString& positionVar, const QString& rotationVar, 
                                   bool trackPosition, bool trackRotation, const QString& typeVar) {
    IKTargetVar targetVar(jointName, controllerBoneTarget, 
        targetLinkName, 
        positionVar, rotationVar, 
        trackPosition, trackRotation, typeVar);

    // if there are duplications, last one wins.
    bool found = false;
    for (auto& targetVarIter : _targetVarVec) {
        if (targetVarIter.jointName == jointName) {
            targetVarIter = targetVar;
            found = true;
            break;
        }
    }
    if (!found) {
        // create a new entry
        _targetVarVec.push_back(targetVar);
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

    if (!_skeleton || !_characterHandle)
        return underPoses;

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

    if (_relativePoses.empty())
        return underPoses;

    std::vector<IKTarget> targets;
    computeTargets(context, animVars, targets);

    for (const auto& target : targets) {

        const auto& boneTarget = target.getControllerBoneTarget();
        const auto& trackerTransform = target.getTransform();

        _characterController->SetLimbPositionTrackingEnabled(boneTarget, target.isTrackingPosition());
        if (target.isTrackingPosition())
            _characterController->SetTrackingPosition(target.getControllerBoneTarget(), trackerTransform.m_Position);
        
        _characterController->SetLimbOrientationTrackingEnabled(boneTarget, target.isTrackingRotation());
        if (target.isTrackingRotation())
            _characterController->SetTrackingOrientation(target.getControllerBoneTarget(), trackerTransform.m_Orientation);
    }

    _engineInterface.TickGeneralPurposeRuntime(dt);

    updateRelativePosesFromCharacterLinks();

    drawDebug(context);

    return _relativePoses;
}

void DeepMotionNode::overridePhysCharacterPositionAndOrientation(float floorDistance, glm::vec3& position, glm::quat& rotation) {
    static auto dmToHfCharacterShift = Vectors::MAX;

    if (!_characterHandle || _relativePoses.empty())
        return;

    auto dmCharacterPos = toVec3(_characterHandle->GetTransform().m_Position) / METERS_TO_CENTIMETERS;
    dmCharacterPos.x *= -1.0f;
    dmCharacterPos.z *= -1.0f;
    auto& dmGroundPos = toVec3(_groundHandle->GetTransform().m_Position) / METERS_TO_CENTIMETERS;

    const float dmRootToToesDistance = 1.3551f;
    auto dmGroundDistance = (dmCharacterPos - dmGroundPos).y - dmRootToToesDistance;

    if (dmToHfCharacterShift == Vectors::MAX) {

        // align position to match floor distance
        position.y += dmGroundDistance - floorDistance;

        dmToHfCharacterShift = position - dmCharacterPos;
    }

#ifdef APPLY_X_Z_MOVEMENT_TO_CHARACTER
    position = dmCharacterPos + dmToHfCharacterShift;
#else
    position.y = (dmCharacterPos + dmToHfCharacterShift).y;
#endif

    //const vec3 appliedPosition = position;
    //const vec3 dmCharPos = dmCharacterPos;
    //qCCritical(animation) << "GK_GK dmCharacterPos: " << to_string(dmCharPos).c_str();
    //qCCritical(animation) << "GK_GK dmToHfCharacterShift: " << to_string(dmToHfCharacterShift).c_str();
    //qCCritical(animation) << "GK_GK position: " << to_string(appliedPosition).c_str();
}

namespace {
    bool wasDebugDrawIKTargetsDisabledInLastFrame(const AnimContext& context) {
        static bool previousValue = context.getEnableDebugDrawIKTargets();
        
        if (previousValue && !context.getEnableDebugDrawIKTargets())
            return true;
        previousValue = context.getEnableDebugDrawIKTargets();
    }
} // anon

void DeepMotionNode::computeTargets(const AnimContext& context, const AnimVariantMap& animVars, std::vector<IKTarget>& targets) {

    static const glm::mat4 rigToAvatarMat = createMatFromQuatAndPos(Quaternions::Y_180, glm::vec3());
    static const vec4 GREEN(0.0f, 1.0f, 0.0f, 1.0f);

    const bool debugDrawIKTargetsDisabledInLastFrame = wasDebugDrawIKTargetsDisabledInLastFrame(context);

    for (auto& targetVar : _targetVarVec) {

        // update targetVar jointIndex cache
        if (targetVar.jointIndex == -1) {
            int jointIndex = _skeleton->nameToJointIndex(targetVar.jointName);
            if (jointIndex >= 0) {
                // this targetVar has a valid joint --> cache the indices
                targetVar.jointIndex = jointIndex;
            }
            else {
                qCWarning(animation) << "DeepMotionNode could not find jointName" << targetVar.jointName << "in skeleton";
            }
        }

        int targetType = (int)IKTarget::Type::Unknown;
        if (targetVar.jointIndex != -1) {
            targetType = animVars.lookup(targetVar.typeVar, (int)IKTarget::Type::Unknown);
            if (targetType == (int)IKTarget::Type::DMTracker) {
                auto linkIndex = _linkNameToIndex.at(targetVar.targetLinkName.toStdString());
                IKTarget target { targetVar };
                AnimPose absPose = _skeleton->getAbsolutePose(target.getJointIndex(), _relativePoses);

                target.setPosition(animVars.lookupRigToGeometry(targetVar.positionVar, absPose.trans()));
                target.setRotation(animVars.lookupRigToGeometry(targetVar.rotationVar, absPose.rot()));

#ifdef USE_FIX_FOR_TRACKER_ROT
                const auto& trackerRot = target.pose.rot();
                const quat& trackerRotFix = { -trackerRot.w, -trackerRot.x, -trackerRot.y, -trackerRot.z };
                target.pose.rot() = trackerRotFix;
#endif

                int rootTargetJointIndex = _characterLinks[ROOT_LINK_INDEX].targetJointIndex;
                const auto& rootTargetJointPose = _skeleton->getAbsolutePose(rootTargetJointIndex, _relativePoses);

                auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_characterLinks[linkIndex].linkToFbxJointTransform));
                auto unskinnedTrackerPose = target.pose * linkToFbxJoint.inverse();

                const auto& trackerRelToRootTarget = rootTargetJointPose.inverse() * unskinnedTrackerPose;
                const auto& dmCharTransform = toAnimPose(_characterHandle->GetTransform());

                auto& trackerPoseInDmCharSpace = dmCharTransform * trackerRelToRootTarget;
                target.transform = toAvtTransform(trackerPoseInDmCharSpace);

                // debug render ik targets
                if (context.getEnableDebugDrawIKTargets()) {
                    QString name = "tracker_" + targetVar.controllerBoneTargetName;
#ifdef DLL_WITH_DEBUG_VISU
                    if (!targetVar.debugBody) {
                        auto colliderDefinition = new avatar::BoxColliderDefinition();
                        colliderDefinition->m_HalfSize = avatar::Vector3 {0.1f, 0.1f, 0.1f};
                        avatar::RigidBodyDefinition objDefinition;
                        objDefinition.m_Collidable = false;
                        objDefinition.m_Transform = target.transform;
                        objDefinition.m_Collider = std::unique_ptr<avatar::ColliderDefinition>(colliderDefinition);

                        targetVar.debugBody = _sceneHandle->AddNewRigidBody(name.toStdString().c_str(), objDefinition);
                        // TODO: uncomment once fix for kinematic RBs will be delivered
                        //if (targetVar.debugBody)
                        //    targetVar.debugBody->SetIsKinematic(true);
                    } else {
                        targetVar.debugBody->SetTransform(target.transform);
                    }
#endif

                    glm::mat4 geomTargetMat = createMatFromQuatAndPos(target.pose.rot(), target.pose.trans());
                    glm::mat4 avatarTargetMat = rigToAvatarMat * context.getGeometryToRigMatrix() * geomTargetMat;

                    DebugDraw::getInstance().addMyAvatarMarker(name, glmExtractRotation(avatarTargetMat), extractTranslation(avatarTargetMat), GREEN);
                }

                targets.push_back(target);
            }
        }

        if (debugDrawIKTargetsDisabledInLastFrame || targetVar.jointIndex == -1 || targetType != (int)IKTarget::Type::DMTracker) {
            DebugDraw::getInstance().removeMyAvatarMarker("tracker_" + targetVar.controllerBoneTargetName);
#ifdef DLL_WITH_DEBUG_VISU
            if (targetVar.debugBody) {
                _sceneHandle->DeleteSceneObject(targetVar.debugBody);
                targetVar.debugBody = nullptr;
            }
#endif
        }
    }
}

void DeepMotionNode::updateRelativePosesFromCharacterLinks() {
    for (int linkIndex = 0; linkIndex < (int)_characterLinks.size(); ++linkIndex) {
        auto link = _characterLinks[linkIndex].linkHandle;
        int jointIndex = _characterLinks[linkIndex].targetJointIndex;

        if (jointIndex < 0)
            return;

        AnimPose linkPose = getLinkTransformInRigSpace(linkIndex);
        updateRelativePoseFromCharacterLink(linkPose, jointIndex);
        for (auto jointIndex : _characterLinks[linkIndex].additionalTargetJointsIndices)
            updateRelativePoseFromCharacterLink(linkPose, jointIndex);
    }
}

void DeepMotionNode::updateRelativePoseFromCharacterLink(const AnimPose& linkPose, int jointIndex) {
    if (jointIndex >= 0) {

        // drive rotations
        AnimPose absPose = _skeleton->getAbsolutePose(jointIndex, _relativePoses);
        AnimPose rotationOnly(linkPose.rot(), absPose.trans());

        AnimPose parentAbsPose = _skeleton->getAbsolutePose(_skeleton->getParentIndex(jointIndex), _relativePoses);
        AnimPose linkRelativePose = parentAbsPose.inverse() * rotationOnly;

        _relativePoses[jointIndex] = linkRelativePose;
    }
}

AnimPose DeepMotionNode::getLinkTransformInRigSpace(int linkIndex) const
{
    auto& linkInfo = _characterLinks[linkIndex];
    int rootTargetJointIndex = _characterLinks[ROOT_LINK_INDEX].targetJointIndex;
    const auto& rootTargetJointPose = _skeleton->getAbsolutePose(rootTargetJointIndex, _relativePoses);

    std::string linkName = linkInfo.linkName;
    if (0 == linkName.compare("root"))
        return AnimPose(toQuat(_characterHandle->GetLinkTransform(linkInfo.linkHandle).m_Orientation), rootTargetJointPose.trans());


    const auto& linkDmWorldTransform = toAnimPose(_characterHandle->GetLinkTransform(linkInfo.linkHandle));
    const auto& dmCharTransform = toAnimPose(_characterHandle->GetTransform());

    const auto& linkRelToDmChar = dmCharTransform.inverse() * linkDmWorldTransform;

    return rootTargetJointPose * linkRelToDmChar;
}

AnimPose DeepMotionNode::getFbxJointPose(int linkIndex) const {
    const auto& linkPose = getLinkTransformInRigSpace(linkIndex);

    auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_characterLinks[linkIndex].linkToFbxJointTransform));
                      // link --> fbxJoint, fbxJoint relative to link
    return linkPose * linkToFbxJoint;
}

int DeepMotionNode::getTargetJointIndex(int linkIndex) const {
    const auto targetName = _characterHandle->GetTargetBoneName(_characterLinks[linkIndex].linkHandle);
    std::string target(targetName);
    size_t right = target.find("Right");
    size_t left = target.find("Left");
    if (std::string::npos != right)
        target.replace(right, 5, "Left");
    else if (std::string::npos != left)
        target.replace(left, 4, "Right");

    return _skeleton->nameToJointIndex(QString(target.c_str()));
}

void DeepMotionNode::getAdditionalTargetJointIndices(std::string targetBoneName, std::vector<int>& additionalTargetJointIndices) const {
    std::string target(targetBoneName);
    size_t middle = target.find("Middle");

    if (std::string::npos == middle)
        return;

    std::string prefix = target.substr(0, middle);
    std::string suffix = target.substr(middle + 6, target.size());

    //qCCritical(animation) << "GK_GK additional joints for: " << targetBoneName.c_str();
    //qCCritical(animation) << "GK_GK " << (prefix + "Pinky" + suffix).c_str();
    //qCCritical(animation) << "GK_GK " << (prefix + "Ring" + suffix).c_str();
    //qCCritical(animation) << "GK_GK " << (prefix + "Index" + suffix).c_str();

    additionalTargetJointIndices.reserve(3);
    std::string pinky = prefix + "Pinky" + suffix;
    std::string ring  = prefix + "Ring" + suffix;
    std::string index = prefix + "Index" + suffix;
    additionalTargetJointIndices.push_back(_skeleton->nameToJointIndex(pinky.c_str()));
    additionalTargetJointIndices.push_back(_skeleton->nameToJointIndex(ring.c_str()));
    additionalTargetJointIndices.push_back(_skeleton->nameToJointIndex(index.c_str()));
}

AnimPose DeepMotionNode::getTargetJointAbsPose(int linkIndex) const {
    int jointIndex = _characterLinks[linkIndex].targetJointIndex;
    if (jointIndex < 0) {
        qCCritical(animation) << "Can't find target joint index for link: " << _characterLinks[linkIndex].linkName.c_str();
        return AnimPose();
    }

    return _skeleton->getAbsolutePose(jointIndex, _relativePoses);
}

void DeepMotionNode::debugDrawRelativePoses(const AnimContext& context) const {
    AnimPoseVec poses = _relativePoses;

    // convert relative poses to absolute
    _skeleton->convertRelativePosesToAbsolute(poses);

    mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();

    static const vec4 RED(1.0f, 0.0f, 0.0f, 0.5f);
    static const vec4 GREEN(0.0f, 1.0f, 0.0f, 0.5f);
    static const vec4 BLUE(0.0f, 0.0f, 1.0f, 0.5f);
    static const vec4 GRAY(0.2f, 0.2f, 0.2f, 0.5f);
    static const float AXIS_LENGTH = 3.0f; // cm

    for (int jointIndex = 0; jointIndex < (int)poses.size(); ++jointIndex) {
        // transform local axes into world space.
        auto pose = poses[jointIndex];
        glm::vec3 xAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_X);
        glm::vec3 yAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_Y);
        glm::vec3 zAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_Z);
        glm::vec3 pos = transformPoint(geomToWorldMatrix, pose.trans());
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * xAxis, RED);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * yAxis, GREEN);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * zAxis, BLUE);

        // draw line to parent
        int parentIndex = _skeleton->getParentIndex(jointIndex);
        if (parentIndex != -1) {
            glm::vec3 parentPos = transformPoint(geomToWorldMatrix, poses[parentIndex].trans());
            DebugDraw::getInstance().drawRay(pos, parentPos, GRAY);
        }

        for (int linkIndex = 0; linkIndex < _characterLinks.size(); ++linkIndex)
        {
            if (_characterLinks[linkIndex].targetJointIndex == jointIndex) {
                auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_characterLinks[linkIndex].linkToFbxJointTransform));
                auto unskinnedLinkPose = pose * linkToFbxJoint.inverse();

                glm::vec3 xAxis = transformVectorFast(geomToWorldMatrix, unskinnedLinkPose.rot() * Vectors::UNIT_X);
                glm::vec3 yAxis = transformVectorFast(geomToWorldMatrix, unskinnedLinkPose.rot() * Vectors::UNIT_Y);
                glm::vec3 zAxis = transformVectorFast(geomToWorldMatrix, unskinnedLinkPose.rot() * Vectors::UNIT_Z);
                glm::vec3 vpos = transformPoint(geomToWorldMatrix, unskinnedLinkPose.trans());
                DebugDraw::getInstance().drawRay(vpos, vpos + 2.0f * xAxis, vec4(0.8f, 0.0f, 0.0f, 1.0f));
                DebugDraw::getInstance().drawRay(vpos, vpos + 2.0f * yAxis, vec4(0.0f, 0.8f, 0.0f, 1.0f));
                DebugDraw::getInstance().drawRay(vpos, vpos + 2.0f * zAxis, vec4(0.0f, 0.0f, 0.8f, 1.0f));

                DebugDraw::getInstance().drawRay(pos, vpos, vec4(1.0f, 1.0f, 0.0f, 1.0f));
            }
        }
        
    }
}

void DeepMotionNode::drawBoxCollider(const AnimContext& context, AnimPose transform, avatar::IBoxColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals) const {

    vec3 halfSizes = toVec3(collider.GetHalfSize()) * 1.7f * transform.scale();
    vec3 pos = transformPoint(geomToWorld, transform.trans());
    quat rot = transform.rot();

    vec3 rightUpFront   = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_Y     + Vectors::UNIT_Z    );
    vec3 rightUpBack    = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_Y     + Vectors::UNIT_NEG_Z);
    vec3 leftUpFront    = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_Y     + Vectors::UNIT_Z    );
    vec3 leftUpBack     = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_Y     + Vectors::UNIT_NEG_Z);

    vec3 rightDownFront = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_NEG_Y + Vectors::UNIT_Z    );
    vec3 rightDownBack  = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_NEG_Y + Vectors::UNIT_NEG_Z);
    vec3 leftDownFront  = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_NEG_Y + Vectors::UNIT_Z    );
    vec3 leftDownBack   = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_NEG_Y + Vectors::UNIT_NEG_Z);

    vec3 colliderRightUpFront   = transformVectorFast(geomToWorld, rot * (halfSizes * rightUpFront  ));
    vec3 colliderRightUpBack    = transformVectorFast(geomToWorld, rot * (halfSizes * rightUpBack   ));
    vec3 colliderLeftUpFront    = transformVectorFast(geomToWorld, rot * (halfSizes * leftUpFront   ));
    vec3 colliderLeftUpBack     = transformVectorFast(geomToWorld, rot * (halfSizes * leftUpBack    ));
    vec3 colliderRightDownFront = transformVectorFast(geomToWorld, rot * (halfSizes * rightDownFront));
    vec3 colliderRightDownBack  = transformVectorFast(geomToWorld, rot * (halfSizes * rightDownBack ));
    vec3 colliderLeftDownFront  = transformVectorFast(geomToWorld, rot * (halfSizes * leftDownFront ));
    vec3 colliderLeftDownBack   = transformVectorFast(geomToWorld, rot * (halfSizes * leftDownBack  ));

    DebugDraw::getInstance().drawRay(pos + colliderRightUpFront,  pos + colliderRightUpBack,    color);
    DebugDraw::getInstance().drawRay(pos + colliderRightUpFront,  pos + colliderLeftUpFront,    color);
    DebugDraw::getInstance().drawRay(pos + colliderRightUpFront,  pos + colliderRightDownFront, color);
    DebugDraw::getInstance().drawRay(pos + colliderLeftDownFront, pos + colliderLeftUpFront,    color);
    DebugDraw::getInstance().drawRay(pos + colliderLeftDownFront, pos + colliderLeftDownBack,   color);
    DebugDraw::getInstance().drawRay(pos + colliderLeftDownFront, pos + colliderRightDownFront, color);
    
    DebugDraw::getInstance().drawRay(pos + colliderLeftUpBack,    pos + colliderLeftUpFront,    color);
    DebugDraw::getInstance().drawRay(pos + colliderLeftUpBack,    pos + colliderLeftDownBack,   color);
    DebugDraw::getInstance().drawRay(pos + colliderLeftUpBack,    pos + colliderRightUpBack,    color);
    DebugDraw::getInstance().drawRay(pos + colliderRightDownBack, pos + colliderLeftDownBack,   color);
    DebugDraw::getInstance().drawRay(pos + colliderRightDownBack, pos + colliderRightUpBack,    color);
    DebugDraw::getInstance().drawRay(pos + colliderRightDownBack, pos + colliderRightDownFront, color);
    
    if (drawDiagonals) {
        DebugDraw::getInstance().drawRay(pos + colliderRightUpFront, pos + colliderLeftUpBack, color);
        DebugDraw::getInstance().drawRay(pos + colliderLeftUpFront, pos + colliderRightUpBack, color);
    }
}

void DeepMotionNode::drawCompoundCollider(const AnimContext& context, AnimPose transform, avatar::ICompoundColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals) const {
    std::vector<avatar::IColliderHandle*> childHandles;
    {
        avatar::STDVectorArrayInterface<avatar::IColliderHandle*> handleInterface(childHandles);
        collider.GetChildColliders(handleInterface);
    }

    avatar::Transform childTransform;
    for (auto& childHandle : childHandles) {
        if (collider.GetChildColliderTransform(*childHandle, childTransform)) {
            drawCollider(context, transform * toAnimPose(childTransform), *childHandle, color, geomToWorld, drawDiagonals);
        }
    }
}

void DeepMotionNode::debugDrawLink(const AnimContext& context, int linkIndex) const {

    const auto& targetJointPose = getTargetJointAbsPose(linkIndex);
    const auto& linkTransformRigSpace = getLinkTransformInRigSpace(linkIndex);
    const auto& fbxJointPose = getFbxJointPose(linkIndex);

    const mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();

    const vec4 RED(0.7f, 0.0f, 0.0f, 0.5f);
    const vec4 GREEN(0.0f, 0.7f, 0.0f, 0.5f);
    const vec4 BLUE(0.0f, 0.0f, 0.7f, 0.5f);
    const vec4 ORANGE(0.9f, 0.4f, 0.0f, 1.0f);

    const float AXIS_LENGTH = 5.0f; // cm

    vec3 linkPos = transformPoint(geomToWorldMatrix, linkTransformRigSpace.trans());
    quat linkRot = linkTransformRigSpace.rot();

    vec3 xAxis = transformVectorFast(geomToWorldMatrix, linkRot * Vectors::UNIT_X);
    vec3 yAxis = transformVectorFast(geomToWorldMatrix, linkRot * Vectors::UNIT_Y);
    vec3 zAxis = transformVectorFast(geomToWorldMatrix, linkRot * Vectors::UNIT_Z);
    DebugDraw::getInstance().drawRay(linkPos, linkPos + AXIS_LENGTH * xAxis, RED);
    DebugDraw::getInstance().drawRay(linkPos, linkPos + AXIS_LENGTH * yAxis, GREEN);
    DebugDraw::getInstance().drawRay(linkPos, linkPos + AXIS_LENGTH * zAxis, BLUE);

    vec3 fbxJointPos = transformPoint(geomToWorldMatrix, fbxJointPose.trans());
    quat fbxJointRot = fbxJointPose.rot();

    vec3 fbx_xAxis = transformVectorFast(geomToWorldMatrix, fbxJointRot * -Vectors::UNIT_X);
    vec3 fbx_yAxis = transformVectorFast(geomToWorldMatrix, fbxJointRot * -Vectors::UNIT_Y);
    vec3 fbx_zAxis = transformVectorFast(geomToWorldMatrix, fbxJointRot * -Vectors::UNIT_Z);
    DebugDraw::getInstance().drawRay(fbxJointPos, fbxJointPos + AXIS_LENGTH * 0.3f * fbx_xAxis, vec4(0.4f, 0.0f, 0.0f, 0.5f));
    DebugDraw::getInstance().drawRay(fbxJointPos, fbxJointPos + AXIS_LENGTH * 0.3f * fbx_yAxis, vec4(0.0f, 0.4f, 0.0f, 0.5f));
    DebugDraw::getInstance().drawRay(fbxJointPos, fbxJointPos + AXIS_LENGTH * 0.3f * fbx_zAxis, vec4(0.0f, 0.0f, 0.4f, 0.5f));

    glm::vec3 targetJointpos = transformPoint(geomToWorldMatrix, targetJointPose.trans());
    DebugDraw::getInstance().drawRay(fbxJointPos, targetJointpos, ORANGE);

    static const std::map<std::string, glm::vec4> linkColliderColor = {
        { "root"             , glm::vec4(0.0f, 0.0f, 0.5f, 1.0f) },
        { "pelvis_lowerback" , glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) },
        { "lowerback_torso"  , glm::vec4(0.0f, 0.4f, 0.1f, 1.0f) },
        { "torso_head"       , glm::vec4(0.0f, 0.5f, 0.5f, 1.0f) },
        { "head"             , glm::vec4(0.0f, 0.5f, 1.0f, 1.0f) },
        { "backpack"         , glm::vec4(0.0f, 0.8f, 0.2f, 1.0f) },
        { "lHip"             , glm::vec4(0.0f, 0.9f, 0.6f, 1.0f) },
        { "lKnee"            , glm::vec4(0.1f, 0.4f, 0.5f, 1.0f) },
        { "lAnkle"           , glm::vec4(0.1f, 0.9f, 1.0f, 1.0f) },
        { "lToe"             , glm::vec4(0.3f, 0.0f, 0.1f, 1.0f) },
        { "rHip"             , glm::vec4(0.3f, 0.0f, 0.9f, 1.0f) },
        { "rKnee"            , glm::vec4(0.3f, 0.3f, 0.4f, 1.0f) },
        { "rAnkle"           , glm::vec4(0.3f, 0.8f, 1.0f, 1.0f) },
        { "rToe"             , glm::vec4(0.3f, 1.0f, 1.0f, 1.0f) },
        { "lClav"            , glm::vec4(0.5f, 0.0f, 0.0f, 1.0f) },
        { "lShoulder"        , glm::vec4(0.5f, 0.4f, 0.0f, 1.0f) },
        { "lElbow"           , glm::vec4(0.6f, 0.1f, 0.7f, 1.0f) },
        { "lWrist"           , glm::vec4(0.7f, 0.3f, 0.4f, 1.0f) },
        { "lFinger01"        , glm::vec4(0.7f, 0.8f, 0.5f, 1.0f) },
        { "lFinger02"        , glm::vec4(0.7f, 0.8f, 1.0f, 1.0f) },
        { "rClav"            , glm::vec4(0.8f, 0.0f, 0.0f, 1.0f) },
        { "rShoulder"        , glm::vec4(0.8f, 0.4f, 0.1f, 1.0f) },
        { "rElbow"           , glm::vec4(0.8f, 1.0f, 0.8f, 1.0f) },
        { "rWrist"           , glm::vec4(0.9f, 0.5f, 0.0f, 1.0f) },
        { "rFinger01"        , glm::vec4(0.9f, 0.4f, 0.9f, 1.0f) },
        { "rFinger02"        , glm::vec4(0.9f, 1.0f, 0.3f, 1.0f) }
        };
    
    auto linkCollider = _characterHandle->GetLinkCollider(_characterLinks[linkIndex].linkHandle);
    if (linkCollider) {
        glm::vec4 color = linkColliderColor.at(_characterLinks[linkIndex].linkName);
        drawCollider(context, linkTransformRigSpace, *linkCollider, color, geomToWorldMatrix);
    }
}

void DeepMotionNode::drawCollider(const AnimContext& context, AnimPose transform, avatar::IColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals) const {
    switch(collider.GetColliderType()) {
    case avatar::ColliderType::BOX: {
        drawBoxCollider(context, transform, static_cast<avatar::IBoxColliderHandle&>(collider), color, geomToWorld, drawDiagonals);
        break;
    }
    case avatar::ColliderType::CAPSULE: {
        qCCritical(animation) << "Trying to draw capsule collider";
        break;
    }
    case avatar::ColliderType::CYLINDER: {
        qCCritical(animation) << "Trying to draw cylinder collider";
        break;
    }
    case avatar::ColliderType::SPHERE: {
        qCCritical(animation) << "Trying to draw sphere collider";
        break;
    }
    case avatar::ColliderType::COMPOUND: {
        drawCompoundCollider(context, transform, static_cast<avatar::ICompoundColliderHandle&>(collider), color, geomToWorld, drawDiagonals);
        break;
    }
    default: {
        qCCritical(animation) << "Trying to draw unsupported type of collider";
        break;
    }
    }
}

void DeepMotionNode::debugDrawCharacter(const AnimContext& context) const {

    for (int linkIndex = 0; linkIndex < _characterLinks.size(); ++linkIndex) {
        debugDrawLink(context, linkIndex);
    }
}

void DeepMotionNode::debugDrawGround(const AnimContext& context) const {
    auto groundCollider = _groundHandle->GetCollider();
    AnimPose groundTransform = toAnimPose(_groundHandle->GetTransform());
    const glm::vec4 brown(0.4f, 0.2f, 0.0f, 1.0f);

    mat4 rigToWorldMatrix = context.getRigToWorldMatrix();
    mat4 rigToWorldMatrixNoRot = createMatFromScaleQuatAndPos(extractScale(rigToWorldMatrix), Quaternions::IDENTITY, extractTranslation(rigToWorldMatrix));

    const mat4 geomToWorldMatrix = rigToWorldMatrixNoRot * context.getGeometryToRigMatrix();

    const vec4 RED(0.7f, 0.0f, 0.0f, 0.5f);
    const vec4 GREEN(0.0f, 0.7f, 0.0f, 0.5f);
    const vec4 BLUE(0.0f, 0.0f, 0.7f, 0.5f);
    const vec4 ORANGE(0.9f, 0.4f, 0.0f, 1.0f);

    const float AXIS_LENGTH = 5.0f; // cm

    vec3 groundPos = transformPoint(geomToWorldMatrix, groundTransform.trans());
    quat groundRot = groundTransform.rot();

    vec3 xAxis = transformVectorFast(geomToWorldMatrix, groundRot * Vectors::UNIT_X);
    vec3 yAxis = transformVectorFast(geomToWorldMatrix, groundRot * Vectors::UNIT_Y);
    vec3 zAxis = transformVectorFast(geomToWorldMatrix, groundRot * Vectors::UNIT_Z);
    DebugDraw::getInstance().drawRay(groundPos, groundPos + AXIS_LENGTH * xAxis, RED);
    DebugDraw::getInstance().drawRay(groundPos, groundPos + AXIS_LENGTH * yAxis, GREEN);
    DebugDraw::getInstance().drawRay(groundPos, groundPos + AXIS_LENGTH * zAxis, BLUE);

    drawCollider(context, groundTransform, *groundCollider, brown, geomToWorldMatrix, true);
}

void DeepMotionNode::drawDebug(const AnimContext& context) {
    if (context.getEnableDebugDrawIKChains()) {
        debugDrawRelativePoses(context);
    }
    if (context.getEnableDebugDrawIKConstraints()) {
        debugDrawCharacter(context);
        debugDrawGround(context);
    }
}
