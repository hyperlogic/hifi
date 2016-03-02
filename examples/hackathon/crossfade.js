Script.include("../libraries/utils.js");
var UPDATE_TIME = 50;

var orientation = Camera.getOrientation();
orientation = Quat.safeEulerAngles(orientation);
orientation.x = 0;
orientation = Quat.fromVec3Degrees(orientation);
var center = Vec3.sum(MyAvatar.position, Vec3.multiply(3, Quat.getFront(orientation)));

var currentVolume = 1.0;

var clip = SoundCache.getSound("https://s3-us-west-1.amazonaws.com/hifi-content/eric/Sounds/gregorian.wav");

var injector = Audio.playSound(clip, {
    position: center,
    volume: currentVolume
});



function update() {
    // We want to get realtive y distance of avatar's hand from avatar's y position
    var relativeHandYPosition =  MyAvatar.getRightPalmPosition().y - MyAvatar.position.y; 
    var newVolume = map(relativeHandYPosition, 1.1, -0.1, 1, 0);
    newVolume = clamp(newVolume, 0, 1);
    print(newVolume)
    injector.setOptions({position: center, volume: newVolume});
   
}


function cleanup() {
    injector.stop();
}


Script.setInterval(update, UPDATE_TIME)
Script.scriptEnding.connect(cleanup);