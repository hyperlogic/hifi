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

#include "DebugDraw.h"

static const float CHARACTER_LOAD_PRIORITY = 10.0f;

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
}

namespace {
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

    avatar::IMultiBodyHandle::LinkHandle& getLinkByName(
        const QString& name, 
        avatar::IMultiBodyHandle* characterHandle, 
        std::vector<avatar::IMultiBodyHandle::LinkHandle>& characterLinks) {

        if (characterHandle) {
            for (auto& link : characterLinks) {
                if (name.compare(characterHandle->GetLinkName(link)) == 0)
                    return link;
            }
        }
        qCCritical(animation) << "Can't find link with name: " << name;
        return avatar::IMultiBodyHandle::LinkHandle(nullptr);
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

DeepMotionNode::IKTarget::IKTarget(const IKTargetVar& targetVar, avatar::IMultiBodyHandle::LinkHandle& link) :
    controllerBoneTarget(toControllerBoneTarget(targetVar.controllerBoneTargetName)),
    targetLink(link),
    trackPosition(targetVar.trackPosition),
    trackRotation(targetVar.trackRotation),
    jointIndex(targetVar.jointIndex) {
}

avatar::Transform DeepMotionNode::IKTarget::calculateTransform()
{
    return avatar::Transform();
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
    for (const auto& targetVar : _targetVarVec)
        DebugDraw::getInstance().removeMyAvatarMarker("tracker_" + targetVar.controllerBoneTargetName);

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

            avatar::STDVectorArrayInterface<avatar::IMultiBodyHandle::LinkHandle> linksInterface(_characterLinks);
            _characterHandle->GetLinkHandles(linksInterface);
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

    _rigToWorldMatrix = context.getRigToWorldMatrix();
    _geometryToRigMatrix = context.getGeometryToRigMatrix();

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
    computeTargets(animVars, targets, context.getEnableDebugDrawIKTargets());

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

#if APPLY_X_Z_MOVEMENT_TO_CHARACTER
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

void DeepMotionNode::computeTargets(const AnimVariantMap& animVars, std::vector<IKTarget>& targets, bool drawDebug) {

    glm::mat4 rigToAvatarMat = createMatFromQuatAndPos(Quaternions::Y_180, glm::vec3());
    const vec4 GREEN(0.0f, 1.0f, 0.0f, 1.0f);

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
                IKTarget target { targetVar, getLinkByName(targetVar.targetLinkName, _characterHandle, _characterLinks) };
                AnimPose absPose = _skeleton->getAbsolutePose(target.getJointIndex(), _relativePoses);

                target.setPosition(animVars.lookupRigToGeometry(targetVar.positionVar, absPose.trans()));
                target.setRotation(animVars.lookupRigToGeometry(targetVar.rotationVar, absPose.rot()));

#if USE_FIX_FOR_TRACKER_ROT
                const auto& trackerRot = target.pose.rot();
                const quat& trackerRotFix = { -trackerRot.w, -trackerRot.x, -trackerRot.y, -trackerRot.z };
                target.pose.rot() = trackerRotFix;
#endif

                const avatar::IMultiBodyHandle::LinkHandle& rootLink = _characterLinks[0];
                int rootTargetJointIndex = getTargetJointIndex(rootLink);
                const auto& rootTargetJointPose = _skeleton->getAbsolutePose(rootTargetJointIndex, _relativePoses);
                //{ // getLinkTransformInRigSpace
                //
                //    std::string linkName = _characterHandle->GetLinkName(link);
                //    if (0 == linkName.compare("root"))
                //        auto ret = AnimPose(toQuat(_characterHandle->GetLinkTransform(link).m_Orientation), rootTargetJointPose.trans());
                //
                //
                //    const auto& linkDmWorldTransform = toAnimPose(_characterHandle->GetLinkTransform(link));
                //    const auto& dmCharTransform = toAnimPose(_characterHandle->GetTransform());
                //
                //    const auto& linkRelToDmChar = dmCharTransform.inverse() * linkDmWorldTransform;
                //
                //    auto ret = rootTargetJointPose * linkRelToDmChar;
                //}

                auto linkName = _characterHandle->GetLinkName(target.getTargetLink());
                auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_linkToFbxJointTransform.at(linkName)));
                auto unskinnedTrackerPose = target.pose * linkToFbxJoint.inverse();

                const auto& trackerRelToRootTarget = rootTargetJointPose.inverse() * unskinnedTrackerPose;
                const auto& dmCharTransform = toAnimPose(_characterHandle->GetTransform());

                auto& trackerPoseInDmCharSpace = dmCharTransform * trackerRelToRootTarget;

#if USE_MAX_TRACKER_DISTANCE_PER_AXIS

#endif

                target.transform = toAvtTransform(trackerPoseInDmCharSpace);

                // debug render ik targets
                if (drawDebug) {

                    glm::mat4 geomTargetMat = createMatFromQuatAndPos(target.pose.rot(), target.pose.trans());
                    glm::mat4 avatarTargetMat = rigToAvatarMat * _geometryToRigMatrix * geomTargetMat;

                    DebugDraw::getInstance().addMyAvatarMarker("tracker_" + targetVar.controllerBoneTargetName, glmExtractRotation(avatarTargetMat), extractTranslation(avatarTargetMat), GREEN);
                }

                targets.push_back(target);
            }
        }

        if (!drawDebug || targetVar.jointIndex == -1 || targetType != (int)IKTarget::Type::DMTracker)
            DebugDraw::getInstance().removeMyAvatarMarker("tracker_" + targetVar.controllerBoneTargetName);
    }
}

