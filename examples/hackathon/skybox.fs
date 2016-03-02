
#line 5
const float PI = 3.14159;
uniform float red = 0.5;
const float axialTilt = 0.408407;
const vec2 atsc = vec2(sin(axialTilt), cos(axialTilt));
const mat3 axialTiltMatrix = mat3(
        vec3(atsc.y, -atsc.x, 0.0),
        vec3(atsc.x, atsc.y, 0.0),
        vec3(0.0, 0.0, 1.0));


vec2 directionToSpherical(in vec3 d) {
    return vec2(atan(d.x, -d.z), asin(d.y)) * -1.0f;
}

vec2 directionToUv(in vec3 d) {
    vec2 uv = directionToSpherical(d);
    uv /= PI;
    uv.x /= 2.0;
    uv += 0.5;
    return uv;
}
vec3 stars3(in vec3 d) {
    //return rd;
    vec3 dir = d * axialTiltMatrix;
    
    float theta =   0;
    vec2 sc = vec2(sin(theta), cos(theta));
    dir = dir * mat3( vec3(sc.y, 0.0, sc.x),
                    vec3(0.0, 1.0, 0.0),
                    vec3(-sc.x, 0.0, sc.y));
    
    vec2 uv = directionToUv(dir);
    vec3 starColor = texture2D( iChannel0, uv).xyz;
    starColor = pow(starColor, vec3(0.75));
    return starColor;
}

vec3 render( in vec3 ro, in vec3 rd ) {
    vec3 vStarDir = rd;
    return stars3(vStarDir);
}
vec3 getSkyboxColor() {
       return render( vec3(0.0), normalize(_normal) ).rgb;
}
