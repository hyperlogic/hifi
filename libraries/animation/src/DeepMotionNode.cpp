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
#include "dm_public/interfaces/i_controller_handle.h"

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
const float METERS_TO_CENTIMETERS = 100.0f;

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
        glm::vec3 trans = toVec3(avatarTransform.m_Position) * METERS_TO_CENTIMETERS;
        glm::quat ori = toQuat(avatarTransform.m_Orientation);
        glm::vec3 scale = toVec3(avatarTransform.m_Scale);

        AnimPose newPose{ scale, ori, trans };
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

    if (!_skeleton
        || !_characterHandle
        || _relativePoses.empty())
        return underPoses;

    _engineInterface.TickGeneralPurposeRuntime(dt);

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

    drawDebug(context);
    //qCCritical(animation) << "---";

    return _relativePoses;
}

void DeepMotionNode::overridePhysCharacterPositionAndOrientation(glm::vec3& position, glm::quat& rotation) {
    //qCCritical(animation) << "KG_KG Controller pos: " << to_string(position).c_str() << " rot: " << to_string(rotation).c_str();

    if (!_characterHandle || _relativePoses.empty())
        return;

    if (_rootToCharacterShift == Vectors::MAX) {
        const auto& rootPosition = toVec3(_characterHandle->GetTransform().m_Position);
        _rootToCharacterShift = position - rootPosition;
        //qCCritical(animation) << "GK_GK root position:      " << to_string(rootPosition).c_str();
        //qCCritical(animation) << "GK_GK character position: " << to_string(position).c_str();
        //qCCritical(animation) << "GK_GK calculated shift:   " << to_string(_rootToCharacterShift).c_str();
        return;
    }

    const auto& rootPosition = toVec3(_characterHandle->GetTransform().m_Position);
    //qCCritical(animation) << "GK_GK root position:      " << to_string(rootPosition).c_str();
    //qCCritical(animation) << "GK_GK character position: " << to_string(position).c_str();
    //qCCritical(animation) << "GK_GK shift:              " << to_string(_rootToCharacterShift).c_str();
    position = rootPosition + _rootToCharacterShift;
    //qCCritical(animation) << "GK_GK character position: " << to_string(position).c_str();


    //if (_dmRootToHfCharacter == Matrices::IDENTITY) {
    //    AnimPose dmCharacter = toAnimPose(_characterHandle->GetTransform()) * AnimPose(_dmToAvtMatrix);
    //    AnimPose hfCharacter = AnimPose(rotation, position);
    //    _dmRootToHfCharacter = createMatFromQuatAndPos(hfCharacter.rot() * inverse(dmCharacter.rot()), hfCharacter.trans() - dmCharacter.trans());
    //    //position.y = 20.0f;
    //    return;
    //}

    //AnimPose characterTransform = toAnimPose(_characterHandle->GetTransform());
    //applyDMToHFCharacterOffset(characterTransform);
    //AnimPose rootPose = characterTransform * AnimPose(_dmRootToHfCharacter * _dmToAvtMatrix);
    //position = rootPose.trans();
    //rotation = rootPose.rot();
    //_relativePoses[0] = rootPose;
}

AnimPose DeepMotionNode::getLinkTransformInRigSpace(const avatar::IMultiBodyHandle::LinkHandle& link) const
{
    auto linkDmWorldTransform = toAnimPose(_characterHandle->GetLinkTransform(link));
    // get link transform relative to dm character
    AnimPose linkDmCharTransform = toAnimPose(_characterHandle->GetTransform()).inverse() * linkDmWorldTransform;
    // get link transform relative to root target bone
    int rootTargetIndex = getTargetJointIndex(_characterLinks[0]);
    AnimPose rootTargetTransform = _skeleton->getAbsolutePose(rootTargetIndex, _relativePoses);
    AnimPose linkRootTransform = AnimPose(Quaternions::IDENTITY, rootTargetTransform.trans()) * linkDmCharTransform;

    return linkRootTransform;
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
    const float AXIS_LENGTH = 5.0f; // cm

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
    }
}

