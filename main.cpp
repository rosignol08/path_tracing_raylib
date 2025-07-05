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
#define MAX_BLOCKS 6

// Structure pour les sphères
typedef struct {
    Vector3 position;
    float radius;
} Sphere;

//structure pour les blocs (murs)
typedef struct {
    Vector3 position;
    Vector3 size; // Taille du bloc (largeur, hauteur, profondeur)
} Block;

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
//int type;       // Type de matériau
//float roughness; // Rugosité (métal, verre)
//float ior;      // Indice de réfraction (verre)
//float padding;  // Padding pour l'alignement
//vec3 albedo;    // Couleur de base
//float padding2; // Padding supplémentaire

Material2 materials[MAX_SPHERES] = {
    {4, 0.0f, 1.0f, 0.0f, {1.0f, 1.0f, 1.0f}, 0.0f},    // Balle miroir
    {1, 0.1f, 1.0f, 0.0f, {0.8f, 0.8f, 0.9f}, 0.0f},    // Métal bleuté
    {2, 0.0f, 1.5f, 0.0f, {0.9f, 0.9f, 0.9f}, 0.0f},    // Verre
    {0, 0.5f, 1.0f, 0.0f, {0.8f, 0.8f, 0.8f}, 0.0f},    // Sol gris diffus
    {0, 0.2f, 1.0f, 0.0f, {0.9f, 0.3f, 0.3f}, 0.0f},    // Rouge diffus
    {1, 0.2f, 1.0f, 0.0f, {0.9f, 0.6f, 0.2f}, 0.0f},    // Métal doré
    {2, 0.1f, 1.3f, 0.0f, {0.3f, 0.7f, 0.9f}, 0.0f},    // Verre bleuté
    {3, 0.0f, 1.0f, 0.0f, {0.9f, 0.9f, 0.0f}, 0.0f}     // Jaune diffus
};

// Dans main.cpp, ajustez la définition des murs selon vos besoins
Block blocks[MAX_BLOCKS] = {
    {{0.0f, -1.0f, 0.0f}, {20.0f, 0.1f, 20.0f}},  // Sol
    {{0.0f, 10.0f, 0.0f}, {20.0f, 0.1f, 20.0f}},  // Plafond
    {{-10.0f, 0.0f, 0.0f}, {0.1f, 20.0f, 20.0f}}, // Mur gauche
    {{10.0f, 0.0f, 0.0f}, {0.1f, 20.0f, 20.0f}},  // Mur droit
    {{0.0f, 0.0f, -10.0f}, {20.0f, 20.0f, 0.1f}}, // Mur arrière
    {{0.0f, 0.0f, 10.0f}, {20.0f, 20.0f, 0.1f}}   // Mur avant
};

