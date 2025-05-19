#define MAX_SPHERES 8
#define MAX_BOUNCES 10  // Augmenté pour plus de réalisme
#define MAX_SAMPLES 8  // Anti-aliasing
#define PI 3.14159265

// Structures de matériaux
#define MAT_DIFFUSE 0
#define MAT_METALLIC 1
#define MAT_GLASS 2
#define MAT_EMISSIVE 3


// Structure pour les matériaux, alignée sur vec4 pour la compatibilité avec des uniformes
struct Material {
    int type;       // Type de matériau
    float roughness; // Rugosité (métal, verre)
    float ior;      // Indice de réfraction (verre)
    float padding;  // Padding pour l'alignement
    vec3 albedo;    // Couleur de base
    float padding2; // Padding supplémentaire
};

uniform vec4 spheres[MAX_SPHERES];     // xyz = position, w = rayon
uniform Material materials[MAX_SPHERES]; // Matériaux des sphères
uniform int sphereCount;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform vec2 resolution;
uniform vec3 viewEye;
uniform vec3 viewCenter;
uniform float time;     // Pour le bruit

uniform sampler2D previousFrame;
uniform float frameBlend; // 0.1 to 0.2 works well


out vec4 finalColor;

// Hash function pour générer des nombres pseudo-aléatoires
uint hash(uint x) {
    x = x * 1664525u + 1013904223u;
    x ^= x >> 16u;
    x *= 0x3dba2d8du;
    x ^= x >> 16u;
    return x;
}

float random(vec3 pos, float seed) {
    uint h = hash(uint(pos.x * 8192.0) ^ hash(uint(pos.y * 8192.0) ^ hash(uint(pos.z * 8192.0) ^ hash(uint(seed * 91.237)))));
    return float(h) / 4294967296.0;
}

vec2 randomVec2(vec3 pos, float seed) {
    return vec2(
        random(pos, seed),
        random(pos, seed + 1.618)
    );
}

// Échantillonnage cosinus pondéré pour une meilleure distribution
vec3 sampleHemisphere(vec3 normal, vec3 pos, float seed) {
    vec2 rand = randomVec2(pos, seed);
    
    float phi = 2.0 * PI * rand.x;
    float cosTheta = sqrt(rand.y);  // Distribution en cosinus
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 dir = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    
    // Créer une base orthonormée alignée avec la normale
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);
    
    return normalize(tbn * dir);
}

// Réflexion spéculaire avec perturbation pour rugosité
vec3 reflect(vec3 incident, vec3 normal, float roughness, vec3 pos, float seed) {
    vec3 reflected = reflect(incident, normal);
    
    if (roughness > 0.0) {
        vec2 rand = randomVec2(pos, seed);
        float phi = 2.0 * PI * rand.x;
        float cosTheta = pow(1.0 - rand.y * roughness * roughness, 1.0 / 3.0);
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        
        vec3 scatter = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        
        vec3 up = abs(reflected.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, reflected));
        vec3 bitangent = cross(reflected, tangent);
        mat3 tbn = mat3(tangent, bitangent, reflected);
        
        return normalize(tbn * scatter);
    }
    
    return reflected;
}

// Réfraction avec loi de Fresnel et perturbation pour rugosité
vec3 refract(vec3 incident, vec3 normal, float ior, float roughness, vec3 pos, float seed, out float reflectionChance) {
    float eta = dot(incident, normal) < 0.0 ? 1.0 / ior : ior;
    vec3 n = dot(incident, normal) < 0.0 ? normal : -normal;
    
    float cosI = abs(dot(incident, n));
    float sinT2 = eta * eta * (1.0 - cosI * cosI);
    
    // Réflexion totale interne
    if (sinT2 > 1.0) {
        reflectionChance = 1.0;
        return reflect(incident, n, roughness, pos, seed);
    }
    
    float cosT = sqrt(1.0 - sinT2);
    
    // Approximation de Schlick pour Fresnel
    float r0 = ((1.0 - eta) / (1.0 + eta)) * ((1.0 - eta) / (1.0 + eta));
    float fresnel = r0 + (1.0 - r0) * pow(1.0 - cosI, 5.0);
    
    reflectionChance = fresnel;
    
    if (random(pos, seed + 4.269) < fresnel) {
        return reflect(incident, n, roughness, pos, seed);
    }
    
    vec3 refracted = normalize(eta * incident + (eta * cosI - cosT) * n);
    
    if (roughness > 0.0) {
        vec2 rand = randomVec2(pos, seed + 2.718);
        float phi = 2.0 * PI * rand.x;
        float cosTheta = pow(1.0 - rand.y * roughness * roughness, 1.0 / 2.0);
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        
        vec3 scatter = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        
        vec3 up = abs(refracted.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, refracted));
        vec3 bitangent = cross(refracted, tangent);
        mat3 tbn = mat3(tangent, bitangent, refracted);
        
        return normalize(tbn * scatter);
    }
    
    return refracted;
}

