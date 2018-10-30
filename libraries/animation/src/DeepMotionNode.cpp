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

static const glm::mat4 DM_TO_HF_MATRIX = createMatFromQuatAndPos(Quaternions::Y_180, {0.0f, -11.48f, 0.0f});

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

#ifdef ENABLE_PRINTING

    std::string fixed(std::string text, size_t length = 20)
    {
        const size_t max_width = length;
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
#endif

    glm::vec3 toVec3_noScaling(const avatar::Vector3& avatarVec) {
        glm::vec3 vec;
        vec.x = avatarVec.m_V[0];
        vec.y = avatarVec.m_V[1];
        vec.z = avatarVec.m_V[2];
        return vec;
    }

    avatar::Vector3 toAvtVec3_noScaling(const glm::vec3& glmVec) {
        return {{glmVec.x, glmVec.y, glmVec.z}};
    }

    glm::vec3 toVec3(const avatar::Vector3& avatarVec) {
        return toVec3_noScaling(avatarVec) * AVATAR_SCALE_INV;
    }

    avatar::Vector3 toAvtVec3(const glm::vec3& glmVec) {
        glm::vec3 vec = glmVec * AVATAR_SCALE;
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
        return {{glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w}};
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
        { "root"             , {{-0.000359838f , -0.01583385f , -0.005984783f }} },
        { "pelvis_lowerback" , {{ 0.003543873f , -0.0486927f  , -0.008932948f }} },
        { "lowerback_torso"  , {{ 0.003712396f , -0.1127622f  ,  0.02840054f  }} },
        { "torso_head"       , {{ 0.0009440518f, -0.03898144f , -0.0004016161f}} },
        { "head"             , {{ 0.005072041f , -0.2198207f  , -0.02136278f  }} },
        { "backpack"         , {{ 0.0f         ,  0.0f        ,  0.0f         }} },
        { "lHip"             , {{-0.002366312f ,  0.2312979f  ,  0.01390105f  }} },
        { "lKnee"            , {{ 0.006621502f ,  0.2065052f  ,  0.04739026f  }} },
        { "lAnkle"           , {{-0.01445057f  ,  0.06185609f , -0.01608679f  }} },
        { "lToe"             , {{-0.007985473f , -0.0392971f  , -0.01618613f  }} },
        { "rHip"             , {{ 0.002366304f ,  0.2312979f  ,  0.01390105f  }} },
        { "rKnee"            , {{-0.007434346f ,  0.2063918f  ,  0.04195724f  }} },
        { "rAnkle"           , {{ 0.01106738f  ,  0.06277531f , -0.0280784f   }} },
        { "rToe"             , {{ 0.007094949f , -0.04029393f , -0.02847863f  }} },
        { "lClav"            , {{-0.04634323f  ,  0.01708317f , -0.005042613f }} },
        { "lShoulder"        , {{-0.1400821f   ,  0.02479744f , -0.0009180307f}} },
        { "lElbow"           , {{-0.1195495f   ,  0.02081084f , -0.01671298f  }} },
        { "lWrist"           , {{-0.04874176f  ,  0.008770943f,  0.02434396f  }} },
        { "lFinger01"        , {{-0.03893751f  ,  0.01506829f ,  0.04148491f  }} },
        { "lFinger02"        , {{-0.02153414f  ,  0.009898186f,  0.04781658f  }} },
        { "rClav"            , {{ 0.04634323f  ,  0.01708317f , -0.005042613f }} },
        { "rShoulder"        , {{ 0.1400821f   ,  0.02479744f , -0.0009180307f}} },
        { "rElbow"           , {{ 0.119549f    ,  0.02081704f , -0.0167146f   }} },
        { "rWrist"           , {{ 0.04874116f  ,  0.008776665f,  0.02434108f  }} },
        { "rFinger01"        , {{ 0.03893763f  ,  0.0150733f  ,  0.0414814f   }} },
        { "rFinger02"        , {{ 0.02153325f  ,  0.009903431f,  0.04781274f  }} }
    };
                          // 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
    int linkParentIndex[] {  0,  0,  1,  2,  3,  0,  5,  6,  6,  8,  9, 10, 11, 12,  6, 14, 15, 16, 17, 18,  6, 20,  0, 22, 23, 24};
} // anon

