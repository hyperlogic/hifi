//
// handshake.ts
//
/* global Xform */
/* eslint max-len: ["error", 1024, 4] */
/* eslint brace-style: "stroustrup" */
(function () {
    Script.include("/~/system/libraries/Xform.js");
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
        }
        else {
            tablet.gotoWebScreen(HTML_URL);
        }
    });
    var shown = false;
    function onScreenChanged(type, url) {
        if (type === "Web" && url === HTML_URL) {
            tabletButton.editProperties({ isActive: true });
            if (!shown) {
                // hook up to event bridge
                tablet.webEventReceived.connect(onWebEventReceived);
            }
            shown = true;
        }
        else {
            tabletButton.editProperties({ isActive: false });
            if (shown) {
                // disconnect from event bridge
                tablet.webEventReceived.disconnect(onWebEventReceived);
            }
            shown = false;
        }
    }
    function onWebEventReceived(msg) {
        if (msg.name === "init-complete") {
            var values = [{ name: "grab-distance", val: GRAB_DISTANCE * 100, checked: false },
                { name: "reject-distance", val: REJECT_DISTANCE * 100, checked: false }];
            tablet.emitScriptEvent(JSON.stringify(values));
        }
        else if (msg.name === "grab-distance") {
            GRAB_DISTANCE = parseInt(msg.val, 10) / 100;
        }
        else if (msg.name === "reject-distance") {
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
    var QUAT_Y_180 = { x: 0, y: 1, z: 0, w: 0 };
    var QUAT_IDENTITY = { x: 0, y: 0, z: 0, w: 1 };
    var LEFT_HAND = 0;
    var RIGHT_HAND = 1;
    var myAvatarRightHandXform;
    var myAvatarLeftHandXform;
    var HAPTIC_PULSE_FIRST_STRENGTH = 1.0;
    var HAPTIC_PULSE_FIRST_DURATION = 16.0;
    var HAPTIC_PULSE_DISTANCE = 0.01; // 1 cm
    var HAPTIC_PULSE_MAX_STRENGTH = 0.5;
    // used by GrabLink
    var HAPTIC_PULSE_STRENGTH = 1.0;
    var HAPTIC_PULSE_DURATION = 1.0;
    var HapticBuddy = /** @class */ (function () {
        function HapticBuddy(hand) {
            this.hand = hand;
            this.enabled = false;
            this.frequencyScale = 1;
            this.lastPulseDistance = 0;
        }
        HapticBuddy.prototype.setFrequencyScale = function (frequencyScale) {
            this.frequencyScale = frequencyScale;
        };
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
        return HapticBuddy;
    }());
    var rightHapticBuddy = new HapticBuddy(RIGHT_HAND);
    var leftHapticBuddy = new HapticBuddy(LEFT_HAND);
    var SoundBuddy = /** @class */ (function () {
        function SoundBuddy(url) {
            this.sound = SoundCache.getSound(url);
            this.injector = null;
        }
        SoundBuddy.prototype.play = function (options, doneCallback) {
            if (this.sound.downloaded) {
                if (this.injector) {
                    this.injector.setOptions(options);
                    this.injector.restart();
                }
                else {
                    this.injector = Audio.playSound(this.sound, options);
                    this.injector.finished.connect(function () {
                        if (doneCallback) {
                            doneCallback();
                        }
                    });
                }
            }
        };
        return SoundBuddy;
    }());
    var CLAP_SOUND = "https://s3.amazonaws.com/hifi-public/tony/audio/slap.wav";
    var clapSound = new SoundBuddy(CLAP_SOUND);
    function grabKeyPairtoString(primaryGrabKey, secondaryGrabKey) {
        return primaryGrabKey.avatarId + "_" + primaryGrabKey.jointName + "&&" + secondaryGrabKey.avatarId + "_" + secondaryGrabKey.jointName;
    }
    var GrabMessageType;
    (function (GrabMessageType) {
        GrabMessageType["Grab"] = "GRAB";
        GrabMessageType["Release"] = "RELEASE";
        GrabMessageType["Reject"] = "REJECT";
    })(GrabMessageType || (GrabMessageType = {}));
    var GrabLinkState;
    (function (GrabLinkState) {
        GrabLinkState["Alone"] = "ALONE";
        GrabLinkState["Leader"] = "LEADER";
        GrabLinkState["Follower"] = "FOLLOWER";
        GrabLinkState["Peer"] = "PEER";
        GrabLinkState["Reject"] = "REJECT";
    })(GrabLinkState || (GrabLinkState = {}));
    // A GrabLink is single grab interaction link between two avatars, typically between MyAvatar and another avatar in the scene.
    // It has an internal state machine to keep track of when to enable/disable IK and play sound effects.
    // state element of {alone, leader, follower, peer, reject}
    var GrabLink = /** @class */ (function () {
        function GrabLink(primaryKey, secondaryKey) {
            this.pairKey = grabKeyPairtoString(primaryKey, secondaryKey);
            console.warn("AJT: new GrabLink(" + this.pairKey + ")");
            this.primaryKey = primaryKey;
            this.secondaryKey = secondaryKey;
            this.state = GrabLinkState.Alone;
            this.relXform = new Xform({ x: 0, y: 0, z: 0, w: 1 }, { x: 0, y: 0, z: 0 });
            this.states = (_a = {},
                _a[GrabLinkState.Alone] = {
                    enter: this.aloneEnter,
                    process: this.aloneProcess,
                    exit: this.aloneExit
                },
                _a[GrabLinkState.Follower] = {
                    enter: this.followerEnter,
                    process: this.followerProcess,
                    exit: this.followerExit
                },
                _a[GrabLinkState.Peer] = {
                    enter: this.peerEnter,
                    process: this.peerProcess,
                    exit: this.peerExit
                },
                _a[GrabLinkState.Leader] = {
                    enter: this.leaderEnter,
                    process: this.leaderProcess,
                    exit: this.leaderExit
                },
                _a[GrabLinkState.Reject] = {
                    enter: this.rejectEnter,
                    process: this.rejectProcess,
                    exit: this.rejectExit
                },
                _a);
            this.primaryJointInfo = scanner.getJointInfo(primaryKey);
            this.secondaryJointInfo = scanner.getJointInfo(secondaryKey);
            this.timeInState = 0;
            this.initiator = false;
            var _a;
        }
        GrabLink.findOrCreateLink = function (primaryKey, secondaryKey) {
            var key = grabKeyPairtoString(primaryKey, secondaryKey);
            var link = GrabLink.myAvatarGrabLinkMap[key];
            if (!link) {
                link = new GrabLink(primaryKey, secondaryKey);
                GrabLink.myAvatarGrabLinkMap[key] = link;
            }
            return link;
        };
        GrabLink.findLink = function (primaryKey, secondaryKey) {
            var key = grabKeyPairtoString(primaryKey, secondaryKey);
            return GrabLink.myAvatarGrabLinkMap[key];
        };
        GrabLink.findOrCreateOtherAvatarLink = function (primaryKey, secondaryKey) {
            var key = grabKeyPairtoString(primaryKey, secondaryKey);
            var link = GrabLink.otherAvatarGrabLinkMap[key];
            if (!link) {
                link = new GrabLink(primaryKey, secondaryKey);
                GrabLink.otherAvatarGrabLinkMap[key] = link;
            }
            return link;
        };
        GrabLink.findOtherAvatarLink = function (primaryKey, secondaryKey) {
            var key = grabKeyPairtoString(primaryKey, secondaryKey);
            return GrabLink.otherAvatarGrabLinkMap[key];
        };
        GrabLink.process = function (dt) {
            Object.keys(GrabLink.myAvatarGrabLinkMap).forEach(function (key) {
                GrabLink.myAvatarGrabLinkMap[key].process(dt);
            });
            Object.keys(GrabLink.otherAvatarGrabLinkMap).forEach(function (key) {
                GrabLink.otherAvatarGrabLinkMap[key].process(dt);
            });
        };
        GrabLink.prototype.changeState = function (newState) {
            if (this.state !== newState) {
                console.warn("AJT: GrabLink(" + this.pairKey + "), changeState " + this.state + " -> " + newState);
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
            if (this.primaryKey.jointName.slice(0, 4) === "Left") {
                Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, LEFT_HAND);
            }
            else if (this.primaryKey.jointName.slice(0, 5) === "Right") {
                Controller.triggerHapticPulse(HAPTIC_PULSE_STRENGTH, HAPTIC_PULSE_DURATION, RIGHT_HAND);
            }
        };
        GrabLink.prototype.clearPinOnJoint = function (key) {
            console.warn("AJT: clearPinOnJoint(), pairKey = " + this.pairKey + ", key = " + JSON.stringify(key));
            if (key.avatarId === MyAvatar.SELF_ID) {
                console.warn("AJT: clearPinOnJoint() myAvatar!");
                // AJT: TODO for now we only support hands for MyAvatar
                if (key.jointName === "LeftHand") {
                    console.warn("AJT: clearPinOnJoint(), myAvatar, leftHand");
                    myAvatarLeftHandXform = undefined;
                }
                else if (key.jointName === "RightHand") {
                    console.warn("AJT: clearPinOnJoint(), myAvatar, rightHand");
                    myAvatarRightHandXform = undefined;
                }
            }
            else {
                console.warn("AJT: clearPinOnJoint(), other avatar!");
                var avatar = AvatarManager.getAvatar(key.avatarId);
                if (avatar) {
                    console.warn("AJT: clearPinOnJoint(), otherAvatar, jointIndex = " + this.secondaryJointInfo.jointIndex);
                    avatar.clearPinOnJoint(this.secondaryJointInfo.jointIndex);
                }
                else {
                    console.warn("AJT: WARNING: clearPinOnJoint(), no avatar found");
                }
            }
        };
        GrabLink.prototype.pinJoint = function (key, jointInfo, targetXform) {
            if (key.avatarId === MyAvatar.SELF_ID) {
                // AJT: TODO for now we only support hands for MyAvatar
                // Modify the myAvatar*HandXform global which will control MyAvatar's IK
                if (key.jointName === "LeftHand") {
                    myAvatarLeftHandXform = targetXform;
                }
                else if (key.jointName === "RightHand") {
                    myAvatarRightHandXform = targetXform;
                }
            }
            else {
                var avatar = AvatarManager.getAvatar(key.avatarId);
                if (avatar) {
                    avatar.pinJoint(jointInfo.jointIndex, targetXform.pos, targetXform.rot);
                }
                else {
                    console.warn("AJT: WARNING: pinJoint(), no avatar found");
                }
            }
        };
        GrabLink.prototype.computeDeltaXform = function () {
            // compute delta xform
            if ((this.primaryJointInfo.jointName === "LeftHand" && this.secondaryJointInfo.jointName === "LeftHand") ||
                (this.primaryJointInfo.jointName === "RightHand" && this.secondaryJointInfo.jointName === "RightHand")) {
                this.relXform = calculateHandDeltaOffset(this.primaryJointInfo, this.secondaryJointInfo);
            }
            else {
                this.relXform = calculateDeltaOffset(this.primaryJointInfo, this.secondaryJointInfo);
            }
        };
        GrabLink.prototype.enableControllerDispatcher = function () {
            if (this.primaryKey.avatarId !== MyAvatar.SELF_ID) {
                return;
            }
            // enable controller dispatcher script for this hand.
            if (this.primaryKey.jointName === "LeftHand") {
                if (GrabLink.rightControllerDispatcherEnabled) {
                    console.warn("AJT: send disable none");
                    Messages.sendMessage("Hifi-Hand-Disabler", "none");
                }
                else {
                    console.warn("AJT: send disable right");
                    Messages.sendMessage("Hifi-Hand-Disabler", "right");
                }
                GrabLink.leftControllerDispatcherEnabled = true;
            }
            else if (this.primaryKey.jointName === "RightHand") {
                if (GrabLink.leftControllerDispatcherEnabled) {
                    console.warn("AJT: send disable none");
                    Messages.sendMessage("Hifi-Hand-Disabler", "none");
                }
                else {
                    console.warn("AJT: send disable left");
                    Messages.sendMessage("Hifi-Hand-Disabler", "left");
                }
                GrabLink.rightControllerDispatcherEnabled = true;
            }
        };
        GrabLink.prototype.disableControllerDispatcher = function () {
            if (this.primaryKey.avatarId !== MyAvatar.SELF_ID) {
                return;
            }
            // disable controller dispatcher script for this hand.
            if (this.primaryKey.jointName === "LeftHand") {
                if (GrabLink.rightControllerDispatcherEnabled) {
                    console.warn("AJT: send disable left");
                    Messages.sendMessage("Hifi-Hand-Disabler", "left");
                }
                else {
                    console.warn("AJT: send disable both");
                    Messages.sendMessage("Hifi-Hand-Disabler", "both");
                }
                GrabLink.leftControllerDispatcherEnabled = false;
            }
            else if (this.primaryKey.jointName === "RightHand") {
                if (GrabLink.leftControllerDispatcherEnabled) {
                    console.warn("AJT: send disable right");
                    Messages.sendMessage("Hifi-Hand-Disabler", "right");
                }
                else {
                    console.warn("AJT: send disable both");
                    Messages.sendMessage("Hifi-Hand-Disabler", "both");
                }
                GrabLink.rightControllerDispatcherEnabled = false;
            }
        };
        GrabLink.prototype.transmitMessage = function (type) {
            if (this.primaryKey.avatarId !== MyAvatar.SELF_ID) {
                return;
            }
            var msg = {
                type: type,
                receiver: this.secondaryKey.avatarId,
                grabbingJoint: this.primaryKey.jointName,
                grabbedJoint: this.secondaryKey.jointName,
                relXform: this.relXform,
                initiator: this.initiator
            };
            console.warn("AJT: transmitMessage, msg = " + JSON.stringify(msg));
            Messages.sendMessage("Hifi-Handshake", JSON.stringify(msg));
        };
        GrabLink.prototype.playClap = function () {
            if (this.primaryKey.avatarId !== MyAvatar.SELF_ID) {
                return;
            }
            clapSound.play({ position: this.primaryJointInfo.jointPos, loop: false });
        };
        GrabLink.prototype.getHapticBuddy = function () {
            if (this.primaryKey.jointName === "RightHand") {
                return rightHapticBuddy;
            }
            else if (this.primaryKey.jointName === "LeftHand") {
                return leftHapticBuddy;
            }
            else {
                return undefined;
            }
        };
        GrabLink.prototype.startStressHaptics = function () {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.start(this.primaryJointInfo, this.secondaryJointInfo);
            }
        };
        GrabLink.prototype.updateStressHaptics = function () {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.update(this.primaryJointInfo, this.secondaryJointInfo);
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
                hapticBuddy.start(this.primaryJointInfo, this.secondaryJointInfo);
            }
        };
        GrabLink.prototype.updateRejectHaptics = function () {
            var myXform = new Xform(this.primaryJointInfo.controllerRot, this.primaryJointInfo.controllerPos);
            var otherXform = new Xform(this.secondaryJointInfo.controllerRot, this.secondaryJointInfo.controllerPos);
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
                hapticBuddy.setFrequencyScale(frequency);
                hapticBuddy.update(this.primaryJointInfo, this.secondaryJointInfo);
            }
        };
        GrabLink.prototype.stopRejectHaptics = function () {
            var hapticBuddy = this.getHapticBuddy();
            if (hapticBuddy) {
                hapticBuddy.stop();
            }
        };
        GrabLink.prototype.updateJointInfo = function () {
            this.primaryJointInfo = scanner.getJointInfo(this.primaryKey);
            this.secondaryJointInfo = scanner.getJointInfo(this.secondaryKey);
        };
        GrabLink.prototype.receivedGrab = function (relXform) {
            console.warn("AJT: receivedGrab, relXform = " + JSON.stringify(relXform));
            this.updateJointInfo();
            switch (this.state) {
                case GrabLinkState.Alone:
                    this.initiator = false;
                    this.relXform = relXform;
                    this.changeState(GrabLinkState.Follower);
                    break;
                case GrabLinkState.Leader:
                    // Don't set relXform here, keep using the one we previously computed in triggerPress.
                    this.changeState(GrabLinkState.Peer);
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.receivedGrab: illegal transition, state = " + this.state);
                    break;
            }
        };
        GrabLink.prototype.receivedRelease = function () {
            console.warn("AJT: receivedRelease");
            this.updateJointInfo();
            switch (this.state) {
                case GrabLinkState.Follower:
                    this.changeState(GrabLinkState.Alone);
                    break;
                case GrabLinkState.Peer:
                    this.changeState(GrabLinkState.Leader);
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.receivedRelease: illegal transition, state = " + this.state);
                    break;
            }
        };
        GrabLink.prototype.reject = function () {
            console.warn("AJT: reject");
            this.updateJointInfo();
            switch (this.state) {
                case GrabLinkState.Follower:
                case GrabLinkState.Peer:
                case GrabLinkState.Leader:
                    this.changeState(GrabLinkState.Reject);
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.reject: illegal transition, state = " + this.state);
                    break;
            }
        };
        GrabLink.prototype.triggerPress = function (relXformOverride) {
            this.updateJointInfo();
            switch (this.state) {
                case GrabLinkState.Alone:
                    this.initiator = true;
                    this.changeState(GrabLinkState.Leader);
                    if (relXformOverride) {
                        this.relXform = relXformOverride;
                    }
                    break;
                case GrabLinkState.Follower:
                    this.changeState(GrabLinkState.Peer);
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.triggerGrab: illegal transition, state = " + this.state);
                    break;
            }
        };
        GrabLink.prototype.triggerRelease = function () {
            this.updateJointInfo();
            switch (this.state) {
                case GrabLinkState.Leader:
                    this.changeState(GrabLinkState.Alone);
                    break;
                case GrabLinkState.Peer:
                    this.changeState(GrabLinkState.Follower);
                    break;
                default:
                    console.warn("AJT: WARNING GrabLink.triggerRelease: illegal transition, state = " + this.state);
                    break;
            }
        };
        //
        // alone
        //
        GrabLink.prototype.aloneEnter = function () {
            HMD.requestHideHandControllers();
            if (this.state === GrabLinkState.Leader) {
                // AJT_B: leader -> alone
                this.hapticPulse();
                this.clearPinOnJoint(this.secondaryKey);
                this.transmitMessage(GrabMessageType.Release);
                this.enableControllerDispatcher();
            }
            else if (this.state === GrabLinkState.Follower) {
                // AJT_H: follower -> alone
                this.hapticPulse();
                this.clearPinOnJoint(this.primaryKey);
                this.enableControllerDispatcher();
            }
            else if (this.state === GrabLinkState.Reject) {
                // do nothing
            }
            else {
                console.warn("AJT: WARNING aloneEnter from illegal state " + this.state);
            }
            this.initiator = false;
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
            if (this.state === GrabLinkState.Peer) {
                // AJT_F: peer -> follower
                this.hapticPulse();
                this.clearPinOnJoint(this.secondaryKey);
                this.transmitMessage(GrabMessageType.Release);
            }
            else if (this.state === GrabLinkState.Alone) {
                // AJT_G: alone -> follower
                this.hapticPulse();
                this.disableControllerDispatcher();
            }
            else {
                console.warn("AJT: WARNING followerEnter from illegal state " + this.state);
            }
        };
        GrabLink.prototype.followerProcess = function () {
            this.updateJointInfo();
            var myXform = new Xform(this.primaryJointInfo.controllerRot, this.primaryJointInfo.controllerPos);
            var otherXform = new Xform(this.secondaryJointInfo.controllerRot, this.secondaryJointInfo.controllerPos);
            // AJT: TODO: dynamic change of blend factor based on distance?
            var blendFactor = 1;
            // Perform IK on MyAvatar, having our hand follow their controller.
            var myOtherXform = Xform.mul(otherXform, this.relXform);
            var myTargetXform = tweenXform(myXform, myOtherXform, blendFactor);
            this.pinJoint(this.primaryKey, this.primaryJointInfo, myTargetXform);
            var distance = Vec3.distance(myXform.pos, myTargetXform.pos);
            this.updateRejectHaptics();
            if (distance > REJECT_DISTANCE) {
                this.transmitMessage(GrabMessageType.Reject);
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
            if (this.state === GrabLinkState.Leader) {
                // AJT_C: leader -> peer
                this.hapticPulse();
            }
            else if (this.state === GrabLinkState.Follower) {
                // AJT_E: follower -> peer
                this.hapticPulse();
                this.transmitMessage(GrabMessageType.Grab);
            }
            else {
                console.warn("AJT: WARNING peerEnter from illegal state " + this.state);
            }
            this.startStressHaptics();
        };
        GrabLink.prototype.peerProcess = function () {
            this.updateJointInfo();
            var myXform = new Xform(this.primaryJointInfo.controllerRot, this.primaryJointInfo.controllerPos);
            var otherXform = new Xform(this.secondaryJointInfo.controllerRot, this.secondaryJointInfo.controllerPos);
            // AJT: TODO: dynamic change of blend factor based on distance.
            var blendFactor = 0.5;
            // Perform IK on MyAvatar, having our hand follow their controller.
            var myOtherXform = Xform.mul(otherXform, this.relXform);
            var myTargetXform = tweenXform(myXform, myOtherXform, blendFactor);
            this.pinJoint(this.primaryKey, this.primaryJointInfo, myTargetXform);
            // Perform local IK on the other avatar, making their hand follow our controller.
            var otherMyXform = Xform.mul(myXform, this.relXform.inv());
            var otherTargetXform = tweenXform(otherMyXform, otherXform, blendFactor);
            this.pinJoint(this.secondaryKey, this.secondaryJointInfo, otherTargetXform);
            this.updateStressHaptics();
            if (Vec3.distance(myXform.pos, myTargetXform.pos) > REJECT_DISTANCE) {
                this.transmitMessage(GrabMessageType.Reject);
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
            if (this.state === GrabLinkState.Alone) {
                // AJT_A: alone -> leader
                this.playClap();
                this.hapticPulse();
                this.disableControllerDispatcher();
                this.computeDeltaXform();
                this.transmitMessage(GrabMessageType.Grab);
            }
            else if (this.state === GrabLinkState.Peer) {
                // AJT_D: peer -> leader
                this.hapticPulse();
                this.clearPinOnJoint(this.primaryKey);
            }
            else {
                console.warn("AJT: WARNING: leaderEnter from unknown state " + this.state);
            }
        };
        GrabLink.prototype.leaderProcess = function () {
            this.updateJointInfo();
            var myXform = new Xform(this.primaryJointInfo.controllerRot, this.primaryJointInfo.controllerPos);
            var otherXform = new Xform(this.secondaryJointInfo.controllerRot, this.secondaryJointInfo.controllerPos);
            // AJT: TODO: dynamic change of blend factor based on distance?
            var blendFactor = 0;
            // Perform local IK on the other avatar, making their hand follow our controller.
            var otherMyXform = Xform.mul(myXform, this.relXform.inv());
            var otherTargetXform = tweenXform(otherMyXform, otherXform, blendFactor);
            this.pinJoint(this.secondaryKey, this.secondaryJointInfo, otherTargetXform);
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
            if (this.state === GrabLinkState.Follower) {
                // AJT_I: follower -> reject
                this.hapticPulse();
                this.clearPinOnJoint(this.primaryKey);
            }
            else if (this.state === GrabLinkState.Peer) {
                // AJT_J: peer -> reject
                this.hapticPulse();
                this.clearPinOnJoint(this.secondaryKey);
                this.clearPinOnJoint(this.primaryKey);
            }
            else if (this.state === GrabLinkState.Leader) {
                // AJT_K: leader -> reject
                this.hapticPulse();
                this.clearPinOnJoint(this.secondaryKey);
            }
            else {
                console.warn("AJT: WARNING: rejectEnter from unknown state " + this.state);
            }
        };
        GrabLink.prototype.rejectProcess = function () {
            // immediately go to the alone state
            this.changeState(GrabLinkState.Alone);
        };
        GrabLink.prototype.rejectExit = function () {
            // do nothing
        };
        // All GrabLink objects that affect myAvatar
        GrabLink.myAvatarGrabLinkMap = {};
        // All GrabLink objects that do not affect myAvatar
        GrabLink.otherAvatarGrabLinkMap = {};
        GrabLink.rightControllerDispatcherEnabled = true;
        GrabLink.leftControllerDispatcherEnabled = true;
        return GrabLink;
    }());
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
    var GrabbableJointScanner = /** @class */ (function () {
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
        GrabbableJointScanner.prototype.computeJointInfoMap = function (avatarId) {
            var avatar = avatarId ? AvatarManager.getAvatar(avatarId) : MyAvatar;
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
                    controllerRot: { x: 0, y: 0, z: 0, w: 1 },
                    controllerPos: { x: 0, y: 0, z: 0 },
                    controllerValid: false,
                    jointRot: { x: 0, y: 0, z: 0, w: 1 },
                    jointPos: { x: 0, y: 0, z: 0 },
                    jointValid: false,
                    palmPos: { x: 0, y: 0, z: 0 },
                    palmRot: { x: 0, y: 0, z: 0, w: 1 }
                };
                // transform joint into world space
                var localXform = new Xform(avatar.getAbsoluteJointRotationInObjectFrame(jointIndex), avatar.getAbsoluteJointTranslationInObjectFrame(jointIndex));
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
                    }
                    else {
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
                    var localControllerXform = new Xform(Mat4.extractRotation(mat), Mat4.extractTranslation(mat));
                    if (!isIdentity(localControllerXform.rot, localControllerXform.pos)) {
                        var worldControllerXform = Xform.mul(avatarXform, localControllerXform);
                        result.controllerRot = worldControllerXform.rot;
                        result.controllerPos = worldControllerXform.pos;
                        result.controllerValid = true;
                    }
                    else {
                        result.controllerRot = result.jointRot;
                        result.controllerPos = result.jointPos;
                        result.controllerValid = false;
                    }
                }
                else {
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
                }
                else {
                    if (self.addAvatarCb) {
                        self.addAvatarCb(id, avatarMap[id]);
                    }
                }
            });
            this.prevAvatarMap = avatarMap;
        };
        // searches the most recently scanned jointInfoMap for a grabbable joint on other avatar.
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
            }
            else {
                return { avatarId: closestId, jointName: closestJointName };
            }
        };
        GrabbableJointScanner.prototype.getJointInfo = function (key) {
            if (key) {
                return this.prevAvatarMap[key.avatarId].jointInfoMap[key.jointName];
            }
        };
        GrabbableJointScanner.prototype.getMyHand = function (hand) {
            if (hand === RIGHT_HAND) {
                return this.getJointInfo({ avatarId: MyAvatar.SELF_ID, jointName: "RightHand" });
            }
            else {
                return this.getJointInfo({ avatarId: MyAvatar.SELF_ID, jointName: "LeftHand" });
            }
        };
        return GrabbableJointScanner;
    }());
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
        var primaryKey = { avatarId: MyAvatar.SELF_ID, jointName: "LeftHand" };
        var link;
        if (value === 1) {
            var secondaryKey = scanner.findGrabbableJoint(LEFT_HAND, GRAB_DISTANCE);
            if (secondaryKey) {
                link = GrabLink.findOrCreateLink(primaryKey, secondaryKey);
                link.triggerPress();
                leftHandActiveKeys = { myKey: primaryKey, otherKey: secondaryKey };
            }
        }
        else if (leftHandActiveKeys) {
            link = GrabLink.findLink(leftHandActiveKeys.myKey, leftHandActiveKeys.otherKey);
            if (link) {
                link.triggerRelease();
            }
            else {
                console.warn("AJT: WARNING, leftTrigger(), could not find gripLink for LeftHand");
            }
            leftHandActiveKeys = undefined;
        }
    }
    var rightHandActiveKeys;
    // Controller system callback on rightTrigger pull or release.
    function rightTrigger(value) {
        var primaryKey = { avatarId: MyAvatar.SELF_ID, jointName: "RightHand" };
        var link;
        if (value === 1) {
            var secondaryKey = scanner.findGrabbableJoint(RIGHT_HAND, GRAB_DISTANCE);
            if (secondaryKey) {
                link = GrabLink.findOrCreateLink(primaryKey, secondaryKey);
                link.triggerPress();
                rightHandActiveKeys = { myKey: primaryKey, otherKey: secondaryKey };
            }
        }
        else if (rightHandActiveKeys) {
            link = GrabLink.findLink(rightHandActiveKeys.myKey, rightHandActiveKeys.otherKey);
            if (link) {
                link.triggerRelease();
            }
            else {
                console.warn("AJT: WARNING, rightTrigger(), could not find gripLink for RightHand");
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
    // message is a JSON serialized GrabMessage
    function messageHandler(channel, message, sender) {
        if (channel === "Hifi-Handshake" && sender !== MyAvatar.sessionUUID) {
            console.warn("AJT: messageHandler, msg = " + message);
            var obj;
            try {
                obj = JSON.parse(message);
            }
            catch (e) {
                console.warn("AJT: messageHander, error parsing msg = " + message);
                return;
            }
            var link;
            var primaryKey, secondaryKey;
            var relXform;
            if (obj.receiver === MyAvatar.sessionUUID) {
                primaryKey = { avatarId: MyAvatar.SELF_ID, jointName: obj.grabbedJoint };
                secondaryKey = { avatarId: sender, jointName: obj.grabbingJoint };
                if (obj.type === GrabMessageType.Grab) {
                    link = GrabLink.findOrCreateLink(primaryKey, secondaryKey);
                    relXform = new Xform(obj.relXform.rot, obj.relXform.pos);
                    link.receivedGrab(relXform.inv());
                }
                else if (obj.type === GrabMessageType.Release) {
                    link = GrabLink.findLink(primaryKey, secondaryKey);
                    if (link) {
                        link.receivedRelease();
                    }
                    else {
                        console.warn("AJT: WARNING, messageHandler() release, could not find gripLink for " + obj.grabbingJoint);
                    }
                }
                else if (obj.type === GrabMessageType.Reject) {
                    link = GrabLink.findLink(primaryKey, secondaryKey);
                    if (link) {
                        link.reject();
                    }
                    else {
                        console.warn("AJT: WARNING, messageHandler() reject, could not find gripLink for " + obj.grabbedJoint);
                    }
                }
            }
            else {
                // Two other avatars are grabbing each other.
                primaryKey = { avatarId: sender, jointName: obj.grabbingJoint };
                secondaryKey = { avatarId: obj.receiver, jointName: obj.grabbedJoint };
                // if this message is not from the initiator, swap the primary and secondary keys.
                if (!obj.initiator) {
                    var tempKey = primaryKey;
                    primaryKey = secondaryKey;
                    secondaryKey = primaryKey;
                }
                console.warn("AJT: other avatars, primaryKey = " + JSON.stringify(primaryKey) + ", secondaryKey = " + JSON.stringify(secondaryKey));
                if (obj.initiator) {
                    if (obj.type === GrabMessageType.Grab) {
                        link = GrabLink.findOrCreateOtherAvatarLink(primaryKey, secondaryKey);
                        relXform = new Xform(obj.relXform.rot, obj.relXform.pos);
                        link.triggerPress(relXform);
                    }
                    else if (obj.type === GrabMessageType.Release) {
                        link = GrabLink.findOtherAvatarLink(primaryKey, secondaryKey);
                        if (link) {
                            link.triggerRelease();
                        }
                    }
                    else if (obj.type === GrabMessageType.Reject) {
                        link = GrabLink.findOtherAvatarLink(primaryKey, secondaryKey);
                        if (link) {
                            link.reject();
                        }
                    }
                }
                else {
                    if (obj.type === GrabMessageType.Grab) {
                        link = GrabLink.findOtherAvatarLink(primaryKey, secondaryKey);
                        if (link) {
                            relXform = new Xform(obj.relXform.rot, obj.relXform.pos);
                            link.receivedGrab(relXform.inv());
                        }
                    }
                    else if (obj.type === GrabMessageType.Release) {
                        link = GrabLink.findOtherAvatarLink(primaryKey, secondaryKey);
                        if (link) {
                            link.receivedRelease();
                        }
                    }
                    else if (obj.type === GrabMessageType.Reject) {
                        link = GrabLink.findOtherAvatarLink(primaryKey, secondaryKey);
                        if (link) {
                            link.reject();
                        }
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
            }
            else {
                result.rightHandType = 3; // HipsRelativeRotationAndPosition
            }
            if (leftHandPose.valid) {
                result.leftHandType = 0; // RotationAndPosition
            }
            else {
                result.leftHandType = 3; // HipsRelativeRotationAndPosition
            }
            var avatarXform = new Xform(Quat.multiply(MyAvatar.orientation, QUAT_Y_180), MyAvatar.position);
            if (myAvatarRightHandXform) {
                var rightHandTargetXform = Xform.mul(avatarXform.inv(), myAvatarRightHandXform);
                result.rightHandRotation = rightHandTargetXform.rot;
                result.rightHandPosition = rightHandTargetXform.pos;
            }
            else {
                delete result.rightHandRotation;
                delete result.rightHandPosition;
            }
            if (myAvatarLeftHandXform) {
                var leftHandTargetXform = Xform.mul(avatarXform.inv(), myAvatarLeftHandXform);
                result.leftHandRotation = leftHandTargetXform.rot;
                result.leftHandPosition = leftHandTargetXform.pos;
            }
            else {
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
        console.warn("addAvatar(" + id + ")");
        console.warn("typeof id = " + (typeof id));
        console.warn("JSON.stringify(id) = " + JSON.stringify(id));
        if (id === "null") {
            id = MyAvatar.SELF_ID;
        }
        console.warn("addAvatar(" + id + ")");
        updateAvatar(id, data);
    }
    function updateAvatar(id, data) {
    }
    function removeAvatar(id) {
        console.warn("removeAvatar(" + id + ")");
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