bool intersectSphere(vec3 ro, vec3 rd, vec4 sphere, out float t, out vec3 n) {
    vec3 oc = ro - sphere.xyz;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - sphere.w * sphere.w;
    float h = b*b - c;
    
    if (h < 0.0) return false;
    
    h = sqrt(h);
    t = -b - h;
    
    if (t < 0.001) t = -b + h;
    if (t < 0.001) return false;
    
    vec3 hit = ro + t * rd;
    n = normalize(hit - sphere.xyz);
    
    return true;
}

// Fonction auxiliaire pour calculer l'éclairage direct
vec3 directLight(vec3 p, vec3 n, vec3 viewDir, int matType, vec3 albedo, float roughness, float dist) {
    vec3 toLight = normalize(lightPos - p);
    float distToLight = length(lightPos - p);
    
    // Vérifier si le point est dans l'ombre
    bool occluded = false;
    for (int i = 0; i < sphereCount; ++i) {
        float t;
        vec3 tmp;
        if (intersectSphere(p + n * 0.001, toLight, spheres[i], t, tmp)) {
            if (t < distToLight) {
                occluded = true;
                break;
            }
        }
    }
    
    if (occluded) return vec3(0.0);
    
    // Atténuation de la lumière
    float attenuation = lightIntensity / (1.0 + 0.1 * distToLight + 0.01 * distToLight * distToLight); //bloque la lumière à 2
    
    // Différents modèles d'éclairage en fonction du matériau
    vec3 lightContrib = vec3(0.0);
    
    if (matType == MAT_DIFFUSE) {
        // Modèle d'éclairage Lambert pour surfaces diffuses
        float diff = max(dot(n, toLight), 0.0);
        lightContrib = albedo * lightColor * diff * attenuation;
        
        // Ajout d'une légère composante ambiante
        lightContrib += albedo * lightColor * 0.05;
    }
    else if (matType == MAT_METALLIC) {
        // Modèle d'éclairage Phong/Blinn-Phong pour surfaces métalliques
        float diff = max(dot(n, toLight), 0.0);
        vec3 halfwayDir = normalize(toLight + viewDir);
        float spec = pow(max(dot(n, halfwayDir), 0.0), (1.0 - roughness) * 128.0 + 1.0);
        
        lightContrib = albedo * lightColor * diff * attenuation;
        lightContrib += albedo * lightColor * spec * (1.0 - roughness) * attenuation;
    }
    else if (matType == MAT_GLASS) {
        // Pour le verre on inclut principalement la composante spéculaire
        vec3 reflectDir = reflect(-toLight, n);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), (1.0 - roughness) * 128.0 + 1.0);
        
        lightContrib = lightColor * spec * (1.0 - roughness) * attenuation;
    }
    
    return lightContrib;
}

