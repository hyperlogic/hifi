#ifndef hifi_DeepMotionNode_h
#define hifi_DeepMotionNode_h

#include "AnimNode.h"
#include "qnetworkreply.h"

#include "dm_public/types.h"
#include "dm_public/i_array_interface.h"
#include "dm_public/interfaces/i_scene_object_handle.h"
#include "dm_public/interfaces/i_scene_handle.h"
#include "dm_public/interfaces/i_multi_body_handle.h"

class RotationConstraint;
class Resource;

namespace avatar
{
    class IEngineInterface;
    class IBoxColliderHandle;
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

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut) override;
    virtual const AnimPoseVec& overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, AnimVariantMap& triggersOut, const AnimPoseVec& underPoses) override;

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override { return _relativePoses; }

    static void applyDMToHFCharacterOffset(avatar::Transform& characterTransform);

    //std::map<int, RotationConstraint*> _constraints;
    AnimPoseVec _relativePoses; // current relative poses

    avatar::IEngineInterface& _engineInterface;
    std::shared_ptr<avatar::ISceneHandle> _sceneHandle = nullptr;
    const QString _characterPath = QString("deepMotion/schoolBoyScene.json");
    QSharedPointer<Resource> _characterResource;
    std::unique_ptr<avatar::IMultiBodyHandle> _characterHandle = nullptr;
    std::vector<avatar::IMultiBodyHandle::LinkHandle> _characterLinks;

    std::map<std::string, std::string> _boneTargetNameToJointName = {
        { "SK_Char_Boy_Bip001 R Thigh"      , "SK_Char_Boy_Bip001 L Thigh"      },
        { "SK_Char_Boy_Bip001 Spine1"       , "SK_Char_Boy_Bip001 Spine1"       },
        { "SK_Char_Boy_Bip001 L Thigh"      , "SK_Char_Boy_Bip001 R Thigh"      },
        { "SK_Char_Boy_Bip001 R Calf"       , "SK_Char_Boy_Bip001 L Calf"       },
        { "SK_Char_Boy_Bip001 Spine2"       , "SK_Char_Boy_Bip001 Spine2"       },
        { "SK_Char_Boy_Bip001 L Calf"       , "SK_Char_Boy_Bip001 R Calf"       },
        { "SK_Char_Boy_Bip001 R Foot"       , "SK_Char_Boy_Bip001 L Foot"       },
        { "SK_Char_Boy_Bip001 R Clavicle"   , "SK_Char_Boy_Bip001 L Clavicle"   },
        { "SK_Char_Boy_Bip001 L Foot"       , "SK_Char_Boy_Bip001 R Foot"       },
        { "SK_Char_Boy_Bip001 L Clavicle"   , "SK_Char_Boy_Bip001 R Clavicle"   },
        { "SK_Char_Boy_Bip001 Head"         , "SK_Char_Boy_Bip001 Head"         },
        { "SK_Char_Boy_Bip001 R UpperArm"   , "SK_Char_Boy_Bip001 L UpperArm"   },
        { "Bip001 R Toe0Nub"                , "Bip001 L Toe0Nub"                },
        { "SK_Char_Boy_Bip001 L UpperArm"   , "SK_Char_Boy_Bip001 R UpperArm"   },
        { "Bip001 L Toe0Nub"                , "Bip001 R Toe0Nub"                },
        { "SK_Char_Boy_Bip001 R Forearm"    , "SK_Char_Boy_Bip001 L Forearm"    },
        { "SK_Char_Boy_Bip001 L Forearm"    , "SK_Char_Boy_Bip001 R Forearm"    },
        { "SK_Char_Boy_Bip001 R Hand"       , "SK_Char_Boy_Bip001 L Hand"       },
        { "SK_Char_Boy_Bip001 L Hand"       , "SK_Char_Boy_Bip001 R Hand"       }
    };
};

#endif // hifi_DeepMotionNode_h
