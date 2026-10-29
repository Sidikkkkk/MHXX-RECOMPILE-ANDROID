const float BayerMatrix[] = float[](
     0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
    48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
    12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
    60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
     3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
    51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
    15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
    63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
);
float dither(vec2 coord) {
    ivec2 pos = ivec2(mod(coord, 8.0));
    return BayerMatrix[pos.y * 8 + pos.x] / 64.0;
}
float insideBox(vec2 v, vec2 bottomLeft, vec2 topRight) {
    vec2 s = step(bottomLeft, v) - step(topRight, v);
    return s.x * s.y;
}
void main()
{
    vec2 bottomLeft = screen_coord.xy;
    vec2 topRight = screen_coord.zw;
    float flag = insideBox(gl_FragCoord.xy, bottomLeft, topRight);
    vec2 center = (bottomLeft + topRight) / 2.0;
    float radius = length(bottomLeft - center);
    float distance = length(gl_FragCoord.xy - center);
    float value = distance / radius;
    float threshold = flag * dither(gl_FragCoord.xy);
    if(value < threshold) {
        discard;
    }
    SetOutput(Sample());
}
