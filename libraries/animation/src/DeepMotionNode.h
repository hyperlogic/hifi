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

        explicit IKTarget(const IKTargetVar&, avatar::IMultiBodyHandle::LinkHandle&);

        void setPosition(vec3 position) { pose.trans() = position; }
        void setRotation(quat rotation) { pose.rot() = rotation; }
        //void setTransform(avatar::Transform trans) { transform = trans; }

        avatar::IHumanoidControllerHandle::BoneTarget getControllerBoneTarget() const { return controllerBoneTarget; }
        avatar::IMultiBodyHandle::LinkHandle& getTargetLink() const { return targetLink; }
        bool isTrackingPosition() const { return trackPosition; }
        bool isTrackingRotation() const { return trackRotation; }
        int getJointIndex() const { return jointIndex; }
        //AnimPose getPose() const { return pose; }
        avatar::Transform calculateTransform();
        avatar::Transform getTransform() const { return transform; }

        AnimPose pose;
        avatar::Transform transform;
    private:
        avatar::IHumanoidControllerHandle::BoneTarget controllerBoneTarget;
        avatar::IMultiBodyHandle::LinkHandle& targetLink;
        bool trackPosition = false;
        bool trackRotation = false;
        int jointIndex = -1;
    };

    void computeTargets(const AnimVariantMap& animVars, std::vector<IKTarget>& targets, bool drawDebug = false);
    void updateRelativePosesFromCharacterLinks();
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override { return _relativePoses; }

    AnimPose getLinkTransformInRigSpace(const avatar::IMultiBodyHandle::LinkHandle& link) const;
    AnimPose getFbxJointPose(const avatar::IMultiBodyHandle::LinkHandle& link) const;
    int getTargetJointIndex(const avatar::IMultiBodyHandle::LinkHandle& link) const;
    AnimPose getTargetJointAbsPose(const avatar::IMultiBodyHandle::LinkHandle& link) const;

    void debugDrawRelativePoses(const AnimContext& context) const;
    void debugDrawCharacter(const AnimContext& context) const;
    void debugDrawGround(const AnimContext& context) const;

    void debugDrawLink(const AnimContext& context, const avatar::IMultiBodyHandle::LinkHandle& link) const;
    void drawCollider(const AnimContext& context, AnimPose transform, avatar::IColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals = false) const;
    void drawCompoundCollider(const AnimContext& context, AnimPose transform, avatar::ICompoundColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals = false) const;
    void drawBoxCollider(const AnimContext& context, AnimPose transform, avatar::IBoxColliderHandle& collider, glm::vec4 color, const mat4& geomToWorld, bool drawDiagonals = false) const;

    void drawDebug(const AnimContext& context);

    std::vector<IKTargetVar> _targetVarVec;

    //std::map<int, RotationConstraint*> _constraints;
    AnimPoseVec _relativePoses; // current relative poses
    std::map<std::string, avatar::Vector3> _linkToFbxJointTransform = {
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

    const QString _characterPath = QString("deepMotion/0911_schoolBoyScene.json");
    QSharedPointer<Resource> _characterResource;

    glm::mat4 _dmToAvtMatrix;
    avatar::IEngineInterface& _engineInterface;
    std::shared_ptr<avatar::ISceneHandle> _sceneHandle = nullptr;
    avatar::IMultiBodyHandle* _characterHandle = nullptr;
    std::vector<avatar::IMultiBodyHandle::LinkHandle> _characterLinks;
    avatar::IHumanoidControllerHandle* _characterController = nullptr;

    avatar::IRigidBodyHandle* _groundHandle = nullptr;

    mat4 _geometryToRigMatrix = Matrices::IDENTITY;
    mat4 _rigToWorldMatrix = Matrices::IDENTITY;
};

#endif // hifi_DeepMotionNode_h
