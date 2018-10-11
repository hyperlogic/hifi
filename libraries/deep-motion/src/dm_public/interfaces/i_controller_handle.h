#pragma once
#include "dm_public/interfaces/i_scene_object_handle.h"
#include <stdint.h>
#include "dm_public/types.h"
namespace avatar
{
    class IControllerHandle : public ISceneObjectHandle
    {
    public:
        virtual ObjectType GetType() const override { return ObjectType::Controller; }

        enum class ControllerType
        {
            Biped,
            Quadruped
        };
        virtual ControllerType GetControllerType() const = 0;
    };


    class IHumanoidControllerHandle : public IControllerHandle
    {
    public:
        virtual ControllerType GetControllerType() const override { return ControllerType::Biped; }

        enum class BoneTarget : uint16_t
        {
            Head,
            Left_Hand,
            Right_Hand,
            Left_Foot,
            Right_Foot,
            Root,
            Left_Elbow,
            Right_Elbow,
            Left_Shoulder,
            Right_Shoulder,
            Left_Knee,
            Right_Knee,
            Left_Hip,
            Right_Hip
        };

        virtual bool GetLimbAccelerationTrackingEnabled(BoneTarget) const = 0;
        virtual void SetLimbAccelerationTrackingEnabled(BoneTarget, bool) = 0;
        virtual bool GetLimbPositionTrackingEnabled(BoneTarget) const = 0;
        virtual void SetLimbPositionTrackingEnabled(BoneTarget, bool) = 0;
        virtual bool GetLimbOrientationTrackingEnabled(BoneTarget) const = 0;
        virtual void SetLimbOrientationTrackingEnabled(BoneTarget, bool) = 0;
        virtual void SetTrackingAcceleration(BoneTarget, const Vector3&) = 0;
        virtual Vector3 GetTrackingAcceleration(BoneTarget) const = 0;
        virtual void SetTrackingPosition(BoneTarget, const Vector3&) = 0;
        virtual Vector3 GetTrackingPosition(BoneTarget) const = 0;
        virtual void SetTrackingOrientation(BoneTarget, const Quaternion&) = 0;
        virtual Quaternion GetTrackingOrientation(BoneTarget) = 0;
        virtual void SetLimbTrackingKP(float) = 0;
        virtual float GetLimbTrackingKP() const = 0;
        virtual void SetLimbTrackingKD(float) = 0;
        virtual float GetLimbTrackingKD() const = 0;
        virtual void SetMaxLimbTrackingForce(float) = 0;
        virtual float GetMaxLimbTrackingForce() const = 0;
        virtual void SetAntiLegCrossing(bool) = 0;
        virtual bool GetAntiLegCrossing() = 0;

        virtual bool SetDesiredHeading(float) = 0;
        virtual float GetHeadingAngle() const = 0;

    };

    class IDogControllerHandle : public IControllerHandle
    {
    public:
        virtual ControllerType GetControllerType() const override { return ControllerType::Quadruped; }
    };
}