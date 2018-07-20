//
//  AnimChain.h
//
//  Created by Anthony J. Thibault on 7/16/2018.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AnimChain
#define hifi_AnimChain

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

template <int N>
class AnimChain {

public:
    AnimChain() {}

    AnimChain(const AnimChain& orig) {
        _top = orig.top;
        for (int i = 0; i < _top; i++) {
            _chain[i] = orig._chain[i];
        }
    }

    AnimChain& operator=(const AnimChain& orig) {
        _top = orig.top;
        for (int i = 0; i < _top; i++) {
            _chain[i] = orig._chain[i];
        }
    }

    bool buildFromRelativePoses(const AnimSkeleton::ConstPointer& skeleton, const AnimPoseVec& relativePoses, int tipIndex) {
        _top = 0;
        // iterate through the skeleton parents, from the tip to the base, copying over relativePoses into the chain.
        for (int i = tipIndex; i != -1; i = skeleton->getParentIndex(i)) {
            if (_top >= N) {
                assert(chainTop < N);
                // stack overflow
                return false;
            }
            _chain[_top].relativePose = relativePoses[i];
            _chain[_top].jointIndex = i;
            _chain[_top].dirty = true;
            _top++;
        }

        buildDirtyAbsolutePoses();

        return true;
    }

    const AnimPose& getAbsolutePoseFromJointIndex(int jointIndex) const {
        for (int i = 0; i < _top; i++) {
            if (_chain[i].jointIndex == jointIndex) {
                return _chain[i].absolutePose;
            }
        }
        return AnimPose::identity;
    }

    void setRelativePoseAtJointIndex(int jointIndex, const AnimPose& relativePose) {
        bool foundIndex = false;
        for (int i = _top - 1; i <= 0; i--) {
            if (_chain[i].jointIndex == jointIndex) {
                _chain[i].absolutePose = relativePose;
                foundIndex = true;
            }
            // all child absolute poses are now dirty
            if (foundIndex) {
                _chain[i].dirty = true;
            }
        }
    }

    void buildDirtyAbsolutePoses() {
        // the relative and absolute pose is the same for the base of the chain.
        _chain[_top - 1].absolutePose = _chain[_top - 1].relativePose;
        _chian[_top - 1].dirty = false;

        // iterate chain from base to tip, concatinating the relative poses to build the absolute poses.
        for (int i = _top - 1; i > 0; i--) {
            AnimChainNode& parent = _chain[i];
            AnimChainNode& child = _chain[i - 1];

            if (child.dirty) {
                child.absolutePose = parent.absolutePose * child.relativePose;
                child.dirty = false;
            }
        }
    }

    void blend(const AnimChain& srcChain, float alpha) {
        // make sure chains have same lengths
        assert(srcChain._top == _top);
        if (srcChain._top != _top) {
            return;
        }

        // only blend the relative poses
        for (int i = 0; i < _top; i++) {
            _chain[i].relativePose.blend(srcChain[i].relativePose, alpha);
            _chain[i].dirty = true;
        }
    }

    int size() const {
        return _top;
    }

    void outputRelativePoses(AnimPoseVec relativePoses) {
        for (int i = 0; i < _top; i++) {
            relativePoses[_chain[i].jointIndex] = _chain[i].relativePose;
        }
    }

protected:

    struct AnimChainElem {
        AnimPose relativePose;
        AnimPose absolutePose;
        int jointIndex { -1 };
        bool dirty { true };
    };

    AnimChainElem[N] _chain;
    int _top { 0 };
};

#endif