//fonction d'échantillonnage direct de la lumière
vec3 sampleDirectLight(vec3 p, vec3 n, vec3 viewDir, Material mat, float seed) {
    // Éviter l'auto-intersection avec un petit décalage
    vec3 origin = p + n * 0.001;
    vec3 contrib = vec3(0.0);
    
    // Trouver les sources de lumière émissives (sphères)
    for (int i = 0; i < sphereCount; ++i) {
        if (materials[i].type == MAT_EMISSIVE) {
            // Échantillonnage de la sphère lumineuse
            vec3 lightCenter = spheres[i].xyz;
            float lightRadius = spheres[i].w;
            float distToLight = length(lightCenter - p);
            
            // Génération d'un point aléatoire sur la sphère lumineuse
            vec2 rand = randomVec2(p, seed + float(i) * 0.773);
            float phi = 2.0 * PI * rand.x;
            float cosTheta = 2.0 * rand.y - 1.0;
            float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
            
            vec3 sampleOffset = lightRadius * vec3(
                cos(phi) * sinTheta,
                sin(phi) * sinTheta,
                cosTheta
            );
            
            vec3 lightPos = lightCenter + sampleOffset;
            vec3 toLight = normalize(lightPos - p);
            
            // Vérifier la visibilité (ombres)
            bool occluded = false;
            for (int j = 0; j < sphereCount; ++j) {
                if (j == i) continue; // Ignorer la source
                float t;
                vec3 tmp;
                if (intersectSphere(origin, toLight, spheres[j], t, tmp)) {
                    if (t < distToLight) {
                        occluded = true;
                        break;
                    }
                }
            }
            
            if (!occluded) {
                // Calculer la contribution de cette lumière
                float solidAngle = 2.0 * PI * (1.0 - sqrt(1.0 - (lightRadius*lightRadius)/(distToLight*distToLight)));
                float cosLight = max(0.0, dot(n, toLight));
                
                // BRDF selon le matériau
                vec3 brdf = vec3(0.0);
                if (mat.type == MAT_DIFFUSE) {
                    brdf = mat.albedo / PI; // Lambert
                } 
                else if (mat.type == MAT_METALLIC) {
                    vec3 halfwayDir = normalize(toLight + viewDir);
                    float spec = pow(max(dot(n, halfwayDir), 0.0), (1.0 - mat.roughness) * 128.0 + 1.0);
                    brdf = (mat.albedo + spec * (1.0 - mat.roughness)) / PI;
                }
                else if (mat.type == MAT_GLASS) {
                    // Approximation simple pour le verre
                    vec3 reflectDir = reflect(-toLight, n);
                    float spec = pow(max(dot(viewDir, reflectDir), 0.0), (1.0 - mat.roughness) * 128.0 + 1.0);
                    brdf = vec3(spec * (1.0 - mat.roughness)) / PI;
                }
                
                // Ajout de la contribution lumineuse
                contrib += brdf * materials[i].albedo * cosLight * solidAngle * lightIntensity;
            }
        }
    }
    
    return contrib;
}

