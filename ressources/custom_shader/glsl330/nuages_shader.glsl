// Fragment shader (fs.glsl)
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;      // Texture principale
uniform sampler2D noiseTexture;  // Texture de bruit de Perlin
uniform vec4 colDiffuse;
uniform float cloudDensity = 0.5;  // Contrôle la densité des nuages (0.0 - 1.0)
uniform float cloudSharpness = 3.0; // Contrôle la netteté des bords des nuages
uniform float timeValue;          // Pour l'animation
uniform float noiseScale = 5.0;    // Nouvelle variable pour contrôler l'échelle du bruit (valeurs plus élevées = variations plus petites)

void main() {
    // Texture de base (la sphère)
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Utiliser les coordonnées 3D pour simuler un nuage volumétrique
    // Nous créons une illusion de 3D en projetant sur la sphère
    float sphereRadius = 0.5;
    vec2 centeredCoord = fragTexCoord * 2.0 - 1.0;
    float distFromCenter = length(centeredCoord);
    
    // Ne traiter que les pixels à l'intérieur de la sphère
    if (distFromCenter <= 1.0) {
        // Coordonnées décalées pour animation
        vec2 shiftedCoord = fragTexCoord + vec2(timeValue * 0.02, timeValue * 0.01);
        
        // Coordonnées pour le bruit, avec une composante pseudo-3D
        // pour simuler un aspect volumétrique
        vec2 noiseCoord1 = shiftedCoord;
        vec2 noiseCoord2 = shiftedCoord * 1.5 + vec2(0.2, 0.3);
        vec2 noiseCoord3 = shiftedCoord * 2.5 + vec2(0.1, -0.2);
        
        // Calculer la profondeur basée sur la distance au centre
        float depth = sqrt(1.0 - distFromCenter*distFromCenter) * 0.5;
        
        // Échantillonnage multi-couches pour un effet plus réaliste
        float noise1 = texture(noiseTexture, noiseCoord1).r * 0.6;
        float noise2 = texture(noiseTexture, noiseCoord2).r * 0.3;
        float noise3 = texture(noiseTexture, noiseCoord3).r * 0.1;
        
        // Combinaison des couches de bruit
        float noiseValue = (noise1 + noise2 + noise3) * (1.0 + depth);
        
        // Application de la densité et du contraste
        float threshold = 1.0 - cloudDensity;
        float detail = 1.0/cloudSharpness;
        float cloudShape = smoothstep(threshold - detail, threshold + detail, noiseValue);
        
        // Variation d'opacité basée sur la distance du centre pour un effet sphérique
        float sphereEdge = 1.0 - smoothstep(0.85, 1.0, distFromCenter);
        
        // Couleur de base du nuage (blanc)
        vec3 cloudColor = vec3(1.0);
        
        // Ombrage pour donner du volume
        float shading = mix(0.7, 1.0, noiseValue + depth * 0.3);
        cloudColor *= shading;
        
        // Opacité finale
        float opacity = cloudShape * sphereEdge * colDiffuse.a;
        
        // Mélanger la couleur du nuage avec la texture de base
        finalColor = vec4(mix(texelColor.rgb, cloudColor, opacity), max(texelColor.a, opacity));
    } else {
        // En dehors de la sphère, utiliser la couleur de base
        finalColor = texelColor * colDiffuse;
    }
}