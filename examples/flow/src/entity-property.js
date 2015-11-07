// ctor
var EntityProperty = function (id) {
    this.id = id;
    this.dirty = 0;
    var self = this;

    // inputs
    this.inputs = {};
    this.inputs.position = function (position) {
        self.position = position;
        self.dirty |= 0x01;
    };
    this.inputs.orientation = function (orientation) {
        self.orientation = orientation;
        self.dirty |= 0x02;
    };
    this.inputs.color = function (color) {
        self.color = color;
        self.dirty |= 0x04;
    };
    this.inputs.trigger$ = function () {
        var props = {};
        if (self.dirty & 0x01) {
            props.position = self.position;
        } else if (self.dirty & 0x02) {
            props.orientation = self.orientation;
        } else if (self.dirty & 0x04) {
            props.color = self.color;
        }
        Entities.editEntity(self.id, props);
        self.dirty = 0;
    };
};

EntityProperty.prototype.start = function () {
    // no-op
};

// exports
exports.EntityProperty = EntityProperty;
