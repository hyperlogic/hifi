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
#include <unordered_map>

#define APPLY_X_Z_MOVEMENT_TO_CHARACTER
#define USE_FIX_FOR_TRACKER_ROT
//#define ENABLE_PRINTING
//#define DLL_WITH_DEBUG_VISU;

const float METERS_TO_CENTIMETERS = 100.0f;
const float AVATAR_SCALE = 1.4f;

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
    
    void overridePhysCharacterPositionAndOrientation(float floorDistance, glm::vec3& position, glm::quat& rotation);

protected:

    void computeTargets(const AnimContext& context, const AnimVariantMap& animVars);
    void updateRelativePosesFromCharacterLinks();
    void updateRelativePoseFromCharacterLink(const AnimPose& linkPose, int jointIndex);
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override { return _relativePoses; }

    AnimPose getLinkTransformInRigSpace(int linkIndex) const;
    AnimPose getFbxJointPose(int linkIndex) const;
    int getTargetJointIndex(int linkIndex) const;
    void getAdditionalTargetJointIndices(std::string targetBoneName, std::vector<int>& additionalTargetJointIndices) const;
    AnimPose getTargetJointAbsPose(int linkIndex) const;

    void debugDrawRelativePoses(const AnimContext& context) const;
    void debugDrawCharacter(const AnimContext& context) const;
    void debugDrawGround(const AnimContext& context) const;

    void debugDrawLink(const AnimContext& context, int linkIndex) const;
    void drawCollider(const AnimContext& context, AnimPose transform, avatar::IColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals = false) const;
    void drawCompoundCollider(const AnimContext& context, AnimPose transform, avatar::ICompoundColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals = false) const;
    void drawBoxCollider(const AnimContext& context, AnimPose transform, avatar::IBoxColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals = false) const;

    void drawDebug(const AnimContext& context);

    struct LinkInfo {
        int targetJointIndex;
        std::vector<int> additionalTargetJointsIndices;
        avatar::IMultiBodyHandle::LinkHandle linkHandle;
        std::string linkName;
        avatar::Vector3 linkToFbxJointTransform;

        LinkInfo(avatar::IMultiBodyHandle::LinkHandle& link, std::string& name, avatar::Vector3& linkToFbx)
            : linkHandle(link), linkName(name), linkToFbxJointTransform(linkToFbx)
        {}
    };
    class IKTargetVar {
    public:
        friend void DeepMotionNode::computeTargets(const AnimContext& context, const AnimVariantMap& animVars);

        enum class IKTargetType {
            DMTracker = 0,
            Unknown
        };

        IKTargetVar(const QString& jointNameIn, const QString& controllerBoneTargetIn,
            const QString& targetLinkName,
            const QString& positionVar, const QString& rotationVar,
            bool trackPosition, bool trackRotation, const QString& typeVar);
        IKTargetVar(const IKTargetVar&) = default;
        IKTargetVar& operator=(const IKTargetVar&);

        avatar::IHumanoidControllerHandle::BoneTarget getControllerBoneTarget() const;
        QString getControllerBoneTargetName() const { return controllerBoneTargetName; }
        QString getJointName() const { return jointName; }
        void setPosition(vec3 position) { pose.trans() = position; }
        void setRotation(quat rotation) { pose.rot() = rotation; }
        avatar::Transform getTransform() const { return transform; }


    private:
        AnimPose pose;
        avatar::Transform transform;
        bool trackPosition = false;
        bool trackRotation = false;

        QString jointName;
        QString controllerBoneTargetName;
        QString targetLinkName;
        QString positionVar;
        QString rotationVar;
        QString typeVar;
        int jointIndex = -1; // cached joint index
#ifdef DLL_WITH_DEBUG_VISU
        avatar::IRigidBodyHandle* debugBody = nullptr;
#endif
    };

    std::vector<IKTargetVar> _targetVarVec;

    //std::map<int, RotationConstraint*> _constraints;
    AnimPoseVec _relativePoses; // current relative poses

    const QString _characterPath = QString("deepMotion/0911_schoolBoyScene.json");
    QSharedPointer<Resource> _characterResource;

    glm::mat4 _dmToAvtMatrix;
    avatar::IEngineInterface& _engineInterface;
    std::shared_ptr<avatar::ISceneHandle> _sceneHandle = nullptr;
    avatar::IMultiBodyHandle* _characterHandle = nullptr;
    std::vector<LinkInfo> _characterLinks;
    std::unordered_map<std::string, int> _linkNameToIndex;
    avatar::IHumanoidControllerHandle* _characterController = nullptr;

    avatar::IRigidBodyHandle* _groundHandle = nullptr;
};

#endif // hifi_DeepMotionNode_h
