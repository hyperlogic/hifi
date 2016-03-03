
(function() {
    Script.include("../libraries/utils.js");
    var _this;
    SoundCartridge = function() {
        _this = this;
        _this.audioOptions = {loop: true};
     
    };

    SoundCartridge.prototype = {

        playSound: function() {
            var position = Entities.getEntityProperties(_this.entityID, "position").position;
            if (!_this.injector) {
                _this.audioOptions.position = position;
                _this.injector = Audio.playSound(_this.clip, _this.audioOptions);  
            } else {
                //We already have injector so just restart
                _this.injector.restart();
            }
        }, 

        stopSound: function() {
            print("STOP SOUND!!!!!!!!!!!!!!")
            if (_this.injector) {
                _this.injector.stop();
            }
        },

        preload: function(entityID) {
            this.entityID = entityID;
            var userData = getEntityUserData(_this.entityID);
            if (!userData.soundURL) {
                print("WARNING: NO SOUND URL ON USER DATA!!!!!");
            }
            _this.clip = SoundCache.getSound(userData.soundURL);
        },

        unload: function() {
            if (_this.injector) {
                _this.injector.stop();
                delete _this.injector;
            }
        }

   
    };

    // entity scripts always need to return a newly constructed object of our type
    return new SoundCartridge();
});