DeepMotionNode::IKTargetVar::IKTargetVar(
    const QString& jointNameIn, const QString& controllerBoneTargetIn, 
    const QString& targetLinkName,
    const QString& positionVar, const QString& rotationVar, 
    bool trackPosition, bool trackRotation, const QString& typeVar) :
    trackPosition(trackPosition),
    trackRotation(trackRotation),
    jointName(jointNameIn),
    controllerBoneTargetName(controllerBoneTargetIn),
    targetLinkName(targetLinkName),
    positionVar(positionVar),
    rotationVar(rotationVar),
    typeVar(typeVar) {
}

DeepMotionNode::IKTargetVar& DeepMotionNode::IKTargetVar::operator=(const IKTargetVar& other) {
    trackPosition = other.trackPosition;
    trackRotation = other.trackRotation;
    jointName = other.jointName;
    controllerBoneTargetName = other.controllerBoneTargetName;
    targetLinkName = other.targetLinkName;
    positionVar = other.positionVar;
    rotationVar = other.rotationVar;
    typeVar = other.typeVar;

    return *this;
}

avatar::IHumanoidControllerHandle::BoneTarget DeepMotionNode::IKTargetVar::getControllerBoneTarget() const {
    return toControllerBoneTarget(controllerBoneTargetName);
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

DeepMotionNode::~DeepMotionNode() {
    for (auto& targetVar : _targetVarVec) {
        DebugDraw::getInstance().removeMyAvatarMarker("tracker_" + targetVar.getControllerBoneTargetName());
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

void DeepMotionNode::characterLoaded(const QByteArray& data) {
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
            for (auto linkIndex = 0u; linkIndex < characterLinks.size(); ++linkIndex) {
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

void DeepMotionNode::characterFailedToLoad(QNetworkReply::NetworkError error) {
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
        for (auto linkIndex = 0u; linkIndex < _characterLinks.size(); ++linkIndex) {
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
        if (targetVarIter.getJointName() == jointName) {
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

    _rigToWorldMatrix = context.getRigToWorldMatrix();
    _geomToRigMatrix = context.getGeometryToRigMatrix();

    const float MAX_OVERLAY_DT = 1.0f / 60.0f;
    if (dt > MAX_OVERLAY_DT) {
        dt = MAX_OVERLAY_DT;
    }

    if (!_skeleton || !_characterHandle)
        return underPoses;

    if (_relativePoses.size() != underPoses.size())
        loadPoses(underPoses);
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

    computeTargets(context, animVars);

    _engineInterface.TickGeneralPurposeRuntime(dt);

    updateRelativePosesFromCharacterLinks();

    drawDebug(context);

    return _relativePoses;
}

void DeepMotionNode::overridePhysCharacterPositionAndOrientation(float floorDistance, glm::vec3& position, glm::quat& rotation) {
    static auto dmToHfCharacterShift = Vectors::MAX;
    const float maxFloorDistance = 1000.0f; // for couple first calls the floorDistance have a very high value, than it jumps to 1.5

    if (!_characterHandle || _relativePoses.empty())
        return;

    if (floorDistance > maxFloorDistance)
        return;

    const auto dmCharacterPos = transformPoint(DM_TO_HF_MATRIX, toAnimPose(_characterHandle->GetTransform()).trans());
    float rotationAngle = glm::angle(rotation);
    const glm::vec3 rotationAxis = glm::axis(rotation);
    if (rotationAxis.y < 0.0f)
        rotationAngle = 2.0f * PI - rotationAngle;

    position = dmCharacterPos;
    if (_characterController)
        _characterController->SetDesiredHeading(rotationAngle);
}

namespace {
    bool wasDebugDrawIKTargetsDisabledInLastFrame(const AnimContext& context) {
        static bool previousValue = context.getEnableDebugDrawIKTargets();
        
        if (previousValue && !context.getEnableDebugDrawIKTargets())
            return true;
        previousValue = context.getEnableDebugDrawIKTargets();
        return false;
    }
} // anon

void DeepMotionNode::computeTargets(const AnimContext& context, const AnimVariantMap& animVars) {
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

        int targetType = (int)IKTargetVar::IKTargetType::Unknown;
        if (targetVar.jointIndex != -1) {
            targetType = animVars.lookup(targetVar.typeVar, (int)IKTargetVar::IKTargetType::Unknown);
            if (targetType == (int)IKTargetVar::IKTargetType::DMTracker) {
                auto linkIndex = _linkNameToIndex.at(targetVar.targetLinkName.toStdString());
                AnimPose absPose = _skeleton->getAbsolutePose(targetVar.jointIndex, _relativePoses);

                targetVar.setPosition(animVars.lookupRigToGeometry(targetVar.positionVar, absPose.trans()));
                targetVar.setRotation(animVars.lookupRigToGeometry(targetVar.rotationVar, absPose.rot()));

                const auto trackerInGeomSpace = targetVar.pose;
                const auto trackerInHFWorldSpace = AnimPose(context.getRigToWorldMatrix() * context.getGeometryToRigMatrix()) * trackerInGeomSpace;

                const auto trackerInDMWorldSpace = AnimPose(DM_TO_HF_MATRIX).inverse() * trackerInHFWorldSpace;

                auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_characterLinks[linkIndex].linkToFbxJointTransform));
                const auto unskinnedTracker = linkToFbxJoint.inverse() * trackerInDMWorldSpace;

                targetVar.transform = toAvtTransform(unskinnedTracker);

                // debug render ik targets
                if (context.getEnableDebugDrawIKTargets()) {
                    const QString name = "tracker_" + targetVar.controllerBoneTargetName;
#ifdef DLL_WITH_DEBUG_VISU
                    if (!targetVar.debugBody) {
                        auto colliderDefinition = new avatar::BoxColliderDefinition();
                        colliderDefinition->m_HalfSize = avatar::Vector3 {0.5f, 0.5f, 0.5f};
                        avatar::RigidBodyDefinition objDefinition;
                        objDefinition.m_Collidable = false;
                        objDefinition.m_Transform = targetVar.transform;
                        objDefinition.m_Collider = std::unique_ptr<avatar::ColliderDefinition>(colliderDefinition);

                        targetVar.debugBody = _sceneHandle->AddNewRigidBody(name.toStdString().c_str(), objDefinition);
                        // TODO: uncomment once fix for kinematic RBs will be delivered
                        //if (targetVar.debugBody)
                        //    targetVar.debugBody->SetIsKinematic(true);
                    } else {
                        targetVar.debugBody->SetTransform(targetVar.transform);
                    }
#endif

                    glm::mat4 geomTargetMat = createMatFromQuatAndPos(targetVar.pose.rot(), targetVar.pose.trans());
                    static const glm::mat4 rigToAvatarMat = createMatFromQuatAndPos(Quaternions::Y_180, glm::vec3());
                    glm::mat4 avatarTargetMat = rigToAvatarMat * context.getGeometryToRigMatrix() * geomTargetMat;

                    static const glm::vec4 GREEN(0.0f, 1.0f, 0.0f, 1.0f);
                    DebugDraw::getInstance().addMyAvatarMarker(name, glmExtractRotation(avatarTargetMat), extractTranslation(avatarTargetMat), GREEN);
                }

                const auto& boneTarget = targetVar.getControllerBoneTarget();
                const auto& trackerTransform = targetVar.transform;

                _characterController->SetLimbPositionTrackingEnabled(boneTarget, targetVar.trackPosition);
                if (targetVar.trackPosition)
                    _characterController->SetTrackingPosition(boneTarget, trackerTransform.m_Position);
                
                if (boneTarget != avatar::IHumanoidControllerHandle::BoneTarget::Root) {
                    _characterController->SetLimbOrientationTrackingEnabled(boneTarget, targetVar.trackRotation);
                    if (targetVar.trackRotation)
                        _characterController->SetTrackingOrientation(boneTarget, trackerTransform.m_Orientation);
                }
            }
        }

        if (debugDrawIKTargetsDisabledInLastFrame || targetVar.jointIndex == -1 || targetType != (int)IKTargetVar::IKTargetType::DMTracker) {
            DebugDraw::getInstance().removeMyAvatarMarker("tracker_" + targetVar.controllerBoneTargetName);
#ifdef DLL_WITH_DEBUG_VISU
            if (targetVar.debugBody) {
                _sceneHandle->DeleteSceneObject(targetVar.debugBody);
                targetVar.debugBody = nullptr;
            }
#endif
        }

        if (targetVar.jointIndex == -1 || targetType != (int)IKTargetVar::IKTargetType::DMTracker) {
            const auto& boneTarget = targetVar.getControllerBoneTarget();
            _characterController->SetLimbPositionTrackingEnabled(boneTarget, false);
            if (boneTarget != avatar::IHumanoidControllerHandle::BoneTarget::Root)
                _characterController->SetLimbOrientationTrackingEnabled(boneTarget, false);
        }
    }
}

void DeepMotionNode::updateRelativePosesFromCharacterLinks() {
    for (int linkIndex = 0; linkIndex < (int)_characterLinks.size(); ++linkIndex) {
        int jointIndex = _characterLinks[linkIndex].targetJointIndex;

        if (jointIndex < 0)
            return;

        AnimPose linkPose = getLinkTransformInGeomSpace(linkIndex);
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

AnimPose DeepMotionNode::getLinkTransformInHFWorldSpace(int linkIndex) const {
    auto& linkInfo = _characterLinks[linkIndex];
    return AnimPose(DM_TO_HF_MATRIX) * toAnimPose(_characterHandle->GetLinkTransform(linkInfo.linkHandle));
}

AnimPose DeepMotionNode::getLinkTransformInGeomSpace(int linkIndex) const {
    const auto linkInWorld = getLinkTransformInHFWorldSpace(linkIndex);
    return AnimPose(inverse(_rigToWorldMatrix * _geomToRigMatrix)) * linkInWorld;
}

AnimPose DeepMotionNode::getSkinnedLinkTransformInHFWorldSpace(int linkIndex) const {
    auto& linkInfo = _characterLinks[linkIndex];
    const auto linkTransform = toAnimPose(_characterHandle->GetLinkTransform(linkInfo.linkHandle));
    auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(linkInfo.linkToFbxJointTransform));
    // link --> fbxJoint, fbxJoint relative to link
    const auto skinnedLink = linkTransform * linkToFbxJoint;

    return AnimPose(DM_TO_HF_MATRIX) * skinnedLink;
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

    additionalTargetJointIndices.reserve(3);
    std::string pinky = prefix + "Pinky" + suffix;
    std::string ring  = prefix + "Ring" + suffix;
    std::string index = prefix + "Index" + suffix;
    additionalTargetJointIndices.push_back(_skeleton->nameToJointIndex(pinky.c_str()));
    additionalTargetJointIndices.push_back(_skeleton->nameToJointIndex(ring.c_str()));
    additionalTargetJointIndices.push_back(_skeleton->nameToJointIndex(index.c_str()));
}

AnimPose DeepMotionNode::getTargetJointAbsPose(int linkIndex) const {
    const int jointIndex = _characterLinks[linkIndex].targetJointIndex;
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

    const glm::mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();

    static const glm::vec4 RED(1.0f, 0.0f, 0.0f, 0.5f);
    static const glm::vec4 GREEN(0.0f, 1.0f, 0.0f, 0.5f);
    static const glm::vec4 BLUE(0.0f, 0.0f, 1.0f, 0.5f);
    static const glm::vec4 GRAY(0.2f, 0.2f, 0.2f, 0.5f);
    static const float AXIS_LENGTH = 5.0f; // cm

    for (int jointIndex = 0; jointIndex < (int)poses.size(); ++jointIndex) {

        // transform local axes into world space.
        auto pose = poses[jointIndex];
        const glm::vec3 xAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_X);
        const glm::vec3 yAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_Y);
        const glm::vec3 zAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_Z);
        const glm::vec3 pos = transformPoint(geomToWorldMatrix, pose.trans());
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * xAxis, RED);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * yAxis, GREEN);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * zAxis, BLUE);

        // draw line to parent
        const int parentIndex = _skeleton->getParentIndex(jointIndex);
        if (parentIndex != -1) {
            const glm::vec3 parentPos = transformPoint(geomToWorldMatrix, poses[parentIndex].trans());
            DebugDraw::getInstance().drawRay(pos, parentPos, GRAY);
        }
    }
}