vec3 trace(vec3 ro, vec3 rd, float seed) {
    vec3 col = vec3(0.0);
    vec3 throughput = vec3(1.0);

    for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
        float minT = 1e9;
        int hitIdx = -1;
        vec3 n, hit;
        
        // Trouver l'intersection la plus proche
        for (int i = 0; i < sphereCount; ++i) {
            float t;
            vec3 ni;
            if (intersectSphere(ro, rd, spheres[i], t, ni)) {
                if (t < minT) {
                    minT = t;
                    hit = ro + rd * t;
                    n = ni;
                    hitIdx = i;
                }
            }
        }

        // Si pas d'intersection, ajouter un fond dégradé et sortir
        if (hitIdx == -1) {
            // Ciel dégradé simple
            float t = 0.5 * (rd.y + 1.0);
            vec3 skyColor = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            col += throughput * skyColor * 0.3;
            break;
        }

        // Après avoir trouvé l'intersection:
        if (hitIdx != -1) {
            Material mat = materials[hitIdx];
            
            // Si on touche une source émissive, ajouter sa contribution et terminer
            if (mat.type == MAT_EMISSIVE) {
                col += throughput * mat.albedo * lightIntensity;
                break;
            }
            
            // Ajout de l'échantillonnage direct de la lumière (NEE)
            vec3 directLight = sampleDirectLight(hit, n, -rd, mat, seed + float(bounce) * 1.618);
            col += throughput * directLight;
        
        //// Récupérer les propriétés du matériau
        //Material mat = materials[hitIdx];
        
        // Calculer l'éclairage direct
        //vec3 direct = directLight(hit, n, -rd, mat.type, mat.albedo, mat.roughness, minT);
        //col += throughput * direct;
        
        // Calculer le prochain rayon en fonction du matériau
        if (mat.type == MAT_DIFFUSE) {
            // Surface diffuse: échantillonnage de l'hémisphère
            rd = sampleHemisphere(n, hit, seed + float(bounce) * 3.14159);
            ro = hit + n * 0.001;
            throughput *= mat.albedo;
        }
        else if (mat.type == MAT_METALLIC) {
            // Surface métallique: réflexion
            rd = reflect(rd, n, mat.roughness, hit, seed + float(bounce) * 2.71828);
            ro = hit + n * 0.001;
            throughput *= mat.albedo;
        }
        else if (mat.type == MAT_EMISSIVE) {
            col += throughput * mat.albedo * lightIntensity;
            break;
        }
        else if (mat.type == MAT_GLASS) {
            // Verre: réfraction ou réflexion
            float reflChance;
            rd = refract(rd, n, mat.ior, mat.roughness, hit, seed + float(bounce) * 1.41421, reflChance);
            ro = hit + normalize(rd) * 0.001;
            
            // Le verre absorbe un peu de lumière, principalement sur les longues distances
            float absorbance = 0.1;
            vec3 absorption = exp(-mat.albedo * absorbance * minT);
            //throughput *= absorption / (1.0 - reflChance);
            throughput *= mix(absorption, vec3(1.0), reflChance);

        }
        }

        
        // Roulette russe pour terminer prématurément les chemins à faible contribution
        if (bounce > 2) {
    float p = max(throughput.r, max(throughput.g, throughput.b));
    if (random(hit, seed + bounce * 0.77) > p) break;
    throughput /= p;
}


    }
    
    return col;
}

mat3 setCamera(vec3 ro, vec3 ta) {
    vec3 cw = normalize(ta - ro);
    vec3 cp = vec3(0.0, 1.0, 0.0);
    vec3 cu = normalize(cross(cw, cp));
    vec3 cv = normalize(cross(cu, cw));
    return mat3(cu, cv, cw);
}

void main() {
    vec3 color = vec3(0.0);
    
    // Anti-aliasing: multiplier les échantillons par pixel
    for (int s = 0; s < MAX_SAMPLES; ++s) {
        // Calculer le décalage du sous-pixel pour l'anti-aliasing
        float strataSize = 1.0 / sqrt(float(MAX_SAMPLES));
int strataX = s % int(sqrt(float(MAX_SAMPLES)));
int strataY = s / int(sqrt(float(MAX_SAMPLES)));

vec2 strata = vec2(float(strataX), float(strataY)) * strataSize;
vec2 inStrata = vec2(random(vec3(gl_FragCoord.xy, time), float(s) * 0.1),
                     random(vec3(gl_FragCoord.xy, time), float(s) * 0.2));

vec2 jitter = strata + inStrata * strataSize - 0.5;
        
        vec2 uv = ((gl_FragCoord.xy + jitter) * 2.0 - resolution.xy) / resolution.y;
        
        // Mise en place de la caméra
        mat3 cam = setCamera(viewEye, viewCenter);
        vec3 rd = cam * normalize(vec3(uv, 1.5));
        vec3 ro = viewEye;
        
        // Seed pour le générateur de nombres aléatoires
        float seed = float(s) + random(vec3(gl_FragCoord.xy, 0.0), time);
        
        // Tracer le rayon
        color += trace(ro, rd, seed);
    }
    
    // Moyenne des échantillons
    color /= float(MAX_SAMPLES);
    
    // Tone mapping (ACES)
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    color = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
    
    // Correction gamma
    color = pow(color, vec3(1.0 / 2.2));
    
    // Légère vignette
    vec2 q = gl_FragCoord.xy / resolution.xy;
    color *= 0.7 + 0.3 * pow(16.0 * q.x * q.y * (1.0 - q.x) * (1.0 - q.y), 0.1);
    vec3 prevColor = texture(previousFrame, gl_FragCoord.xy / resolution.xy).rgb;
    color = mix(color, prevColor, frameBlend);
    finalColor = vec4(color, 1.0);
}