void DeepMotionNode::drawBoxCollider(const AnimContext& context, AnimPose transform, avatar::IBoxColliderHandle& collider, glm::vec4 color, bool drawDiagonals) const {
    mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();

    vec3 halfSizes = toVec3(collider.GetHalfSize()) * transform.scale() * METERS_TO_CENTIMETERS;
    vec3 pos = transformPoint(geomToWorldMatrix, transform.trans());
    quat rot = transform.rot();

    vec3 rightUpFront   = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_Y     + Vectors::UNIT_Z    );
    vec3 rightUpBack    = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_Y     + Vectors::UNIT_NEG_Z);
    vec3 leftUpFront    = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_Y     + Vectors::UNIT_Z    );
    vec3 leftUpBack     = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_Y     + Vectors::UNIT_NEG_Z);

    vec3 rightDownFront = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_NEG_Y + Vectors::UNIT_Z    );
    vec3 rightDownBack  = glm::normalize(Vectors::UNIT_X     + Vectors::UNIT_NEG_Y + Vectors::UNIT_NEG_Z);
    vec3 leftDownFront  = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_NEG_Y + Vectors::UNIT_Z    );
    vec3 leftDownBack   = glm::normalize(Vectors::UNIT_NEG_X + Vectors::UNIT_NEG_Y + Vectors::UNIT_NEG_Z);

    vec3 colliderRightUpFront   = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * rightUpFront  ));
    vec3 colliderRightUpBack    = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * rightUpBack   ));
    vec3 colliderLeftUpFront    = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * leftUpFront   ));
    vec3 colliderLeftUpBack     = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * leftUpBack    ));
    vec3 colliderRightDownFront = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * rightDownFront));
    vec3 colliderRightDownBack  = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * rightDownBack ));
    vec3 colliderLeftDownFront  = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * leftDownFront ));
    vec3 colliderLeftDownBack   = transformVectorFast(geomToWorldMatrix, rot * (halfSizes * leftDownBack  ));

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