void DeepMotionNode::drawBoxCollider(const AnimContext& context, const AnimPose& transform, avatar::IBoxColliderHandle& collider, const glm::vec4& color, bool drawDiagonals) const {
    const glm::vec3 halfSizes = toVec3_noScaling(collider.GetHalfSize()) * 1.23f * transform.scale();
    const glm::vec3 pos = transformPoint(DM_TO_HF_MATRIX, transform.trans());
    const glm::quat rot = transform.rot();

    const glm::vec3 rightUpFront   = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_Y     + Vectors::UNIT_Z    );
    const glm::vec3 rightUpBack    = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_Y     + Vectors::UNIT_NEG_Z);
    const glm::vec3 leftUpFront    = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_Y     + Vectors::UNIT_Z    );
    const glm::vec3 leftUpBack     = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_Y     + Vectors::UNIT_NEG_Z);

    const glm::vec3 rightDownFront = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_NEG_Y + Vectors::UNIT_Z    );
    const glm::vec3 rightDownBack  = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_NEG_Y + Vectors::UNIT_NEG_Z);
    const glm::vec3 leftDownFront  = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_NEG_Y + Vectors::UNIT_Z    );
    const glm::vec3 leftDownBack   = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_NEG_Y + Vectors::UNIT_NEG_Z);

    const glm::vec3 colliderRightUpFront   = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * rightUpFront  ));
    const glm::vec3 colliderRightUpBack    = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * rightUpBack   ));
    const glm::vec3 colliderLeftUpFront    = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * leftUpFront   ));
    const glm::vec3 colliderLeftUpBack     = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * leftUpBack    ));
    const glm::vec3 colliderRightDownFront = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * rightDownFront));
    const glm::vec3 colliderRightDownBack  = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * rightDownBack ));
    const glm::vec3 colliderLeftDownFront  = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * leftDownFront ));
    const glm::vec3 colliderLeftDownBack   = transformVectorFast(DM_TO_HF_MATRIX, rot * (halfSizes * leftDownBack  ));

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

