#ifndef hifi_DeepMotionNode_h
#define hifi_DeepMotionNode_h

#include "AnimNode.h"
#include "qnetworkreply.h"

#include "dm_public/types.h"
#include "dm_public/i_array_interface.h"
#include "dm_public/interfaces/i_scene_object_handle.h"
#include "dm_public/interfaces/i_scene_handle.h"
#include "dm_public/interfaces/i_multi_body_handle.h"
#include "dm_public/interfaces/i_controller_handle.h"

#include <map>

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

    void setTargetVars(const QString& jointName, const QString& controllerBoneTarget, const QString& targetLinkName, 
        const QString& positionVar, const QString& rotationVar, 
        bool trackPosition, bool trackRotation, const QString& typeVar);

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) override;
    virtual const AnimPoseVec& overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut, const AnimPoseVec& underPoses) override;
    
    void overridePhysCharacterPositionAndOrientation(glm::vec3& position, glm::quat& rotation);

protected:
    struct IKTargetVar {
        IKTargetVar(const QString& jointNameIn, const QString& controllerBoneTargetIn,
            const QString& targetLinkName,
            const QString& positionVar, const QString& rotationVar,
            bool trackPosition, bool trackRotation, const QString& typeVar);
        IKTargetVar(const IKTargetVar&) = default;
        IKTargetVar& operator=(const IKTargetVar&);

        QString jointName;
        QString controllerBoneTargetName;
        QString targetLinkName;
        QString positionVar;
        QString rotationVar;
        bool trackPosition = false;
        bool trackRotation = false;
        QString typeVar;
        int jointIndex = -1; // cached joint index
    };

    class IKTarget {
    public:
        enum class Type {
            DMTracker = 0,
            Unknown
        };

        explicit IKTarget(const IKTargetVar&, avatar::IMultiBodyHandle*, std::vector<avatar::IMultiBodyHandle::LinkHandle>&);

        void setPosition(vec3 position) { pose.trans() = position; }
        void setRotation(quat rotation) { pose.rot() = rotation; }

        avatar::IHumanoidControllerHandle::BoneTarget getControllerBoneTarget() const { return controllerBoneTarget; }
        avatar::IMultiBodyHandle::LinkHandle& getTargetLink() const { return targetLink; }
        bool isTrackingPosition() const { return trackPosition; }
        bool isTrackingRotation() const { return trackRotation; }
        int getJointIndex() const { return jointIndex; }
        AnimPose getPose() const { return pose; }

    private:
        avatar::IHumanoidControllerHandle::BoneTarget controllerBoneTarget;
        avatar::IMultiBodyHandle::LinkHandle& targetLink;
        bool trackPosition = false;
        bool trackRotation = false;
        int jointIndex = -1;
        AnimPose pose;
    };

    void computeTargets(const AnimVariantMap& animVars, std::vector<IKTarget>& targets);
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

    std::vector<IKTargetVar> _targetVarVec;

    //std::map<int, RotationConstraint*> _constraints;
    AnimPoseVec _relativePoses; // current relative poses

    const QString _characterPath = QString("deepMotion/0905_schoolBoyScene.json");
    QSharedPointer<Resource> _characterResource;

    glm::mat4 _dmToAvtMatrix;
    avatar::IEngineInterface& _engineInterface;
    std::shared_ptr<avatar::ISceneHandle> _sceneHandle = nullptr;
    avatar::IMultiBodyHandle* _characterHandle = nullptr;
    std::vector<avatar::IMultiBodyHandle::LinkHandle> _characterLinks;
    avatar::IHumanoidControllerHandle* _characterController = nullptr;

    avatar::IRigidBodyHandle* _groundHandle = nullptr;

    mat4 _rootToCharacterMatrix = Matrices::IDENTITY;
    vec3 _rootToCharacterShift = Vectors::MAX;
};

#endif // hifi_DeepMotionNode_h