void DeepMotionNode::drawCompoundCollider(const AnimContext& context, AnimPose transform, avatar::ICompoundColliderHandle& collider, glm::vec4 color, bool drawDiagonals) const {
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

void DeepMotionNode::debugDrawLink(const AnimContext& context, const avatar::IMultiBodyHandle::LinkHandle& link) const {
    //const auto& linkTransform = getLinkTransformInRigSpace(link);

    const auto& targetJointPose = getTargetJointAbsPose(link);
    const auto& linkTransformRigSpace = getLinkTransformInRigSpace(link);
    
    const auto& targetJointTransform = linkTransformRigSpace * (linkTransformRigSpace.inverse() * targetJointPose);

    const mat4 geomToWorldMatrix = context.getRigToWorldMatrix() * context.getGeometryToRigMatrix();

    const vec4 RED(1.0f, 0.0f, 0.0f, 0.5f);
    const vec4 GREEN(0.0f, 1.0f, 0.0f, 0.5f);
    const vec4 BLUE(0.0f, 0.0f, 1.0f, 0.5f);
    const vec4 ORANGE(0.9f, 0.4f, 0.0f, 1.0f);

    const float AXIS_LENGTH = 5.0f; // cm

    vec3 linkPos = transformPoint(geomToWorldMatrix, targetJointTransform.trans());
    quat linkRot = targetJointTransform.rot();

    vec3 xAxis = transformVectorFast(geomToWorldMatrix, linkRot * Vectors::UNIT_X);
    vec3 yAxis = transformVectorFast(geomToWorldMatrix, linkRot * Vectors::UNIT_Y);
    vec3 zAxis = transformVectorFast(geomToWorldMatrix, linkRot * Vectors::UNIT_Z);
    DebugDraw::getInstance().drawRay(linkPos, linkPos + AXIS_LENGTH * xAxis, RED);
    DebugDraw::getInstance().drawRay(linkPos, linkPos + AXIS_LENGTH * yAxis, GREEN);
    DebugDraw::getInstance().drawRay(linkPos, linkPos + AXIS_LENGTH * zAxis, BLUE);


    glm::vec3 targetJointpos = transformPoint(geomToWorldMatrix, targetJointPose.trans());
    DebugDraw::getInstance().drawRay(linkPos, targetJointpos, ORANGE);

    //static std::map<std::string, glm::vec4> linkColliderColor = {
    //    { "root"             , glm::vec4(0.9f, 0.0f, 0.7f, 1.0f) },
    //    { "pelvis_lowerback" , glm::vec4(0.9f, 0.0f, 0.0f, 1.0f) },
    //    { "lowerback_torso"  , glm::vec4(0.9f, 0.4f, 0.0f, 1.0f) },
    //    { "torso_head"       , glm::vec4(0.9f, 0.8f, 0.0f, 1.0f) },
    //    { "lHip"             , glm::vec4(0.4f, 0.0f, 0.9f, 1.0f) },
    //    { "lKnee"            , glm::vec4(0.2f, 0.0f, 0.9f, 1.0f) },
    //    { "lAnkle"           , glm::vec4(0.0f, 0.0f, 0.9f, 1.0f) },
    //    { "lToeJoint"        , glm::vec4(0.0f, 0.2f, 0.9f, 1.0f) },
    //    { "rHip"             , glm::vec4(0.4f, 0.9f, 0.0f, 1.0f) },
    //    { "rKnee"            , glm::vec4(0.2f, 0.9f, 0.0f, 1.0f) },
    //    { "rAnkle"           , glm::vec4(0.0f, 0.9f, 0.0f, 1.0f) },
    //    { "rToeJoint"        , glm::vec4(0.0f, 0.9f, 0.2f, 1.0f) },
    //    { "lClav"            , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "lShoulder"        , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "lElbow"           , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "lWrist"           , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "rClav"            , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "rShoulder"        , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "rElbow"           , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) },
    //    { "rWrist"           , glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) }
    //    };
    //
    //auto linkCollider = _characterHandle->GetLinkCollider(link);
    //if (linkCollider) {
    //    //qCCritical(animation) << "Link: " << _characterHandle->GetLinkName(link) << " | target bone: " << _characterHandle->GetTargetBoneName(link) << " | bone: " << _skeleton->getJointName(jointIndex);
    //    glm::vec4 color = linkColliderColor.at(_characterHandle->GetLinkName(link));
    //    drawCollider(context, linkTransform, *linkCollider, color);
    //}
}

void DeepMotionNode::drawCollider(const AnimContext& context, AnimPose transform, avatar::IColliderHandle& collider, glm::vec4 color, bool drawDiagonals) const {
    switch(collider.GetColliderType()) {
    case avatar::IColliderHandle::ColliderType::BOX: {
        drawBoxCollider(context, transform, static_cast<avatar::IBoxColliderHandle&>(collider), color, drawDiagonals);
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
    //_skeleton->dump(_relativePoses);

    for (const auto& link : _characterLinks) {
        debugDrawLink(context, link);
    }
}

void DeepMotionNode::debugDrawGround(const AnimContext & context) const {
    auto groundCollider = _groundHandle->GetCollider();
    AnimPose groundTransform = toAnimPose(_groundHandle->GetTransform());
    //qCCritical(animation) << "Ground";
    const glm::vec4 brown(0.4f, 0.2f, 0.0f, 1.0f);
    //qCCritical(animation) << fixed("Ground").c_str();
    drawCollider(context, groundTransform, *groundCollider, brown, true);
}

void DeepMotionNode::drawDebug(const AnimContext& context) {
    if (context.getEnableDebugDrawIKChains()) {
        debugDrawRelativePoses(context);
    }
    if (context.getEnableDebugDrawIKConstraints()) {
        debugDrawCharacter(context);
        //debugDrawGround(context);
    }
}
