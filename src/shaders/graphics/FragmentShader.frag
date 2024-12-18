layout(set = 0, binding = 1) uniform samplerCube skyboxSampler;

in vec3 fragTexCoords;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(skyboxSampler, fragTexCoords);
}