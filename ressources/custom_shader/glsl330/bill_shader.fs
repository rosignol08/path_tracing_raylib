#version 330

in vec2 fragTexCoord;
in float lightIntensity;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec3 lightColor;

void main()
{
    vec4 texColor = texture(texture0, fragTexCoord);
    
    // Appliquer l'intensité de la lumière avec la couleur de la lumière
    finalColor = vec4(texColor.rgb * lightColor * lightIntensity, texColor.a);
}
