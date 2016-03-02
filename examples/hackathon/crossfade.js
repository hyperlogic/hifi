Script.include("../libraries/utils.js");
var UPDATE_TIME = 20;


var currentVolume = 0.0;

var clip1 = SoundCache.getSound("https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/synth/chillstep-synth.wav");
var clip2 = SoundCache.getSound("https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/hackathonSounds/drops/drop1.wav");

var injector1, injector2;
var AVATAR_POSITION_Y_OFFSET = 0.2;

var clipPosition = MyAvatar.getHeadPosition();

Script.setTimeout(initAudioClips, 1000);

var handHeightRange = {
    min: 0,
    max: 1
};

var debugBox = Entities.addEntity({
    type: "Box",
    position: Vec3.sum(MyAvatar.position, {x: 0, y: AVATAR_POSITION_Y_OFFSET, z: 0}),
    dimensions: {
        x: 0.1,
        y: 0.1,
        z: 0.1
    },
    color: {
        red: 200,
        green: 10,
        blue: 200
    }
});

function initAudioClips() {
    injector1 = Audio.playSound(clip1, {
        position: clipPosition,
        volume: currentVolume,
        loop: true
    });

    injector2 = Audio.playSound(clip2, {
        position: clipPosition,
        volume: currentVolume,
        loop: true
    });

    Script.setInterval(update, UPDATE_TIME)
}



function update() {
    updateAudio();

}

function updateAudio() {
    // We want to get realtime y distance of avatar's hand from avatar's y position
    var startingHeight = MyAvatar.position.y + AVATAR_POSITION_Y_OFFSET;
    var rightPalmYPosition = MyAvatar.getRightPalmPosition().y
    if (rightPalmYPosition <  startingHeight) {
        newVolume = 0;
    } else {
        var relativeRightHandYPosition = rightPalmYPosition - startingHeight;
        var newVolume = map(relativeRightHandYPosition, handHeightRange.max, handHeightRange.min, 1, 0);
        newVolume = clamp(newVolume, 0, 1);

    }
    injector1.setOptions({
        position: clipPosition,
        volume: newVolume,
        loop: true
    });

    var leftPalmYPosition = MyAvatar.getLeftPalmPosition().y
    if (leftPalmYPosition < startingHeight) {
        newVolume = 0;
    } else {
        var relativeLeftHandYPosition = leftPalmYPosition - startingHeight;
        var newVolume = map(relativeLeftHandYPosition, handHeightRange.max, handHeightRange.min, 1, 0);
        newVolume = clamp(newVolume, 0, 1);

    }
    injector2.setOptions({
        position: clipPosition,
        volume: newVolume,
        loop: true
    });

    print(newVolume)


}


function cleanup() {
    injector1.stop();
    injector2.stop();
    Entities.deleteEntity(debugBox);
}


Script.scriptEnding.connect(cleanup);