(function() {
    Script.include("../libraries/utils.js");
    var _this;
    SoundCartridge = function() {
        _this = this;
        _this.audioOptions = {
            loop: true
        };
        _this.playingColor = {
            red: 0,
            green: 200,
            blue: 10
        };
        _this.notPlayingColor = {
            red: 200,
            green: 0,
            blue: 0
        };

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

            Entities.editEntity(_this.entityID, {
                color: _this.playingColor
            });
        },

        stopSound: function() {
            print("EBL STOP SOUND!!!!!!!!!!!!!!")
            if (_this.injector) {
                _this.injector.stop();
                Entities.editEntity(_this.entityID, {
                    color: _this.notPlayingColor
                });
            }
        },

        setSoundVolume: function(entityID, data) {
            if (!_this.injector) {
                print ("NO INJECTOR!");
                return;
            }
            var newVolume = JSON.parse(data[0]).newVolume;
            _this.audioOptions.volume = newVolume;
            _this.injector.setOptions(_this.audioOptions);
        },

        updateSoundPosition: function() {
            if (_this.injector) {  
              _this.position = Entities.getEntityProperties(_this.entityID, "position").position;
              _this.audioOptions.position = _this.position;
              _this.injector.setOptions(_this.audioOptions);
            }

        },

        releaseGrab: function() {
            _this.updateSoundPosition();

        },

        releaseEquip: function() {
                _this.updateSoundPosition();
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