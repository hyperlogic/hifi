//
// handshake.js
//
/* global Xform */
/* eslint max-len: ["error", 1024, 4] */

(function() { // BEGIN LOCAL_SCOPE

    Script.include("/~/system/libraries/Xform.js");

    // helper
    var AJT_SOLO_DEBUG = false;
    var AJT_SOLO_DEBUG_TIME = 10.0;
    var ALLOW_LIMB_GRABBING = false;

    // globals transmitted to and from web app
    var GRAB_DISTANCE = 0.2;
    var REJECT_DISTANCE = 0.5;

    //
    // tablet app boiler plate
    //

    var TABLET_BUTTON_NAME = "HANDSHAKE";
    var HTML_URL = "https://s3.amazonaws.com/hifi-public/tony/html/handshake3.html?1";

    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    var tabletButton = tablet.addButton({
        text: TABLET_BUTTON_NAME,
        icon: "https://s3.amazonaws.com/hifi-public/tony/icons/tpose-i.svg",
        activeIcon: "https://s3.amazonaws.com/hifi-public/tony/icons/tpose-a.svg"
    });

    tabletButton.clicked.connect(function () {
        if (shown) {
            tablet.gotoHomeScreen();
        } else {
            tablet.gotoWebScreen(HTML_URL);
        }
    });

    var shown = false;

    function onScreenChanged(type, url) {
        if (type === "Web" && url === HTML_URL) {
            tabletButton.editProperties({isActive: true});
            if (!shown) {
                // hook up to event bridge
                tablet.webEventReceived.connect(onWebEventReceived);
            }
            shown = true;
        } else {
            tabletButton.editProperties({isActive: false});
            if (shown) {
                // disconnect from event bridge
                tablet.webEventReceived.disconnect(onWebEventReceived);
            }
            shown = false;
        }
    }

    function onWebEventReceived(msg) {
        if (msg.name === "init-complete") {
            var values = [{name: "grab-distance", val: GRAB_DISTANCE * 100, checked: false},
                          {name: "reject-distance", val: REJECT_DISTANCE * 100, checked: false}];
            tablet.emitScriptEvent(JSON.stringify(values));
        } else if (msg.name === "grab-distance") {
            GRAB_DISTANCE = parseInt(msg.val, 10) / 100;
        } else if (msg.name === "reject-distance") {
            REJECT_DISTANCE = parseInt(msg.val, 10) / 100;
        }
    }

    tablet.screenChanged.connect(onScreenChanged);

    function shutdownTabletApp() {
        tablet.removeButton(tabletButton);
        if (shown) {
            tablet.webEventReceived.disconnect(onWebEventReceived);
            tablet.gotoHomeScreen();
        }
        tablet.screenChanged.disconnect(onScreenChanged);
    }

    //
    // end tablet app boiler plate
    //

    function clamp(val, min, max) {
        return Math.max(min, Math.min(max, val));
    }

    var QUAT_Y_180 = {x: 0, y: 1, z: 0, w: 0};
    var QUAT_IDENTITY = {x: 0, y: 0, z: 0, w: 1};
    var LEFT_HAND = 0;
    var RIGHT_HAND = 1;

    var myAvatarRightHandXform;
    var myAvatarLeftHandXform;

    var HAPTIC_PULSE_FIRST_STRENGTH = 1.0;
    var HAPTIC_PULSE_FIRST_DURATION = 16.0;
    var HAPTIC_PULSE_DISTANCE = 0.01;  // 1 cm

    var HAPTIC_PULSE_MAX_STRENGTH = 0.5;

    // used by GrabLink
    var HAPTIC_PULSE_STRENGTH = 1.0;
    var HAPTIC_PULSE_DURATION = 1.0;

    // ctor
    function HapticBuddy(hand) {
        this.hand = hand;
        this.enabled = false;
        this.frequencyScale = 1;
        this.lastPulseDistance = 0;
    }

    HapticBuddy.prototype.start = function (myHand, otherHand) {
        Controller.triggerHapticPulse(HAPTIC_PULSE_FIRST_STRENGTH, HAPTIC_PULSE_FIRST_DURATION, this.hand);
        this.enabled = true;
        this.lastPulseDistance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
    };

    HapticBuddy.prototype.update = function (myHand, otherHand) {
        if (this.enabled) {
            var distance = Vec3.distance(myHand.controllerPos, otherHand.controllerPos);
            if (Math.abs(this.lastPulseDistance - distance) > (HAPTIC_PULSE_DISTANCE / this.frequencyScale)) {
                this.lastPulseDistance = distance;

                // AJT: disable dynamic strength
                /*
                // convert distance into a value from 0 to 1
                var factor = clamp((distance - HAPTIC_PULSE_MIN_DISTANCE) / (HAPTIC_PULSE_MAX_DISTANCE - HAPTIC_PULSE_MIN_DISTANCE), 0, 1);
                // convert factor into a strength factor
                var strength = factor * (HAPTIC_PULSE_MAX_STRENGTH - HAPTIC_PULSE_MIN_STRENGTH) + HAPTIC_PULSE_MIN_STRENGTH;
                */

                Controller.triggerHapticPulse(HAPTIC_PULSE_MAX_STRENGTH, HAPTIC_PULSE_DURATION, this.hand);
            }
        }
    };

    HapticBuddy.prototype.stop = function () {
        this.enabled = false;
    };

    var rightHapticBuddy = new HapticBuddy(RIGHT_HAND);
    var leftHapticBuddy = new HapticBuddy(LEFT_HAND);

    // ctor
    function SoundBuddy(url) {
        this.sound = SoundCache.getSound(url);
        this.injector = null;
    }

    SoundBuddy.prototype.play = function (options, doneCallback) {
        if (this.sound.downloaded) {
            if (this.injector) {
                this.injector.setOptions(options);
                this.injector.restart();
            } else {
                this.injector = Audio.playSound(this.sound, options);
                this.injector.finished.connect(function () {
                    if (doneCallback) {
                        doneCallback();
                    }
                });
            }
        }
    };

    var CLAP_SOUND = "https://s3.amazonaws.com/hifi-public/tony/audio/slap.wav";
    var clapSound = new SoundBuddy(CLAP_SOUND);

    // A GrabLink is single grab interaction link between two avatars, typically between MyAvatar and another avatar in the scene.
    // It has an internal state machine to keep track of when to enable/disable IK and play sound effects.
    //
    // key is {avatarId: uuid, jointName: string};
    // state element of {alone, leader, follower, peer, reject}
    function GrabLink(myKey, otherKey) {

        print("AJT: new GrabLink(" + this.myKey.jointName + ", " + JSON.stringify(this.otherKey) + ")");

        this.myKey = myKey;
        this.otherKey = otherKey;
        this.state = "alone";
        this.relXform = new Xform({x: 0, y: 0, z: 0, w: 1}, {x: 0, y: 0, z: 0});
        this.states = {
            alone: {
                enter: this.aloneEnter,
                process: this.aloneProcess,
                exit: this.aloneExit
            },
            follower: {
                enter: this.followerEnter,
                process: this.followerProcess,
                exit: this.followerExit
            },
            peer: {
                enter: this.peerEnter,
                process: this.peerProcess,
                exit: this.peerExit
            },
            leader: {
                enter: this.leaderEnter,
                process: this.leaderProcess,
                exit: this.leaderExit
            },
            reject: {
                enter: this.rejectEnter,
                process: this.rejectProcess,
                exit: this.rejectExit
            }
        };
        this.myJointInfo = scanner.getJointInfo(myKey);
        this.otherJointInfo = scanner.getJointInfo(otherKey);
        this.timeInState = 0;
    }

    // static variables
    GrabLink.grabLinkMapMap = {};
    GrabLink.rightControllerDispatcherEnabled = true;
    GrabLink.leftControllerDispatcherEnabled = true;

    // static method
    GrabLink.findOrCreateLink = function (myKey, otherKey) {
        var grabLinkMap = GrabLink.grabLinkMapMap[myKey.jointName];
        var link;
        if (!grabLinkMap) {
            link = new GrabLink(myKey, otherKey);
            var map = {};
            map[otherKey.jointName] = link;
            GrabLink.grabLinkMapMap[myKey.jointName] = map;
        } else {
            link = grabLinkMap[otherKey.jointName];
            if (!link) {
                link = new GrabLink(myKey, otherKey);
                grabLinkMap[otherKey.jointName] = link;
            }
        }
        return link;
    };

    // static method
    GrabLink.findLink = function (myKey, otherKey) {
        var grabLinkMap = GrabLink.grabLinkMapMap[myKey.jointName];
        if (!grabLinkMap) {
            return undefined;
        } else {
            return grabLinkMap[otherKey.jointName];
        }
    };

    // static method
    GrabLink.process = function (dt) {
        var myKeys = Object.keys(GrabLink.grabLinkMapMap);
        var i, numMyKeys = myKeys.length;
        for (i = 0; i < numMyKeys; i++) {
            var otherMap = GrabLink.grabLinkMapMap[myKeys[i]];
            var otherKeys = Object.keys(otherMap);
            var j, numOtherKeys = otherKeys.length;
            for (j = 0; j < numOtherKeys; j++) {
                GrabLink.grabLinkMapMap[myKeys[i]][otherKeys[j]].process(dt);
            }
        }
    };

    GrabLink.prototype.changeState = function (newState) {
        if (this.state !== newState) {

            print("AJT: GrabLink(" + this.myKey.jointName + ", " + JSON.stringify(this.otherKey) + "), changeState " + this.state + " -> " + newState);

            // exit the old state
            this.states[this.state].exit.apply(this);

            // enter the new state
            this.states[newState].enter.apply(this);

            this.state = newState;
            this.timeInState = 0;
        }
    };

    GrabLink.prototype.process = function (dt) {
        this.timeInState += dt;
        this.states[this.state].process.apply(this, [dt]);
    };

    GrabLink.prototype.hapticPulse = function () {
        if (this.myKey.jointName.slice(0, 4) === "Left") {
            Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, LEFT_HAND);
        } else if (this.myKey.jointName.slice(0, 5) === "Right") {
            Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, RIGHT_HAND);
        }
    };

    GrabLink.prototype.clearPinOnJoint = function (key) {
        print("AJT: clearPinOnJoint key = " + JSON.stringify(key));
        if (key.avatarId === this.myKey.avatarId) {

            print("AJT: clearPinOnJoint myAvatar!");

            // AJT: TODO for now we only support hands for MyAvatar
            if (key.jointName === "LeftHand") {
                print("AJT: clearPinOnJoint, myAvatar, leftHand");
                myAvatarLeftHandXform = undefined;
            } else if (key.jointName === "RightHand") {
                print("AJT: clearPinOnJoint, myAvatar, rightHand");
                myAvatarRightHandXform = undefined;
            }
        } else if (key.avatarId === this.otherKey.avatarId) {

            print("AJT: clearPinOnJoint other avatar!");

            var avatar = AvatarManager.getAvatar(this.otherKey.avatarId);
            if (avatar) {
                print("AJT: clearPinOnJoint, otherAvatar, jointIndex = " + this.otherJointInfo.jointIndex);
                avatar.clearPinOnJoint(this.otherJointInfo.jointIndex);
            } else {
                print("AJT: WARNING: clearPinOnJoint() no avatar found");
            }
        } else {
            print("AJT: WARNING: clearPinOnJoint() bad key");
        }
    };

    GrabLink.prototype.pinJoint = function (key, jointInfo, targetXform) {
        if (key.avatarId === this.myKey.avatarId) {

            // AJT: TODO for now we only support hands for MyAvatar
            // Modify the myAvatar*HandXform global which will control MyAvatar's IK
            if (key.jointName === "LeftHand") {
                myAvatarLeftHandXform = targetXform;
            } else if (key.jointName === "RightHand") {
                myAvatarRightHandXform = targetXform;
            }
        } else if (key.avatarId === this.otherKey.avatarId) {
            var avatar = AvatarManager.getAvatar(this.otherKey.avatarId);
            if (avatar) {
                avatar.pinJoint(jointInfo.jointIndex, targetXform.pos, targetXform.rot);
            }
        } else {
            print("AJT: WARNING: pinJoint unknown avatarId, key " + JSON.stringify(key) + ", myKey = " + JSON.stringify(this.myKey) + ", otherKey = " + JSON.stringify(this.otherKey));
        }
    };

    GrabLink.prototype.computeDeltaXform = function () {
        // compute delta xform
        if ((this.myJointInfo.jointName === "LeftHand" && this.otherJointInfo.jointName === "LeftHand") ||
            (this.myJointInfo.jointName === "RightHand" && this.otherJointInfo.jointName === "RightHand")) {
            this.relXform = calculateHandDeltaOffset(this.myJointInfo, this.otherJointInfo);
        } else {
            this.relXform = calculateDeltaOffset(this.myJointInfo, this.otherJointInfo);
        }
    };

    GrabLink.prototype.disableControllerDispatcher = function () {
        // disable controller dispatcher script for this hand.
        if (this.myKey.jointName === "LeftHand") {
            if (GrabLink.rightControllerDispatcherEnabled) {
                print("AJT: send disable left");
                Messages.sendMessage("Hifi-Hand-Disabler", "left");
            } else {
                print("AJT: send disable both");
                Messages.sendMessage("Hifi-Hand-Disabler", "both");
            }
            GrabLink.leftControllerDispatcherEnabled = false;
        } else if (this.myKey.jointName === "RightHand") {
            if (GrabLink.leftControllerDispatcherEnabled) {
                print("AJT: send disable right");
                Messages.sendMessage("Hifi-Hand-Disabler", "right");
            } else {
                print("AJT: send disable both");
                Messages.sendMessage("Hifi-Hand-Disabler", "both");
            }
            GrabLink.rightControllerDispatcherEnabled = false;
        }
    };

    GrabLink.prototype.enableControllerDispatcher = function () {
        // enable controller dispatcher script for this hand.
        if (this.myKey.jointName === "LeftHand") {
            if (GrabLink.rightControllerDispatcherEnabled) {
                print("AJT: send disable none");
                Messages.sendMessage("Hifi-Hand-Disabler", "none");
            } else {
                print("AJT: send disable right");
                Messages.sendMessage("Hifi-Hand-Disabler", "right");
            }
            GrabLink.leftControllerDispatcherEnabled = true;
        } else if (this.myKey.jointName === "RightHand") {
            if (GrabLink.leftControllerDispatcherEnabled) {
                print("AJT: send disable none");
                Messages.sendMessage("Hifi-Hand-Disabler", "none");
            } else {
                print("AJT: send disable left");
                Messages.sendMessage("Hifi-Hand-Disabler", "left");
            }
            GrabLink.rightControllerDispatcherEnabled = true;
        }
    };

    GrabLink.prototype.sendGrabMessage = function () {
        var msg = {
            type: "grab",
            receiver: this.otherKey.avatarId,
            grabbingJoint: this.myKey.jointName,
            grabbedJoint: this.otherKey.jointName,
            relXform: this.relXform
        };

        print("AJT: sendGrabMessage, msg = " + JSON.stringify(msg));

        Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
    };

    GrabLink.prototype.sendReleaseMessage = function () {
        var msg = {
            type: "release",
            receiver: this.otherKey.avatarId,
            grabbingJoint: this.myKey.jointName,
            grabbedJoint: this.otherKey.jointName,
            relXform: this.relXform
        };

        print("AJT: sendReleaseMessage, msg = " + JSON.stringify(msg));

        Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
    };

    GrabLink.prototype.sendRejectMessage = function () {
        var msg = {
            type: "reject",
            receiver: this.otherKey.avatarId,
            grabbingJoint: this.myKey.jointName,
            grabbedJoint: this.otherKey.jointName,
            relXform: this.relXform
        };

        print("AJT: sendRejectMessage, msg = " + JSON.stringify(msg));

        Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
    };


    GrabLink.prototype.playClap = function () {
        clapSound.play({position: this.myJointInfo.jointPos, loop: false, localOnly: true});
    };

    GrabLink.prototype.getHapticBuddy = function () {
        if (this.myKey.jointName === "RightHand") {
            return rightHapticBuddy;
        } else if (this.myKey.jointName === "LeftHand") {
            return leftHapticBuddy;
        } else {
            return undefined;
        }
    };

    GrabLink.prototype.startStressHaptics = function () {
        var hapticBuddy = this.getHapticBuddy();
        if (hapticBuddy) {
            hapticBuddy.start(this.myJointInfo, this.otherJointInfo);
        }
    };

    GrabLink.prototype.updateStressHaptics = function () {
        var hapticBuddy = this.getHapticBuddy();
        if (hapticBuddy) {
            hapticBuddy.update(this.myJointInfo, this.otherJointInfo);
        }
    };

    GrabLink.prototype.stopStressHaptics = function () {
        var hapticBuddy = this.getHapticBuddy();
        if (hapticBuddy) {
            hapticBuddy.stop();
        }
    };

    GrabLink.prototype.startRejectHaptics = function () {
        var hapticBuddy = this.getHapticBuddy();
        if (hapticBuddy) {
            hapticBuddy.start(this.myJointInfo, this.otherJointInfo);
        }
    };

    GrabLink.prototype.updateRejectHaptics = function () {

        var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
        var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);
        var distance = Vec3.distance(myXform.pos, otherXform.pos);

        var XXX_MIN_DISTANCE = 0.0;
        var XXX_MAX_DISTANCE = REJECT_DISTANCE;
        var XXX_MIN_FREQUENCY = 0.5;
        var XXX_MAX_FREQUENCY = 3.0;

        // convert distance into a value from 0 to 1
        var factor = clamp((distance - XXX_MIN_DISTANCE) / (XXX_MAX_DISTANCE - XXX_MIN_DISTANCE), 0, 1);
        // convert factor into a frequency factor
        var frequency = factor * (XXX_MAX_FREQUENCY - XXX_MIN_FREQUENCY) + XXX_MIN_FREQUENCY;

        var hapticBuddy = this.getHapticBuddy();
        if (hapticBuddy) {
            hapticBuddy.frequencyScale = frequency;
            hapticBuddy.update(this.myJointInfo, this.otherJointInfo);
        }
    };

    GrabLink.prototype.stopRejectHaptics = function () {
        var hapticBuddy = this.getHapticBuddy();
        if (hapticBuddy) {
            hapticBuddy.stop();
        }
    };

    GrabLink.prototype.updateJointInfo = function () {
        this.myJointInfo = scanner.getJointInfo(this.myKey);
        this.otherJointInfo = scanner.getJointInfo(this.otherKey);
    };

    GrabLink.prototype.receivedGrab = function (relXform) {

        print("AJT: receivedGrab, relXform = " + JSON.stringify(relXform));

        this.updateJointInfo();
        switch (this.state) {
            case "alone":
                this.relXform = relXform;
                this.changeState("follower");
                break;
            case "leader":
                // Don't set relXform here, keep using the one we previously computed in triggerPress.
                this.changeState("peer");
                break;
            default:
                print("AJT: WARNING GrabLink.receivedGrab: illegal transition, state = " + this.state);
                break;
        }
    };

    GrabLink.prototype.receivedRelease = function () {

        print("AJT: receivedRelease");

        this.updateJointInfo();
        switch (this.state) {
            case "follower":
                this.changeState("alone");
                break;
            case "peer":
                this.changeState("leader");
                break;
            default:
                print("AJT: WARNING GrabLink.receivedRelease: illegal transition, state = " + this.state);
                break;
        }
    };

    GrabLink.prototype.reject = function () {

        print("AJT: reject");

        this.updateJointInfo();
        switch (this.state) {
            case "follower":
            case "peer":
            case "leader":
                this.changeState("reject");
                break;
            default:
                print("AJT: WARNING GrabLink.reject: illegal transition, state = " + this.state);
                break;
        }
    };

    GrabLink.prototype.triggerPress = function () {
        this.updateJointInfo();
        switch (this.state) {
            case "alone":
                this.changeState("leader");
                break;
            case "follower":
                this.changeState("peer");
                break;
            default:
                print("AJT: WARNING GrabLink.triggerGrab: illegal transition, state = " + this.state);
                break;
        }
    };

    GrabLink.prototype.triggerRelease = function () {
        this.updateJointInfo();
        switch (this.state) {
            case "leader":
                this.changeState("alone");
                break;
            case "peer":
                this.changeState("follower");
                break;
            default:
                print("AJT: WARNING GrabLink.triggerRelease: illegal transition, state = " + this.state);
                break;
        }
    };

    //
    // alone
    //

    GrabLink.prototype.aloneEnter = function () {
        HMD.requestHideHandControllers();
        if (this.state === "leader") {
            // AJT_B: leader -> alone
            this.hapticPulse();
            this.clearPinOnJoint(this.otherKey);
            this.sendReleaseMessage();
            this.enableControllerDispatcher();
        } else if (this.state === "follower") {
            // AJT_H: follower -> alone
            this.hapticPulse();
            this.clearPinOnJoint(this.myKey);
            this.enableControllerDispatcher();
        } else if (this.state === "reject") {
            // do nothing
        } else {
            print("AJT: WARNING aloneEnter from illegal state " + this.state);
        }
    };

    GrabLink.prototype.aloneProcess = function () {
        // do nothing
    };

    GrabLink.prototype.aloneExit = function () {
        HMD.requestShowHandControllers();
    };

    //
    // follower
    //

    GrabLink.prototype.followerEnter = function () {
        this.updateJointInfo();
        this.startRejectHaptics();
        if (this.state === "peer") {
            // AJT_F: peer -> follower
            this.hapticPulse();
            this.clearPinOnJoint(this.otherKey);
            this.sendReleaseMessage();
        } else if (this.state === "alone") {
            // AJT_G: alone -> follower
            this.playClap();
            this.hapticPulse();
            this.disableControllerDispatcher();
        } else {
            print("AJT: WARNING followerEnter from illegal state " + this.state);
        }
    };

    GrabLink.prototype.followerProcess = function () {
        this.updateJointInfo();

        var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
        var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);

        // AJT: TODO: dynamic change of blend factor based on distance?
        var blendFactor = 1;

        // Perform IK on MyAvatar, having our hand follow their controller.
        var myOtherXform = Xform.mul(otherXform, this.relXform);
        var myTargetXform = tweenXform(myXform, myOtherXform, blendFactor);
        this.pinJoint(this.myKey, this.myJointInfo, myTargetXform);

        if (AJT_SOLO_DEBUG && this.timeInState > AJT_SOLO_DEBUG_TIME) {
            print("AJT: fake a trigger press");
            // fake a trigger press
            this.triggerPress();
        }

        var distance = Vec3.distance(myXform.pos, myTargetXform.pos);

        this.updateRejectHaptics();

        if (distance > REJECT_DISTANCE) {
            this.sendRejectMessage();
            this.reject();
        }
    };

    GrabLink.prototype.followerExit = function () {
        this.stopRejectHaptics();
    };

    //
    // peer
    //

    GrabLink.prototype.peerEnter = function () {
        if (this.state === "leader") {
            // AJT_C: leader -> peer
            this.hapticPulse();
        } else if (this.state === "follower") {
            // AJT_E: follower -> peer
            this.hapticPulse();
            this.sendGrabMessage();
        } else {
            print("AJT: WARNING peerEnter from illegal state " + this.state);
        }
        this.startStressHaptics();
    };

    GrabLink.prototype.peerProcess = function () {
        this.updateJointInfo();

        var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
        var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);

        // AJT: TODO: dynamic change of blend factor based on distance.
        var blendFactor = 0.5;

        // Perform IK on MyAvatar, having our hand follow their controller.
        var myOtherXform = Xform.mul(otherXform, this.relXform);
        var myTargetXform = tweenXform(myXform, myOtherXform, blendFactor);
        this.pinJoint(this.myKey, this.myJointInfo, myTargetXform);

        // Perform local IK on the other avatar, making their hand follow our controller.
        var otherMyXform = Xform.mul(myXform, this.relXform.inv());
        var otherTargetXform = tweenXform(otherMyXform, otherXform, blendFactor);
        this.pinJoint(this.otherKey, this.otherJointInfo, otherTargetXform);

        this.updateStressHaptics();

        if (AJT_SOLO_DEBUG && this.timeInState > AJT_SOLO_DEBUG_TIME) {
            print("AJT: fake a trigger release");
            // fake a trigger release
            this.triggerRelease();
        }

        if (Vec3.distance(myXform.pos, myTargetXform.pos) > REJECT_DISTANCE) {
            this.sendRejectMessage();
            this.reject();
        }
    };

    GrabLink.prototype.peerExit = function () {
        this.stopStressHaptics();
    };

    //
    // leader
    //

    GrabLink.prototype.leaderEnter = function () {
        this.updateJointInfo();
        this.startRejectHaptics();
        if (this.state === "alone") {
            // AJT_A: alone -> leader
            this.playClap();
            this.hapticPulse();
            this.disableControllerDispatcher();
            this.computeDeltaXform();
            this.sendGrabMessage();
        } else if (this.state === "peer") {
            // AJT_D: peer -> leader
            this.hapticPulse();
            this.clearPinOnJoint(this.myKey);
        } else {
            print("AJT: WARNING: leaderEnter from unknown state " + this.state);
        }
    };

    GrabLink.prototype.leaderProcess = function () {
        this.updateJointInfo();

        var myXform = new Xform(this.myJointInfo.controllerRot, this.myJointInfo.controllerPos);
        var otherXform = new Xform(this.otherJointInfo.controllerRot, this.otherJointInfo.controllerPos);

        // AJT: TODO: dynamic change of blend factor based on distance?
        var blendFactor = 0;

        // Perform local IK on the other avatar, making their hand follow our controller.
        var otherMyXform = Xform.mul(myXform, this.relXform.inv());
        var otherTargetXform = tweenXform(otherMyXform, otherXform, blendFactor);
        this.pinJoint(this.otherKey, this.otherJointInfo, otherTargetXform);

        if (AJT_SOLO_DEBUG && this.timeInState > AJT_SOLO_DEBUG_TIME) {
            print("AJT: fake a trigger release");
            // fake a trigger release
            this.triggerRelease();
        }

        this.updateRejectHaptics();

        // AJT: TODO: should we reject?
    };

    GrabLink.prototype.leaderExit = function () {
        this.stopRejectHaptics();
    };

    //
    // reject
    //

    GrabLink.prototype.rejectEnter = function () {
        if (this.state === "follower") {
            // AJT_I: follower -> reject
            this.hapticPulse();
            this.clearPinOnJoint(this.myKey);
        } else if (this.state === "peer") {
            // AJT_J: peer -> reject
            this.hapticPulse();
            this.clearPinOnJoint(this.otherKey);
            this.clearPinOnJoint(this.myKey);
        } else if (this.state === "leader") {
            // AJT_K: leader -> reject
            this.hapticPulse();
            this.clearPinOnJoint(this.otherKey);
        } else {
            print("AJT: WARNING: rejectEnter from unknown state " + this.state);
        }
    };

    GrabLink.prototype.rejectProcess = function () {
        // immediately go to the alone state
        this.changeState("alone");
    };

    GrabLink.prototype.rejectExit = function () {
        // do nothing
    };

    var GRABBABLE_JOINT_NAMES = [
        "RightHand",
        "LeftHand"
    ];

    if (ALLOW_LIMB_GRABBING) {
        GRABBABLE_JOINT_NAMES = GRABBABLE_JOINT_NAMES.concat([
            "Hips",
            "RightUpLeg",
            "RightLeg",
            "RightFoot",
            "LeftUpLeg",
            "LeftLeg",
            "LeftFoot",
            "Spine2",
            "Neck",
            "Head",
            "RightShoulder",
            "RightArm",
            "RightForeArm",
            "RightHand",
            "LeftShoulder",
            "LeftArm",
            "LeftForeArm",
            "LeftHand"
        ]);
    }

    // ctor
    function GrabbableJointScanner(addAvatarCb, updateAvatarCb, removeAvatarCb) {
        this.addAvatarCb = addAvatarCb;
        this.updateAvatarCb = updateAvatarCb;
        this.removeAvatarCb = removeAvatarCb;
        this.prevAvatarMap = {};
    }

    GrabbableJointScanner.prototype.destroy = function () {
        // remove all avatars
        Object.keys(this.prevAvatarMap).forEach(function (id) {
            if (this.removeAvatarCb) {
                this.removeAvatarCb(id);
            }
        });
    };

    /*
     * Returns an object with jointName's as keys, and jointInfo's as values,
     * where jointInfo is an object like the following:
     *
     * {
     *   jointName: string,
     *   jointIndex: number,
     *   controllerRot: quat,
     *   controllerPos: vec3,
     *   controllerValid: bool,
     *   jointRot: quat,
     *   jointPos: pos,
     *   jointValid: bool
     * }
     *
     * All positions and rotations are in world space
     *
     */
    GrabbableJointScanner.prototype.computeJointInfoMap = function (id) {
        var avatar = id ? AvatarManager.getAvatar(id) : MyAvatar;
        var avatarXform = new Xform(avatar.orientation, avatar.position);
        var jointInfoMap = {};
        GRABBABLE_JOINT_NAMES.forEach(function (jointName) {
            var jointIndex = avatar.getJointIndex(jointName);
            if (jointIndex < 0) {
                return;
            }
            var result = {
                jointName: jointName,
                jointIndex: jointIndex,
                controllerRot: {x: 0, y: 0, z: 0, w: 1},
                controllerPos: {x: 0, y: 0, z: 0},
                controllerValid: false,
                jointRot: {x: 0, y: 0, z: 0, w: 1},
                jointPos: {x: 0, y: 0, z: 0},
                jointValid: false
            };

            // transform joint into world space
            var localXform = new Xform(avatar.getAbsoluteJointRotationInObjectFrame(jointIndex),
                                       avatar.getAbsoluteJointTranslationInObjectFrame(jointIndex));
            var worldXform = Xform.mul(avatarXform, localXform);
            result.jointRot = worldXform.rot;
            result.jointPos = worldXform.pos;
            result.jointValid = true;

            // get controller pos/rot, if available.
            if (jointName === "LeftHand" || jointName === "RightHand") {
                var mat, palmPos, palmRot;
                if (jointName === "LeftHand") {
                    mat = avatar.controllerLeftHandMatrix;
                    palmPos = avatar.getLeftPalmPosition();
                    palmRot = avatar.getLeftPalmRotation();
                } else {
                    mat = avatar.controllerRightHandMatrix;
                    palmPos = avatar.getRightPalmPosition();
                    palmRot = avatar.getRightPalmRotation();
                }

                var localPalmPos = worldXform.inv().xformPoint(palmPos);

                var PALM_Y_OFFSET_FACTOR = 0.8;
                var PALM_Z_OFFSET_FACTOR = 0.4;

                // adjust the palm position to better match the handshake palm position.
                localPalmPos.y = localPalmPos.y * PALM_Y_OFFSET_FACTOR;
                localPalmPos.z = localPalmPos.z * PALM_Z_OFFSET_FACTOR;

                // transform local palm back into world space
                result.palmPos = worldXform.xformPoint(localPalmPos);
                result.palmRot = palmRot;

                // check to see if hand controller is valid
                var localControllerXform = new Xform(Mat4.extractRotation(mat),
                                                     Mat4.extractTranslation(mat));
                if (!isIdentity(localControllerXform.rot, localControllerXform.pos)) {
                    var worldControllerXform = Xform.mul(avatarXform, localControllerXform);
                    result.controllerRot = worldControllerXform.rot;
                    result.controllerPos = worldControllerXform.pos;
                    result.controllerValid = true;
                } else {
                    result.controllerRot = result.jointRot;
                    result.controllerPos = result.jointPos;
                    result.controllerValid = false;
                }
            } else {
                result.controllerRot = result.jointRot;
                result.controllerPos = result.jointPos;
                result.controllerValid = false;
            }

            jointInfoMap[jointName] = result;
        });

        return jointInfoMap;
    };

    GrabbableJointScanner.prototype.scan = function () {

        var AVATAR_RANGE = 5; // meters
        var avatarIds = AvatarManager.getAvatarsInRange(MyAvatar.position, AVATAR_RANGE);
        var avatarMap = {};
        var self = this;
        avatarIds.forEach(function (id) {
            if (id === null || id === MyAvatar.sessionUUID) {
                id = MyAvatar.SELF_ID;
            }
            avatarMap[id] = {
                jointInfoMap: self.computeJointInfoMap(id)
            };
        });

        // detect removal
        Object.keys(this.prevAvatarMap).forEach(function (id) {
            if (!avatarMap[id]) {
                if (self.removeAvatarCb) {
                    self.removeAvatarCb(id);
                }
            }
        });

        // detect addition
        Object.keys(avatarMap).forEach(function (id) {
            if (self.prevAvatarMap[id]) {
                if (self.updateAvatarCb) {
                    self.updateAvatarCb(id, avatarMap[id]);
                }
            } else {
                if (self.addAvatarCb) {
                    self.addAvatarCb(id, avatarMap[id]);
                }
            }
        });

        this.prevAvatarMap = avatarMap;
    };

    /*
     * searches the most recently scanned jointInfoMap for a grabbable joint on other avatar.
     * it will return null, if none is found or a key object like the following:
     * {
     *   avatarId: uuid,
     *   jointName: string
     * }
     *
     */
    GrabbableJointScanner.prototype.findGrabbableJoint = function (hand, grabDistance) {

        var myHand = this.getMyHand(hand);
        if (!myHand) {
            return null;
        }

        var avatarIds = Object.keys(this.prevAvatarMap);
        var closestId;
        var closestJointName;
        var closestDist = Number.MAX_VALUE;
        var self = this;
        avatarIds.forEach(function (id) {
            if (id !== MyAvatar.SELF_ID) {
                var jointInfoMap = self.prevAvatarMap[id].jointInfoMap;
                Object.keys(jointInfoMap).forEach(function (jointName) {
                    var jointInfo = jointInfoMap[jointName];
                    var dist = Vec3.distance(myHand.jointPos, jointInfo.jointPos);
                    if (dist < grabDistance && dist < closestDist) {
                        closestId = id;
                        closestJointName = jointName;
                        closestDist = dist;
                    }
                });
            }
        });


        if (!closestId) {
            return null;
        } else {
            return {avatarId: closestId, jointName: closestJointName};
        }
    };

    /*
     * keyObj should be of the following format:
     * {
     *   avatarId: uuid,
     *   jointName: string
     * }
     *
     * the result will be null, if no jointInfo is found or an object with the following format:
     *
     * {
     *   jointName: string,
     *   jointIndex: number,
     *   controllerRot: quat,
     *   controllerPos: vec3,
     *   controllerValid: bool,
     *   jointRot: quat,
     *   jointPos: pos,
     *   jointValid: bool
     * }
     *
     */
    GrabbableJointScanner.prototype.getJointInfo = function (keyObj) {
        if (keyObj) {
            return this.prevAvatarMap[keyObj.avatarId].jointInfoMap[keyObj.jointName];
        }
    };

    GrabbableJointScanner.prototype.getMyHand = function (hand) {
        if (hand === RIGHT_HAND) {
            return this.getJointInfo({avatarId: MyAvatar.SELF_ID, jointName: "RightHand"});
        } else {
            return this.getJointInfo({avatarId: MyAvatar.SELF_ID, jointName: "LeftHand"});
        }
    };

    function tweenXform(a, b, alpha) {
        return new Xform(Quat.mix(a.rot, b.rot, alpha), Vec3.sum(Vec3.multiply(1 - alpha, a.pos), Vec3.multiply(alpha, b.pos)));
    }

    function calculateDeltaOffset(myHand, jointInfo) {
        var myJointXform = new Xform(myHand.jointRot, myHand.jointPos);
        var otherJointXform = new Xform(jointInfo.jointRot, jointInfo.jointPos);
        return Xform.mul(otherJointXform.inv(), myJointXform);
    }

    function calculateHandDeltaOffset(myHand, otherHand) {
        var myJointXform = new Xform(myHand.jointRot, myHand.jointPos);
        var otherJointXform = new Xform(otherHand.jointRot, otherHand.jointPos);
        var palmOffset = new Xform(QUAT_IDENTITY, Vec3.subtract(otherHand.palmPos, myHand.palmPos));
        return Xform.mul(otherJointXform.inv(), Xform.mul(palmOffset, myJointXform));
    }

    var leftHandActiveKeys;

    // Controller system callback on leftTrigger pull or release.
    function leftTrigger(value) {
        var myKey = {avatarId: MyAvatar.SELF_ID, jointName: "LeftHand"};
        var grabLink;
        if (value === 1) {
            var otherKey = scanner.findGrabbableJoint(LEFT_HAND, GRAB_DISTANCE);
            if (otherKey) {
                grabLink = GrabLink.findOrCreateLink(myKey, otherKey);
                grabLink.triggerPress();

                leftHandActiveKeys = {myKey: myKey, otherKey: otherKey};
            }
        } else if (leftHandActiveKeys) {
            grabLink = GrabLink.findLink(leftHandActiveKeys.myKey, leftHandActiveKeys.otherKey);
            if (grabLink) {
                grabLink.triggerRelease();
            } else {
                print("AJT: WARNING, leftTrigger(), could not find gripLink for LeftHand");
            }
            leftHandActiveKeys = undefined;
        }
    }

    var rightHandActiveKeys;

    // Controller system callback on rightTrigger pull or release.
    function rightTrigger(value) {
        var myKey = {avatarId: MyAvatar.SELF_ID, jointName: "RightHand"};
        var grabLink;
        if (value === 1) {
            var otherKey = scanner.findGrabbableJoint(RIGHT_HAND, GRAB_DISTANCE);
            if (otherKey) {
                grabLink = GrabLink.findOrCreateLink(myKey, otherKey);
                grabLink.triggerPress();

                rightHandActiveKeys = {myKey: myKey, otherKey: otherKey};
            }
        } else if (rightHandActiveKeys) {
            grabLink = GrabLink.findLink(rightHandActiveKeys.myKey, rightHandActiveKeys.otherKey);
            if (grabLink) {
                grabLink.triggerRelease();
            } else {
                print("AJT: WARNING, rightTrigger(), could not find gripLink for RightHand");
            }
            rightHandActiveKeys = undefined;
        }
    }

    var connectionMapping = Controller.newMapping('handshake-grip');
    var animationHandlerId;
    var ANIM_VARS = [
        "leftHandPosition",
        "leftHandRotation",
        "leftHandType",
        "rightHandPosition",
        "rightHandRotation",
        "rightHandType"
    ];

    // message should look like:
    // {type: "grab", receiver: uuid, grabbingJoint: string, grabbedJoint: string, relXform: {pos: Vec3, rot: Quat}}
    // {type: "release", receiver: uuid, grabbingJoint: string, grabbedJoint: string}
    function messageHandler(channel, message, sender) {
        if (channel === "Hifi-Handshake") {

            print("AJT: messageHandler, msg = " + message);

            var obj = JSON.parse(message);
            if (obj.receiver === MyAvatar.sessionUUID) {
                var myKey = {avatarId: MyAvatar.SELF_ID, jointName: obj.grabbedJoint};
                var otherKey = {avatarId: sender, jointName: obj.grabbingJoint};
                var grabLink;
                if (obj.type === "grab") {
                    grabLink = GrabLink.findOrCreateLink(myKey, otherKey);
                    var relXform = new Xform(obj.relXform.rot, obj.relXform.pos);
                    grabLink.receivedGrab(relXform.inv());
                } else if (obj.type === "release") {
                    grabLink = GrabLink.findLink(myKey, otherKey);
                    if (grabLink) {
                        grabLink.receivedRelease();
                    } else {
                        print("AJT: WARNING, messageHandler() release, could not find gripLink for " + myKey.grabbedJoint);
                    }
                } else if (obj.type === "reject") {
                    grabLink = GrabLink.findLink(myKey, otherKey);
                    if (grabLink) {
                        grabLink.reject();
                    } else {
                        print("AJT: WARNING, messageHandler() reject, could not find gripLink for " + myKey.grabbedJoint);
                    }
                }
            }
        }
    }

    function init() {

        rightHandPose = Controller.getPoseValue(Controller.Standard.RightHand);
        leftHandPose = Controller.getPoseValue(Controller.Standard.LeftHand);

        connectionMapping.from(Controller.Standard.RTClick).peek().to(rightTrigger);
        connectionMapping.from(Controller.Standard.LTClick).peek().to(leftTrigger);
        connectionMapping.enable();

        animationHandlerId = MyAvatar.addAnimationStateHandler(function (props) {

            var result = {};

            if (rightHandPose.valid) {
                result.rightHandType = 0; // RotationAndPosition
            } else {
                result.rightHandType = 3; // HipsRelativeRotationAndPosition
            }

            if (leftHandPose.valid) {
                result.leftHandType = 0; // RotationAndPosition
            } else {
                result.leftHandType = 3; // HipsRelativeRotationAndPosition
            }

            var avatarXform = new Xform(Quat.multiply(MyAvatar.orientation, QUAT_Y_180), MyAvatar.position);

            if (myAvatarRightHandXform) {
                var rightHandTargetXform = Xform.mul(avatarXform.inv(), myAvatarRightHandXform);
                result.rightHandRotation = rightHandTargetXform.rot;
                result.rightHandPosition = rightHandTargetXform.pos;
            } else {
                delete result.rightHandRotation;
                delete result.rightHandPosition;
            }

            if (myAvatarLeftHandXform) {
                var leftHandTargetXform = Xform.mul(avatarXform.inv(), myAvatarLeftHandXform);
                result.leftHandRotation = leftHandTargetXform.rot;
                result.leftHandPosition = leftHandTargetXform.pos;
            } else {
                delete result.leftHandRotation;
                delete result.leftHandPosition;
            }

            return result;
        }, ANIM_VARS);

        Script.update.connect(update);
        Script.scriptEnding.connect(shutdown);

        Messages.subscribe("Hifi-Handshake");
        Messages.messageReceived.connect(messageHandler);
    }

    function isIdentity(rot, pos) {
        var EPSILON = 0.01;
        return (Math.abs(rot.x) < EPSILON &&
                Math.abs(rot.y) < EPSILON &&
                Math.abs(rot.z) < EPSILON &&
                Math.abs(rot.w) - 1.0 < EPSILON && // 1 or -1 is ok.
                Math.abs(pos.x) < EPSILON &&
                Math.abs(pos.y) < EPSILON &&
                Math.abs(pos.z) < EPSILON);
    }

    function addAvatar(id, data) {
        print("addAvatar(" + id + ")");
        print("typeof id = " + (typeof id));
        print("JSON.stringify(id) = " + JSON.stringify(id));
        if (id === "null") {
            id = MyAvatar.SELF_ID;
        }
        print("addAvatar(" + id + ")");
        updateAvatar(id, data);
    }

    function updateAvatar(id, data) {
    }

    function removeAvatar(id) {
        print("removeAvatar(" + id + ")");
        DebugDraw.removeMarker("leftHandController" + id);
        DebugDraw.removeMarker("leftHandJoint" + id);
        DebugDraw.removeMarker("rightHandController" + id);
        DebugDraw.removeMarker("rightHandJoint" + id);
    }

    var scanner = new GrabbableJointScanner(addAvatar, updateAvatar, removeAvatar);

    var leftHandPose;
    var rightHandPose;

    function update(dt) {

        // update hand poses
        rightHandPose = Controller.getPoseValue(Controller.Standard.RightHand);
        leftHandPose = Controller.getPoseValue(Controller.Standard.LeftHand);

        scanner.scan();
        GrabLink.process(dt);
    }

    function shutdown() {
        Messages.messageReceived.disconnect(messageHandler);
        MyAvatar.removeAnimationStateHandler(animationHandlerId);

        scanner.destroy();

        connectionMapping.disable();
        shutdownTabletApp();
    }

    init();

}()); // END LOCAL_SCOPE
