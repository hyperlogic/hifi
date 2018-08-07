#ifndef hifi_DeepMotionNode_h
#define hifi_DeepMotionNode_h

#include "deepMotion_interface.h"

#include "AnimNode.h"

class RotationConstraint;

class DeepMotionNode : public AnimNode {
public:
    DeepMotionNode() = delete;
    explicit DeepMotionNode(const QString& id) : AnimNode(AnimNode::Type::InverseKinematics, id) {
        InitializeIntegration();
    }
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
};

#endif // hifi_DeepMotionNode_h
