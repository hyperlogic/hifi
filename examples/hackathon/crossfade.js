Script.include("../libraries/utils.js");
var UPDATE_TIME = 50;

var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);
var center = Vec3.sum(MyAvatar.position, Vec3.multiply(3, Quat.getFront(orientation)));

var currentVolume = 1.0;

var clip1 = SoundCache.getSound("https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/gregorian.wav");
var clip2 = SoundCache.getSound("https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/alan.wav");

var injector1, injector2;
var updateInterval;


Script.setTimeout(initAudioClips, 5000);

function initAudioClips() {
    injector1 = Audio.playSound(clip1, {
        position: center,
        volume: currentVolume
    });

    injector2 = Audio.playSound(clip2, {
        position: center,
        volume: currentVolume
    });
    
    updateInterval = Script.setInterval(update, UPDATE_TIME)
}



function update() {
    print("INTERVAL")
    // We want to get realtive y distance of avatar's hand from avatar's y position
    var relativeRightHandYPosition =  MyAvatar.getRightPalmPosition().y - MyAvatar.position.y; 
    var newVolume = map(relativeRightHandYPosition, 1.1, -0.1, 1, 0);
    newVolume = clamp(newVolume, 0, 1);
    injector1.setOptions({position: center, volume: newVolume});

    var relativeLeftHandYPosition =  MyAvatar.getLeftPalmPosition().y - MyAvatar.position.y; 
    newVolume = map(relativeLeftHandYPosition, 1.1, -0.1, 1, 0);
    newVolume = clamp(newVolume, 0, 1);
    injector2.setOptions({position: center, volume: newVolume});


   
}


function cleanup() {
    injector1.stop();
    injector2.stop();
}


Script.scriptEnding.connect(cleanup);