Material2 materials_block[MAX_BLOCKS] = {
    {1, 0.1f, 1.0f, 0.0f, {0.2f, 0.2f, 0.225f}, 0.0f}, // Mur gauche gris
    {1, 0.1f, 1.0f, 0.0f, {0.2f, 0.2f, 0.225f}, 0.0f}, // Mur droit gris
    {1, 0.1f, 1.0f, 0.0f, {0.2f, 0.2f, 0.225f}, 0.0f}, // Mur arrière gris
    {1, 0.1f, 1.0f, 0.0f, {0.2f, 0.2f, 0.225f}, 0.0f}, // Mur avant gris
    {1, 0.1f, 1.0f, 0.0f, {0.2f, 0.2f, 0.225f}, 0.0f}, // Mur gauche avant gris
    {1, 0.1f, 1.0f, 0.0f, {0.2f, 0.2f, 0.225f}, 0.0f}  // Mur droit avant gris
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
    //Shader denoiser_shader = LoadShader(0, "denoiser.fs");

    //test denoiser plusieurs passes
    Shader denoise_shader = LoadShader(0, "denoise.fs");
    Shader taa_shader = LoadShader(0, "taa.fs");
    
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
    //pareil pour les blocs de murs
    int blocksLoc = GetShaderLocation(shader, "blocks");
    int materialsBlockLoc = GetShaderLocation(shader, "materials_block");
    int blockCountLoc = GetShaderLocation(shader, "blockCount");
    
    // Passage du nombre de sphères au shader
    int sphereCount = MAX_SPHERES;
    SetShaderValue(shader, sphereCountLoc, &sphereCount, SHADER_UNIFORM_INT);
    
    // Passage du nombre de blocs au shader
    int blockCount = MAX_BLOCKS;
    SetShaderValue(shader, blockCountLoc, &blockCount, SHADER_UNIFORM_INT);

    float runTime = 0.0f;
    
    DisableCursor();  // Limite le curseur à l'intérieur de la fenêtre

    //la render texture pour appliquer le post process shader
    RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);
    //RenderTexture2D history = LoadRenderTexture(screenWidth, screenHeight);

    //pour le shader de denoising
    RenderTexture2D renderNoisy = LoadRenderTexture(screenWidth, screenHeight);
    RenderTexture2D renderNormals = LoadRenderTexture(screenWidth, screenHeight);
    RenderTexture2D renderHistory = LoadRenderTexture(screenWidth, screenHeight);
    RenderTexture2D denoiseTarget = LoadRenderTexture(screenWidth, screenHeight);
    RenderTexture2D taaOutput = LoadRenderTexture(screenWidth, screenHeight);
    
    int frameCounter = 0;

    SetTargetFPS(60); // Limite les FPS à 60
    
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
        // Variable pour suivre si la touche R est pressée
        static bool isColorCycling = false;

        // Vérifier si la touche R est pressée
        if (IsKeyPressed(KEY_R)) {
            isColorCycling = !isColorCycling;  // Activer/désactiver le cycle de couleurs
        }

        // Si le cycle de couleurs est actif, modifier les couleurs
        if (isColorCycling) {
            // Cycle de couleurs pour la première sphère (miroir)
            //materials[0].albedo.x = 0.5f + 0.5f * sinf(runTime * 1.1f);          // Rouge
            //materials[0].albedo.y = 0.5f + 0.5f * sinf(runTime * 0.7f + 2.0f);   // Vert
            //materials[0].albedo.z = 0.5f + 0.5f * sinf(runTime * 0.9f + 4.0f);   // Bleu
            
            // Cycle de couleurs pour la sphère émissive (index 7)
            materials[7].albedo.x = 0.5f + 0.5f * sinf(runTime * 0.5f + 1.0f);   // Rouge
            materials[7].albedo.y = 0.5f + 0.5f * sinf(runTime * 0.8f + 3.0f);   // Vert
            materials[7].albedo.z = 0.5f + 0.5f * sinf(runTime * 0.6f + 5.0f);   // Bleu
            
            // Synchroniser la couleur de la lumière avec la sphère émissive
            lightColor.x = materials[7].albedo.x;
            lightColor.y = materials[7].albedo.y;
            lightColor.z = materials[7].albedo.z;
        }
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
        // Envoi des données des blocs et de leurs matériaux au shader
        for (int i = 0; i < MAX_BLOCKS; i++) {
            // Format vec4 pour chaque bloc (position + taille)
            float blockPos[3] = {
        blocks[i].position.x, 
        blocks[i].position.y, 
        blocks[i].position.z
    };
    SetShaderValue(shader, GetShaderLocation(shader, TextFormat("blocks[%d]", i)),
                   blockPos, SHADER_UNIFORM_VEC3);
    
    // Transmettez la taille séparément
    float blockSize[3] = {
        blocks[i].size.x,
        blocks[i].size.y,
        blocks[i].size.z
    };
    SetShaderValue(shader, GetShaderLocation(shader, TextFormat("blockSizes[%d]", i)),
                   blockSize, SHADER_UNIFORM_VEC3);
            // Transmission du matériau du bloc
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials_block[%d].type", i)),
                          &materials_block[i].type, SHADER_UNIFORM_INT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials_block[%d].roughness", i)),
                            &materials_block[i].roughness, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials_block[%d].ior", i)),
                            &materials_block[i].ior, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, GetShaderLocation(shader, TextFormat("materials_block[%d].albedo", i)),
                            &materials_block[i].albedo, SHADER_UNIFORM_VEC3);
        }
        
        // Mise à jour de la position de la lumière
        SetShaderValue(shader, lightPosLoc, &lightPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, lightColorLoc, &lightColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, lightIntensityLoc, &lightIntensity, SHADER_UNIFORM_FLOAT);
        
        //liaison entre les textures et les shaders
        SetShaderValueTexture(denoise_shader, GetShaderLocation(denoise_shader, "renderNoisy"), renderNoisy.texture);
        SetShaderValueTexture(denoise_shader, GetShaderLocation(denoise_shader, "renderNormals"), renderNormals.texture);
        SetShaderValueTexture(denoise_shader, GetShaderLocation(denoise_shader, "renderHistory"), renderHistory.texture);

        //pour le taa shader
        SetShaderValue(taa_shader, GetShaderLocation(taa_shader, "resolution"), resolution, SHADER_UNIFORM_VEC2);
        SetShaderValue(taa_shader, GetShaderLocation(taa_shader, "time"), &runTime, SHADER_UNIFORM_FLOAT);
        SetShaderValue(taa_shader, GetShaderLocation(taa_shader, "frame"), &frameCounter, SHADER_UNIFORM_INT);

        SetShaderValueTexture(taa_shader, GetShaderLocation(taa_shader, "currentFrame"), denoiseTarget.texture);
        SetShaderValueTexture(taa_shader, GetShaderLocation(taa_shader, "historyFrame"), renderHistory.texture);


        // Vérification si la fenêtre est redimensionnée
        if (IsWindowResized()) {
            resolution[0] = (float)GetScreenWidth();
            resolution[1] = (float)GetScreenHeight();
            SetShaderValue(shader, resolutionLoc, resolution, SHADER_UNIFORM_VEC2);
        }
        
        // Dessin
        BeginTextureMode(renderNoisy);       // Enable drawing to texture
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


            BeginTextureMode(denoiseTarget); // ← on dessine dans denoiseTarget (frame courante débruitée)
                BeginShaderMode(denoise_shader);
                    // Uniformes
                    float resolution[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
                    SetShaderValue(denoise_shader, GetShaderLocation(denoise_shader, "resolution"), resolution, SHADER_UNIFORM_VEC2);

                    SetShaderValue(denoise_shader, GetShaderLocation(denoise_shader, "time"), &runTime, SHADER_UNIFORM_FLOAT);
                    SetShaderValue(denoise_shader, GetShaderLocation(denoise_shader, "frame"), &frameCounter, SHADER_UNIFORM_INT);

                    float denoiseStrength = 1.0f;
                    SetShaderValue(denoise_shader, GetShaderLocation(denoise_shader, "u_denoiseStrength"), &denoiseStrength, SHADER_UNIFORM_FLOAT);

                    // Textures (attention aux noms !)
                    SetShaderValueTexture(denoise_shader, GetShaderLocation(denoise_shader, "renderNoisy"), renderNoisy.texture);
                    SetShaderValueTexture(denoise_shader, GetShaderLocation(denoise_shader, "renderNormals"), renderNormals.texture);
                    SetShaderValueTexture(denoise_shader, GetShaderLocation(denoise_shader, "renderHistory"), renderHistory.texture);

                    // Dessiner un quad plein écran pour appliquer le shader
                    DrawTexturePro(
                        renderNoisy.texture,                       // source texture (image bruitée)
                        (Rectangle){ 0, 0, (float)screenWidth, -(float)screenHeight },
                        (Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight },
                        (Vector2){ 0, 0 },
                        0.0f,
                        WHITE
                    );
                EndShaderMode();
            EndTextureMode();

// Application du TAA à la texture de sortie finale
BeginTextureMode(taaOutput);  // Capture le résultat du TAA dans taaOutput
    BeginShaderMode(taa_shader);
        // Passer la texture courante (débruitée) et la frame précédente
        SetShaderValueTexture(taa_shader, GetShaderLocation(taa_shader, "currentFrame"), denoiseTarget.texture);
        SetShaderValueTexture(taa_shader, GetShaderLocation(taa_shader, "historyFrame"), renderHistory.texture);

        // Uniformes nécessaires
        SetShaderValue(taa_shader, GetShaderLocation(taa_shader, "time"), &runTime, SHADER_UNIFORM_FLOAT);
        SetShaderValue(taa_shader, GetShaderLocation(taa_shader, "frame"), &frameCounter, SHADER_UNIFORM_INT);

        DrawTexturePro(
            denoiseTarget.texture,
            (Rectangle){ 0, 0, (float)screenWidth, -(float)screenHeight },
            (Rectangle){ 0, 0, (float)screenWidth, (float)screenHeight },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
    EndShaderMode();
EndTextureMode();
//pour enlever les artefacts de la frame précédente
if (frameCounter % 3 == 0) {
    BeginTextureMode(renderHistory);
        // On écrase totalement l'historique avec l'image courante (nettoyée)
        DrawTextureRec(
            denoiseTarget.texture,
            (Rectangle){ 0, 0, (float)screenWidth, -(float)screenHeight },
            (Vector2){ 0, 0 },
            WHITE
        );
    EndTextureMode();
}

            //pour la derniere image
            BeginTextureMode(renderHistory);
                DrawTextureRec(
                        taaOutput.texture,
                        (Rectangle){ 0, 0, (float)screenWidth, -(float)screenHeight },
                        (Vector2){ 0, 0 },
                        WHITE
                    );
                EndTextureMode();
                
BeginDrawing();
    //ClearBackground(BLACK); //faut pas mettre ça sinon ça assombrit l'image

    // Dessiner le résultat du TAA
    DrawTextureRec(
        taaOutput.texture,
        (Rectangle){ 0, 0, (float)screenWidth, -(float)screenHeight },
        (Vector2){ 0, 0 },
        WHITE
    );
    
    // Affichage d'informations
    DrawFPS(10, 10);
    DrawText(TextFormat("Light Intensity: %.1f", lightIntensity), 10, 30, 20, WHITE);
    DrawText("Controls:", 10, GetScreenHeight() - 90, 20, WHITE);
    DrawText("  Mouse Right - Rotate camera", 10, GetScreenHeight() - 70, 20, WHITE);
    DrawText("  Mouse Wheel - Zoom in/out", 10, GetScreenHeight() - 50, 20, WHITE);
    DrawText("  H/K/U/J/Y/I - Move light, +/- Change intensity", 10, GetScreenHeight() - 30, 20, WHITE);
EndDrawing();

        frameCounter++;

    }
    
    // Nettoyage
    UnloadShader(shader);
    UnloadShader(denoise_shader);
    UnloadShader(taa_shader);
    UnloadRenderTexture(target); // Unload render texture
    UnloadRenderTexture(renderNoisy);
    UnloadRenderTexture(renderNormals);
    UnloadRenderTexture(renderHistory);
    UnloadRenderTexture(denoiseTarget);
    CloseWindow();
    
    return 0;
}