void DeepMotionNode::updateRelativePosesFromCharacterLinks() {
    for (int i = 0; i < (int)_characterLinks.size(); ++i) {
        auto link = _characterLinks[i];
        auto jointIndex = getTargetJointIndex(link);

        if (jointIndex >= 0) {
            AnimPose linkPose = getLinkTransformInRigSpace(link);

            // drive rotations
            AnimPose absPose = _skeleton->getAbsolutePose(jointIndex, _relativePoses);
            AnimPose rotationOnly(linkPose.rot(), absPose.trans());

            AnimPose parentAbsPose = _skeleton->getAbsolutePose(_skeleton->getParentIndex(jointIndex), _relativePoses);
            AnimPose linkRelativePose = parentAbsPose.inverse() * rotationOnly;

            _relativePoses[jointIndex] = linkRelativePose;
        }
    }
}

AnimPose DeepMotionNode::getLinkTransformInRigSpace(const avatar::IMultiBodyHandle::LinkHandle& link) const
{
    const avatar::IMultiBodyHandle::LinkHandle& rootLink = _characterLinks[0];
    int rootTargetJointIndex = getTargetJointIndex(rootLink);
    const auto& rootTargetJointPose = _skeleton->getAbsolutePose(rootTargetJointIndex, _relativePoses);

    std::string linkName = _characterHandle->GetLinkName(link);
    if (0 == linkName.compare("root"))
        return AnimPose(toQuat(_characterHandle->GetLinkTransform(link).m_Orientation), rootTargetJointPose.trans());


    const auto& linkDmWorldTransform = toAnimPose(_characterHandle->GetLinkTransform(link));
    const auto& dmCharTransform = toAnimPose(_characterHandle->GetTransform());

    const auto& linkRelToDmChar = dmCharTransform.inverse() * linkDmWorldTransform;

    return rootTargetJointPose * linkRelToDmChar;
}

AnimPose DeepMotionNode::getFbxJointPose(const avatar::IMultiBodyHandle::LinkHandle& link) const {
    const auto& linkPose = getLinkTransformInRigSpace(link);

    auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_linkToFbxJointTransform.at(_characterHandle->GetLinkName(link))));
                      // link --> fbxJoint, fbxJoint relative to link
    return linkPose * linkToFbxJoint;
}

int DeepMotionNode::getTargetJointIndex(const avatar::IMultiBodyHandle::LinkHandle& link) const {
    const auto targetName = _characterHandle->GetTargetBoneName(link);
    std::string target(targetName);
    size_t right = target.find("Right");
    size_t left = target.find("Left");
    if (std::string::npos != right)
        target.replace(right, 5, "Left");
    else if (std::string::npos != left)
        target.replace(left, 4, "Right");

    return _skeleton->nameToJointIndex(QString(target.c_str()));
}

