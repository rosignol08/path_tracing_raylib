#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "raygui.h"
#include <stdlib.h>
#include <stdio.h>
#include <vector>

#define RLIGHTS_IMPLEMENTATION
#if defined(_WIN32) || defined(_WIN64)
#include "include/shaders/rlights.h"
#elif defined(__linux__)
#include "include/shaders/rlights.h"
#endif

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            330
#endif

// Variables globales pour stocker les angles de rotation
float angleX = 0.0f;
float angleY = 0.0f;
float distance_cam = 5.0f;

// Variable pour activer/désactiver la rotation
bool isRotating = false;

#define MAX_SPHERES 8

// Structure pour les sphères
typedef struct {
    Vector3 position;
    float radius;
} Sphere;

// Structure pour les matériaux
typedef struct {
    int type;         // 0 = diffus, 1 = métallique, 2 = verre, 3 = emissif
    float roughness;  // 0.0 - 1.0
    float ior;        // indice de réfraction (verre)
    float padding;    // pour alignement
    Vector3 albedo;   // couleur
    float padding2;   // pour alignement
} Material2;

// Données des sphères
Sphere spheres[MAX_SPHERES] = {
    {{0.0f, 0.0f, 0.0f}, 1.0f},     // Sphère centrale
    {{-2.5f, 0.0f, 0.0f}, 1.0f},    // Sphère à gauche
    {{2.5f, 0.0f, 0.0f}, 1.0f},     // Sphère à droite
    {{0.0f, -1001.0f, 0.0f}, 1000.0f}, // Sol (grosse sphère en dessous)
    {{0.0f, 0.0f, -2.5f}, 1.0f},    // Sphère derrière
    {{0.0f, 0.0f, 2.5f}, 1.0f},     // Sphère devant
    {{-1.5f, 0.0f, -1.5f}, 0.5f},   // Petite sphère
    {{1.5f, 0.0f, 1.5f}, 0.5f}      // Petite sphère
};

// Matériaux correspondants
Material2 materials[MAX_SPHERES] = {
    {0, 0.2f, 1.0f, 0.0f, {0.9f, 0.3f, 0.3f}, 0.0f},    // Rouge diffus
    {1, 0.1f, 1.0f, 0.0f, {0.8f, 0.8f, 0.9f}, 0.0f},    // Métal bleuté
    {2, 0.0f, 1.5f, 0.0f, {0.9f, 0.9f, 0.9f}, 0.0f},    // Verre
    {0, 0.5f, 1.0f, 0.0f, {0.8f, 0.8f, 0.8f}, 0.0f},    // Sol gris diffus
    {0, 0.3f, 1.0f, 0.0f, {0.3f, 0.9f, 0.3f}, 0.0f},    // Vert diffus
    {1, 0.2f, 1.0f, 0.0f, {0.9f, 0.6f, 0.2f}, 0.0f},    // Métal doré
    {2, 0.1f, 1.3f, 0.0f, {0.3f, 0.7f, 0.9f}, 0.0f},    // Verre bleuté
    {3, 0.0f, 1.0f, 0.0f, {0.9f, 0.9f, 0.0f}, 0.0f}     // Jaune diffus
};

// Position de la lumière
Vector3 lightPos = {5.0f, 10.0f, -2.0f};
// Couleur de la lumière
Vector3 lightColor = {1.0f, 0.9f, 0.8f}; // Lumière légèrement chaude
// Intensité de la lumière
float lightIntensity = 5.0f;

