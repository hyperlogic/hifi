#ifndef hifi_DeepMotionNode_h
#define hifi_DeepMotionNode_h

#include "deepMotion_interface.h"

#include "AnimNode.h"
#include "qnetworkreply.h"

class RotationConstraint;
class Resource;

namespace avatar
{
    struct SimpleGenericHandle;
} // avatar

class DeepMotionNode : public AnimNode, public QObject {
public:
    DeepMotionNode() = delete;
    explicit DeepMotionNode(const QString& id);
    DeepMotionNode(const DeepMotionNode&) = delete;
    DeepMotionNode(const DeepMotionNode&&) = delete;
    DeepMotionNode operator=(const DeepMotionNode&) = delete;

    virtual ~DeepMotionNode() {
        FinalizeIntegration();
    }

    enum class SolutionSource {
        RelaxToUnderPoses = 0,
        RelaxToLimitCenterPoses,
        PreviousSolution,
        UnderPoses,
        LimitCenterPoses,
        NumSolutionSources,
    };

    void characterLoaded(const QByteArray data);
    void characterFailedToLoad(QNetworkReply::NetworkError error);

    void loadPoses(const AnimPoseVec& poses);

    void setTargetVars(const QString& jointName, const QString& positionVar, const QString& rotationVar,
                       const QString& typeVar, const QString& weightVar, float weight, const std::vector<float>& flexCoefficients,
                       const QString& poleVectorEnabledVar, const QString& poleReferenceVectorVar, const QString& poleVectorVar);

    virtual const AnimPoseVec& evaluate(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut) override;
    virtual const AnimPoseVec& overlay(const AnimVariantMap& animVars, const AnimContext& context, float dt, Triggers& triggersOut, const AnimPoseVec& underPoses) override;

protected:
    // for AnimDebugDraw rendering
    virtual const AnimPoseVec& getPosesInternal() const override { return _relativePoses; }


    std::map<int, RotationConstraint*> _constraints;
    AnimPoseVec _relativePoses; // current relative poses

    const SolutionSource _solutionSource{ SolutionSource::PreviousSolution };
    QString _solutionSourceVar;

    avatar::SimpleGenericHandle _sceneHandle;
    QSharedPointer<Resource> _characterResource;
};

#endif // hifi_DeepMotionNode_h