void DeepMotionNode::drawCompoundCollider(const AnimContext& context, const AnimPose& transform, avatar::ICompoundColliderHandle& collider, const glm::vec4& color, bool drawDiagonals) const {
    std::vector<avatar::IColliderHandle*> childHandles;
    {
        avatar::STDVectorArrayInterface<avatar::IColliderHandle*> handleInterface(childHandles);
        collider.GetChildColliders(handleInterface);
    }

    avatar::Transform childTransform;
    for (auto& childHandle : childHandles) {
        if (collider.GetChildColliderTransform(*childHandle, childTransform)) {
            drawCollider(context, transform * toAnimPose(childTransform), *childHandle, color, drawDiagonals);
        }
    }
}

void DeepMotionNode::debugDrawLink(const AnimContext& context, int linkIndex) const {
    const auto& targetJointPose = getTargetJointAbsPose(linkIndex);

    const auto linkInfo = _characterLinks[linkIndex];
    const auto linkTransform = toAnimPose(_characterHandle->GetLinkTransform(linkInfo.linkHandle));

    const glm::vec4 RED(0.7f, 0.0f, 0.0f, 0.5f);
    const glm::vec4 GREEN(0.0f, 0.7f, 0.0f, 0.5f);
    const glm::vec4 BLUE(0.0f, 0.0f, 0.7f, 0.5f);
    const glm::vec4 ORANGE(0.9f, 0.4f, 0.0f, 1.0f);
    const glm::vec4 MAGENTA(1.0f, 0.0f, 1.0f, 1.0f);

    const float AXIS_LENGTH = 0.05f;

    // draw link position
    {
        const glm::vec3 xAxis = transformVectorFast(DM_TO_HF_MATRIX, linkTransform.rot() * Vectors::UNIT_X);
        const glm::vec3 yAxis = transformVectorFast(DM_TO_HF_MATRIX, linkTransform.rot() * Vectors::UNIT_Y);
        const glm::vec3 zAxis = transformVectorFast(DM_TO_HF_MATRIX, linkTransform.rot() * Vectors::UNIT_Z);
        const glm::vec3 pos = transformPoint(DM_TO_HF_MATRIX, linkTransform.trans());
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * xAxis, RED);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * yAxis, GREEN);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * zAxis, BLUE);

        // draw line from link to parent
        const auto parentInfo = _characterLinks[linkParentIndex[linkIndex]];
        const glm::vec3 parentLinkPos = toAnimPose(_characterHandle->GetLinkTransform(parentInfo.linkHandle)).trans();
        const glm::vec3 parentPos = transformPoint(DM_TO_HF_MATRIX, parentLinkPos);
        DebugDraw::getInstance().drawRay(pos, parentPos, MAGENTA);
    }

    // draw fbx joint position
    {
        const auto skinnedLinkInWorld = getSkinnedLinkTransformInHFWorldSpace(linkIndex);
    
        const glm::vec3 fbx_xAxis = skinnedLinkInWorld.rot() * -Vectors::UNIT_X;
        const glm::vec3 fbx_yAxis = skinnedLinkInWorld.rot() * -Vectors::UNIT_Y;
        const glm::vec3 fbx_zAxis = skinnedLinkInWorld.rot() * -Vectors::UNIT_Z;
        const glm::vec3 fbx_pos = skinnedLinkInWorld.trans();
        DebugDraw::getInstance().drawRay(fbx_pos, fbx_pos + AXIS_LENGTH * 0.6f * fbx_xAxis, vec4(0.4f, 0.0f, 0.0f, 0.5f));
        DebugDraw::getInstance().drawRay(fbx_pos, fbx_pos + AXIS_LENGTH * 0.6f * fbx_yAxis, vec4(0.0f, 0.4f, 0.0f, 0.5f));
        DebugDraw::getInstance().drawRay(fbx_pos, fbx_pos + AXIS_LENGTH * 0.6f * fbx_zAxis, vec4(0.0f, 0.0f, 0.4f, 0.5f));
    
        // draw line from link to joint
        const glm::mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();
        const glm::vec3 targetJointpos = transformPoint(geomToWorldMatrix, targetJointPose.trans());
        DebugDraw::getInstance().drawRay(fbx_pos, targetJointpos, ORANGE);
        DebugDraw::getInstance().drawRay(fbx_pos, transformPoint(DM_TO_HF_MATRIX, linkTransform.trans()), ORANGE);
    }

    // draw colliders
    {
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
            const glm::vec4 color = linkColliderColor.at(_characterLinks[linkIndex].linkName);
            drawCollider(context, linkTransform, *linkCollider, color);
        }
    }
}