int main(void) {
    // Initialisation
    const int screenWidth = 1280;
    const int screenHeight = 720;
    
    SetConfigFlags(FLAG_MSAA_4X_HINT); // Enable Multi Sampling Anti Aliasing 4x (if available)
    InitWindow(screenWidth, screenHeight, "Raytracer avancé - GLSL");
    
    Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 2.0f, 6.0f };  // Position initiale de la caméra
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };    // Point visé par la caméra
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };        // Vecteur "up" de la caméra
    camera.fovy = 60.0f;                              // Field of view Y
    camera.projection = CAMERA_PERSPECTIVE;           // Type de projection

    // Chargement du shader de raytracing
    Shader shader = LoadShader(0, "raytest.fs");
    Shader denoiser_shader = LoadShader(0, "denoiser.fs");
    
    // Récupération des emplacements des uniformes dans le shader
    int viewEyeLoc = GetShaderLocation(shader, "viewEye");
    int viewCenterLoc = GetShaderLocation(shader, "viewCenter");
    int resolutionLoc = GetShaderLocation(shader, "resolution");
    int timeLoc = GetShaderLocation(shader, "time");
    
    // Paramètres de résolution pour le shader
    float resolution[2] = { (float)screenWidth, (float)screenHeight };
    SetShaderValue(shader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);
    
    // Emplacement des uniformes pour les sphères et les matériaux
    int spheresLoc = GetShaderLocation(shader, "spheres");
    int materialsLoc = GetShaderLocation(shader, "materials");
    int sphereCountLoc = GetShaderLocation(shader, "sphereCount");
    int lightPosLoc = GetShaderLocation(shader, "lightPos");
    int lightColorLoc = GetShaderLocation(shader, "lightColor");
    int lightIntensityLoc = GetShaderLocation(shader, "lightIntensity");
    
    // Passage du nombre de sphères au shader
    int sphereCount = MAX_SPHERES;
    SetShaderValue(shader, sphereCountLoc, &sphereCount, SHADER_UNIFORM_INT);
    
    float runTime = 0.0f;
    
    DisableCursor();  // Limite le curseur à l'intérieur de la fenêtre

    //la render texture pour appliquer le post process shader
    RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);
    RenderTexture2D history = LoadRenderTexture(screenWidth, screenHeight);

    SetTargetFPS(600); // Limite les FPS à 60
    
    // Boucle principale du jeu
    while (!WindowShouldClose()) {
        // Mise à jour de la logique
        float deltaTime = GetFrameTime();
        runTime += deltaTime;
        
        // Gestion des contrôles de la caméra
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) isRotating = true;
        if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) isRotating = false;
        
        // Capture des mouvements de la souris
        if (isRotating) {
            Vector2 mouseDelta = GetMouseDelta();
            angleX -= mouseDelta.y * 0.2f; // Sensibilité verticale
            angleY -= mouseDelta.x * 0.2f; // Sensibilité horizontale
        }
        
        // Gestion du zoom avec la molette de la souris
        distance_cam -= GetMouseWheelMove() * 0.5f;
        if (distance_cam < 2.0f) distance_cam = 2.0f;   // Distance minimale
        if (distance_cam > 20.0f) distance_cam = 20.0f; // Distance maximale
        
        // Limiter les angles X pour éviter une rotation complète
        if (angleX > 89.0f) angleX = 89.0f;
        if (angleX < -89.0f) angleX = -89.0f;
        
        // Calcul de la position de la caméra en coordonnées sphériques
        float radAngleX = DEG2RAD * angleX;
        float radAngleY = DEG2RAD * angleY;
        
        camera.position.x = distance_cam * cos(radAngleX) * sin(radAngleY);
        camera.position.y = distance_cam * sin(radAngleX);
        camera.position.z = distance_cam * cos(radAngleX) * cos(radAngleY);
        
        // Mouvement de la lumière sur un chemin circulaire
        lightPos.x = 5.0f * cosf(runTime * 0.5f);
        lightPos.y = 5.0f + 2.0f * sinf(runTime * 0.3f);
        lightPos.z = 3.0f * sinf(runTime * 0.7f);
        
        // Contrôles optionnels pour ajuster manuellement la lumière
        if (IsKeyDown(KEY_U)) lightPos.y += 0.2f;
        if (IsKeyDown(KEY_J)) lightPos.y -= 0.2f;
        if (IsKeyDown(KEY_H)) lightPos.x -= 0.2f;
        if (IsKeyDown(KEY_K)) lightPos.x += 0.2f;
        if (IsKeyDown(KEY_Y)) lightIntensity -= 0.2f;
        if (IsKeyDown(KEY_I)) lightIntensity += 0.2f;
        // Make light intensity oscillate between 0 and 2
        //lightIntensity = 1.0f + sinf(runTime * 1.5f);
        // Ajustement de l'intensité de la lumière
        //if (IsKeyDown(KEY_EQUAL)) lightIntensity += 0.2f;
        //if (IsKeyDown(KEY_MINUS) && lightIntensity > 0.2f) lightIntensity -= 0.2f;
        
        // Passage des valeurs des uniformes au shader
        float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        float cameraTarget[3] = { 0.0f, 0.0f, 0.0f }; // On regarde toujours l'origine
        
        SetShaderValue(shader, viewEyeLoc, cameraPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, viewCenterLoc, cameraTarget, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, timeLoc, &runTime, SHADER_UNIFORM_FLOAT);
        
        // Envoi des données des sphères et des matériaux au shader
        // Note: Ces structures doivent être correctement alignées pour le GPU
        for (int i = 0; i < MAX_SPHERES; i++) {
            // Format vec4 pour chaque sphère (position + rayon)
            float sphereData[4] = { 
                spheres[i].position.x, 
                spheres[i].position.y, 
                spheres[i].position.z, 
                spheres[i].radius 
            };
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("spheres[%d]", i)), 
                           sphereData, SHADER_UNIFORM_VEC4);
                           
            // Transmission du matériau
            // Attention: ceci est une approche simplifiée, l'alignement peut poser problème
            // Pour un code plus robuste, considérer l'utilisation d'UBO/SSBO si disponible
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials[%d].type", i)), 
                          &materials[i].type, SHADER_UNIFORM_INT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials[%d].roughness", i)), 
                          &materials[i].roughness, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials[%d].ior", i)), 
                          &materials[i].ior, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials[%d].albedo", i)), 
                          &materials[i].albedo, SHADER_UNIFORM_VEC3);
        }
        
        // Mise à jour de la position de la lumière
        SetShaderValue(shader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, lightColorLoc, &lightColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, lightIntensityLoc, &lightIntensity, SHADER_UNIFORM_FLOAT);
        
        // Vérification si la fenêtre est redimensionnée
        if (IsWindowResized()) {
            resolution[0] = (float)GetScreenWidth();
            resolution[1] = (float)GetScreenHeight();
            SetShaderValue(shader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);
        }
        
        // Dessin
        BeginTextureMode(target);       // Enable drawing to texture
                          // End drawing to texture (now we have a texture available for next passes)
        
        //BeginDrawing();
        //BeginDrawing(); // Start 3d mode drawing
            //ClearBackground(BLACK);
            
            // On dessine simplement un rectangle plein écran blanc,
            // l'image est générée dans le shader de raytracing
            BeginShaderMode(shader);
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
            EndShaderMode();
            //EndDrawing();
            
        //EndDrawing();
        
        EndTextureMode();

        //BeginDrawing();
            //ClearBackground(RAYWHITE);  // Clear screen background

            //pour afficher avec le shader de denoising
            //BeginShaderMode(denoiser_shader);
            //    // NOTE: Render texture must be y-flipped due to default OpenGL coordinates (left-bottom)
            //    DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height }, (Vector2){ 0, 0 }, WHITE);
            //EndShaderMode();
            // Activer le shader de débruitage
            BeginTextureMode(history);
        BeginShaderMode(denoiser_shader);
            // Passer les uniformes nécessaires au shader de débruitage
            int resolutionLoc = GetShaderLocation(denoiser_shader, "u_resolution");
            float resolution[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
            SetShaderValue(denoiser_shader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);

            int timeLoc = GetShaderLocation(denoiser_shader, "u_time");
            SetShaderValue(denoiser_shader, timeLoc, &runTime, SHADER_UNIFORM_FLOAT);

            int denoiseStrengthLoc = GetShaderLocation(denoiser_shader, "u_denoiseStrength");
            float denoiseStrength = 6.0f; // Force le débruitage
            SetShaderValue(denoiser_shader, denoiseStrengthLoc, &denoiseStrength, SHADER_UNIFORM_FLOAT);

            // Passer la texture history (ancienne frame) au shader (il faut ajouter uniform sampler2D u_history)
            int historyLoc = GetShaderLocation(denoiser_shader, "u_history");
            SetShaderValueTexture(denoiser_shader, historyLoc, history.texture);


            // Dessiner la texture avec le shader de débruitage
            DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height }, (Vector2){ 0, 0 }, WHITE);
        EndShaderMode();
        EndTextureMode();
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawTextureRec(history.texture, (Rectangle){ 0, 0, (float)history.texture.width, (float)-history.texture.height }, (Vector2){ 0, 0 }, WHITE);
            // Affichage d'informations
            DrawFPS(10, 10);
            DrawText(TextFormat("Light Intensity: %.1f", lightIntensity), 10, 30, 20, WHITE);
            DrawText("Controls:", 10, GetScreenHeight() - 90, 20, WHITE);
            DrawText("  Mouse Right - Rotate camera", 10, GetScreenHeight() - 70, 20, WHITE);
            DrawText("  Mouse Wheel - Zoom in/out", 10, GetScreenHeight() - 50, 20, WHITE);
            DrawText("  H/K/U/J/Y/I - Move light, +/- Change intensity", 10, GetScreenHeight() - 30, 20, WHITE);
        EndDrawing();
    }
    
    // Nettoyage
    UnloadShader(shader);
    UnloadShader(denoiser_shader);
    UnloadRenderTexture(target); // Unload render texture
    CloseWindow();
    
    return 0;
}