#ifndef hifi_DeepMotionNode_h
#define hifi_DeepMotionNode_h

#include "AnimNode.h"
#include "qnetworkreply.h"

#include "dm_public/types.h"
#include "dm_public/i_array_interface.h"
#include "dm_public/interfaces/i_scene_object_handle.h"
#include "dm_public/interfaces/i_scene_handle.h"
#include "dm_public/interfaces/i_multi_body_handle.h"

#include <unordered_map>

class RotationConstraint;
class Resource;
class MyAvatar;

namespace avatar
{
    class IEngineInterface;
    class IColliderHandle;
    class IBoxColliderHandle;
    class ICompoundColliderHandle;
    class IMultiBodyHandle;
    class IRigidBodyHandle;
    class IHumanoidControllerHandle;
} // avatar

class DeepMotionNode : public AnimNode, public QObject {
public:
    DeepMotionNode() = delete;
    explicit DeepMotionNode(const QString& id);
    DeepMotionNode(const DeepMotionNode&) = delete;
    DeepMotionNode(const DeepMotionNode&&) = delete;
    DeepMotionNode operator=(const DeepMotionNode&) = delete;

    virtual ~DeepMotionNode();

    void characterLoaded(const QByteArray& data);
    static void characterFailedToLoad(QNetworkReply::NetworkError error);

    void loadPoses(const AnimPoseVec& poses);

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) override;
    virtual const AnimPoseVec& overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut, const AnimPoseVec& underPoses) override;
    
    void overridePhysCharacterPositionAndOrientation(glm::vec3& position, glm::quat& rotation);

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override { return _relativePoses; }

    AnimPose getLinkTransformInRigSpace(const avatar::IMultiBodyHandle::LinkHandle& link) const;
    int getTargetJointIndex(const avatar::IMultiBodyHandle::LinkHandle& link) const;
    AnimPose getTargetJointAbsPose(const avatar::IMultiBodyHandle::LinkHandle& link) const;

    void debugDrawRelativePoses(const AnimContext& context) const;
    void debugDrawCharacter(const AnimContext& context) const;
    void debugDrawGround(const AnimContext& context) const;

    void debugDrawLink(const AnimContext& context, const avatar::IMultiBodyHandle::LinkHandle& link) const;
    void drawCollider(const AnimContext& context, AnimPose transform, avatar::IColliderHandle& collider, glm::vec4 color, bool drawDiagonals = false) const;
    void drawCompoundCollider(const AnimContext& context, AnimPose transform, avatar::ICompoundColliderHandle& collider, glm::vec4 color, bool drawDiagonals = false) const;
    void drawBoxCollider(const AnimContext& context, AnimPose transform, avatar::IBoxColliderHandle& collider, glm::vec4 color, bool drawDiagonals = false) const;

    void drawDebug(const AnimContext& context);

    //std::map<int, RotationConstraint*> _constraints;
    AnimPoseVec _relativePoses; // current relative poses

    const QString _characterPath = QString("deepMotion/0905_schoolBoyScene.json");
    QSharedPointer<Resource> _characterResource;

    glm::mat4 _dmToAvtMatrix;
    avatar::IEngineInterface& _engineInterface;
    std::shared_ptr<avatar::ISceneHandle> _sceneHandle = nullptr;
    avatar::IMultiBodyHandle* _characterHandle = nullptr;
    avatar::IHumanoidControllerHandle* _characterController = nullptr;
    std::vector<avatar::IMultiBodyHandle::LinkHandle> _characterLinks;

    avatar::IRigidBodyHandle* _groundHandle = nullptr;

    mat4 _rootToCharacterMatrix = Matrices::IDENTITY;
    vec3 _rootToCharacterShift = Vectors::MAX;
};

#endif // hifi_DeepMotionNode_h