void DeepMotionNode::drawCollider(const AnimContext& context, const AnimPose& transform, avatar::IColliderHandle& collider, const glm::vec4& color, bool drawDiagonals) const {
    switch(collider.GetColliderType()) {
    case avatar::ColliderType::BOX: {
        drawBoxCollider(context, transform, static_cast<avatar::IBoxColliderHandle&>(collider), color, drawDiagonals);
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
        drawCompoundCollider(context, transform, static_cast<avatar::ICompoundColliderHandle&>(collider), color, drawDiagonals);
        break;
    }
    default: {
        qCCritical(animation) << "Trying to draw unsupported type of collider";
        break;
    }
    }
}

void DeepMotionNode::debugDrawCharacter(const AnimContext& context) const {
    for (auto linkIndex = 0u; linkIndex < _characterLinks.size(); ++linkIndex) {
            debugDrawLink(context, linkIndex);
    }
}

void DeepMotionNode::debugDrawGround(const AnimContext& context) const {
    auto groundCollider = _groundHandle->GetCollider();
    const auto groundTransform = toAnimPose(_groundHandle->GetTransform());
    const glm::vec4 brown(0.4f, 0.2f, 0.0f, 1.0f);

    const glm::vec4 RED(0.7f, 0.0f, 0.0f, 0.5f);
    const glm::vec4 GREEN(0.0f, 0.7f, 0.0f, 0.5f);
    const glm::vec4 BLUE(0.0f, 0.0f, 0.7f, 0.5f);
    const glm::vec4 ORANGE(0.9f, 0.4f, 0.0f, 1.0f);

    const float AXIS_LENGTH = 0.05f;

    const glm::vec3 groundPos = transformPoint(DM_TO_HF_MATRIX, groundTransform.trans());
    const glm::quat groundRot = groundTransform.rot();

    const glm::vec3 xAxis = transformVectorFast(DM_TO_HF_MATRIX, groundRot * Vectors::UNIT_X);
    const glm::vec3 yAxis = transformVectorFast(DM_TO_HF_MATRIX, groundRot * Vectors::UNIT_Y);
    const glm::vec3 zAxis = transformVectorFast(DM_TO_HF_MATRIX, groundRot * Vectors::UNIT_Z);
    DebugDraw::getInstance().drawRay(groundPos, groundPos + AXIS_LENGTH * xAxis, RED);
    DebugDraw::getInstance().drawRay(groundPos, groundPos + AXIS_LENGTH * yAxis, GREEN);
    DebugDraw::getInstance().drawRay(groundPos, groundPos + AXIS_LENGTH * zAxis, BLUE);

    drawCollider(context, groundTransform, *groundCollider, brown, true);
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
