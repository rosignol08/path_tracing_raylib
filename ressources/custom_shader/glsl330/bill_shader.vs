#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;

uniform mat4 mvp;        // Matrice Model-View-Projection
uniform vec3 lightDir;   // Direction de la lumière (normalisée)

out vec2 fragTexCoord;
out float lightIntensity;

void main()
{
    fragTexCoord = vertexTexCoord;

    // Simuler une normale perpendiculaire au billboard
    vec3 normal = vec3(0.0, 1.0, 0.0);  // On suppose que les billboards sont orientés verticalement
    
    // Calcul de l'éclairage avec un modèle Lambert simple
    lightIntensity = max(dot(normal, normalize(-lightDir)), 0.2);  // Éviter les ombres trop noires

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