AnimPose DeepMotionNode::getTargetJointAbsPose(const avatar::IMultiBodyHandle::LinkHandle& link) const {
    int jointIndex = getTargetJointIndex(link);
    if (jointIndex < 0) {
        qCCritical(animation) << "Can't find target joint index for link: " << _characterHandle->GetLinkName(link);
        return AnimPose();
    }

    return _skeleton->getAbsolutePose(jointIndex, _relativePoses);
}

void DeepMotionNode::debugDrawRelativePoses(const AnimContext& context) const {
    AnimPoseVec poses = _relativePoses;

    // convert relative poses to absolute
    _skeleton->convertRelativePosesToAbsolute(poses);

    mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();

    const vec4 RED(1.0f, 0.0f, 0.0f, 0.5f);
    const vec4 GREEN(0.0f, 1.0f, 0.0f, 0.5f);
    const vec4 BLUE(0.0f, 0.0f, 1.0f, 0.5f);
    const vec4 GRAY(0.2f, 0.2f, 0.2f, 0.5f);
    const float AXIS_LENGTH = 3.0f; // cm

    for (int i = 0; i < (int)poses.size(); ++i) {
        // transform local axes into world space.
        auto pose = poses[i];
        glm::vec3 xAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_X);
        glm::vec3 yAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_Y);
        glm::vec3 zAxis = transformVectorFast(geomToWorldMatrix, pose.rot() * Vectors::UNIT_Z);
        glm::vec3 pos = transformPoint(geomToWorldMatrix, pose.trans());
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * xAxis, RED);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * yAxis, GREEN);
        DebugDraw::getInstance().drawRay(pos, pos + AXIS_LENGTH * zAxis, BLUE);

        // draw line to parent
        int parentIndex = _skeleton->getParentIndex(i);
        if (parentIndex != -1) {
            glm::vec3 parentPos = transformPoint(geomToWorldMatrix, poses[parentIndex].trans());
            DebugDraw::getInstance().drawRay(pos, parentPos, GRAY);
        }

        for (const auto& link : _characterLinks) 
        {
            int targetIndex = getTargetJointIndex(link);
            if (targetIndex == i) {
                auto linkName = _characterHandle->GetLinkName(link);
                auto linkToFbxJoint = AnimPose(Quaternions::IDENTITY, toVec3(_linkToFbxJointTransform.at(linkName)));
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

void DeepMotionNode::debugDrawLink(const AnimContext& context, const avatar::IMultiBodyHandle::LinkHandle& link) const {

    const auto& targetJointPose = getTargetJointAbsPose(link);
    const auto& linkTransformRigSpace = getLinkTransformInRigSpace(link);
    const auto& fbxJointPose = getFbxJointPose(link);

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

    static std::map<std::string, glm::vec4> linkColliderColor = {
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
    
    auto linkCollider = _characterHandle->GetLinkCollider(link);
    if (linkCollider) {
        glm::vec4 color = linkColliderColor.at(_characterHandle->GetLinkName(link));
        drawCollider(context, linkTransformRigSpace, *linkCollider, color, geomToWorldMatrix);
    }
}

void DeepMotionNode::drawCollider(const AnimContext& context, AnimPose transform, avatar::IColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals) const {
    switch(collider.GetColliderType()) {
    case avatar::IColliderHandle::ColliderType::BOX: {
        drawBoxCollider(context, transform, static_cast<avatar::IBoxColliderHandle&>(collider), color, geomToWorld, drawDiagonals);
        break;
    }
    case avatar::IColliderHandle::ColliderType::CAPSULE: {
        qCCritical(animation) << "Trying to draw capsule collider";
        break;
    }
    case avatar::IColliderHandle::ColliderType::CYLINDER: {
        qCCritical(animation) << "Trying to draw cylinder collider";
        break;
    }
    case avatar::IColliderHandle::ColliderType::SPHERE: {
        qCCritical(animation) << "Trying to draw sphere collider";
        break;
    }
    case avatar::IColliderHandle::ColliderType::COMPOUND: {
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

    for (const auto& link : _characterLinks) {
        debugDrawLink(context, link);
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
