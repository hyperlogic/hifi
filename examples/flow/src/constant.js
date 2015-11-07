// ctor
var Constant = function (value) {
    this.value = value;

    // TODO: abstract this into base prototype.
    this.outputs = {};
    this.outputs.value = [];
};

Constant.prototype.start = function () {
    // TODO: abstract this into emit prototype
    var valueObservers = this.outputs.value;
    var i, l = valueObservers.length;
    for (i = 0; i < l; i++) {
        valueObservers[i](this.value);
    }
};

// exports
exports.Constant = Constant;
