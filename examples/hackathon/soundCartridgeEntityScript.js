
(function() {
    Script.include("../libraries/utils.js");
    var _this;
    SoundCartridge = function() {
        _this = this;
     
    };

    SoundCartridge.prototype = {

        playSound: function() {
            var position = Entities.getEntityProperties(_this.entityID, "position").position;
            _this.injector = Audio.playSound(_this.clip, {position: position});
        }, 

        preload: function(entityID) {
            this.entityID = entityID;
            print("PRELOAD ENTITY SCRIPT!!!")
            var userData = getEntityUserData(_this.entityID);
            if (!userData.soundURL) {
                print("WARNING: NO SOUND URL ON USER DATA!!!!!");
            }
            _this.clip = SoundCache.getSound(userData.soundURL);
        },

        unload: function() {
            if (_this.injector) {
                print("EBL STOP INJECTOR");
                _this.injector.stop();
            }
        }

   
    };

    // entity scripts always need to return a newly constructed object of our type
    return new SoundCartridge();
});