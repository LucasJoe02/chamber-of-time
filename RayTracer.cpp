/*==================================================================================
* COSC 363  Computer Graphics (2023)
* Department of Computer Science and Software Engineering, University of Canterbury.
*
* A basic ray tracer
* See Lab06.pdf   for details.
*===================================================================================
*/
#include <iostream>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include "Sphere.h"
#include "Plane.h"
#include "Cone.h"
#include "SceneObject.h"
#include "Ray.h"
#include "TextureBMP.h"
#include <GL/freeglut.h>
#include "FastNoiseLite.h"
using namespace std;

const float EDIST = 40.0;
const int NUMDIV = 500;
const int MAX_STEPS = 5;
const float XMIN = -10.0;
const float XMAX = 10.0;
const float YMIN = -10.0;
const float YMAX = 10.0;
const float PI = 3.14159265f;

const glm::vec2 FOG_RANGE(-50, -220);
const glm::vec3 FOG_COL(0.7);

const bool FOG = false;
const bool ANTIALIASING = false;

vector<SceneObject*> sceneObjects;
TextureBMP texture;

float noiseOffset;
FastNoiseLite noise;


glm::vec3 fog(glm::vec3 color, Ray ray)
{
    float lambda = (ray.hit.z - FOG_RANGE[0])/(FOG_RANGE[1]-FOG_RANGE[0]);
    color = (1-lambda)*color + lambda * FOG_COL;
    return color;
}

//---The most important function in a ray tracer! ---------------------------------- 
//   Computes the colour value obtained by tracing a ray and finding its 
//     closest point of intersection with objects in the scene.
//----------------------------------------------------------------------------------
glm::vec3 trace(Ray ray, int step)
{
	glm::vec3 backgroundCol(0);						//Background colour = (0,0,0)
    glm::vec3 lightPos(0, 25, -135);					//Light's position
	glm::vec3 color(0);
	SceneObject* obj;

    ray.closestPt(sceneObjects);					//Compare the ray with all objects in the scene
    if(ray.index == -1) return backgroundCol;		//no intersection
	obj = sceneObjects[ray.index];					//object on which the closest point of intersection is found

    if(ray.index == 0) {
        //Checkerboard pattern
        //Shift the pattern by 30 to remove long rectangles from center of board
        int stripeWidth = 5;
        int iz = (int)(ray.hit.z) / stripeWidth;
        int ix = (int)(ray.hit.x) / stripeWidth;
        int j = ix % 2;
        int k = iz % 2;
        if (ray.hit.x < 0) j = !j;
        if ((j + k) % 2 == 0) color = glm::vec3(0.6, 0.2, 0.05);
        else color = glm::vec3(0.65, 0.3, 0.1);

        obj->setColor(color);

    }

    if(ray.index == 1) {
        //Procedural pattern
        float value = noise.GetNoise(ray.hit.x*10, ray.hit.y*10);

        float h = (1.0 - value) * 6.0f;
        float x = 1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f);

        float r, g, b;

        if (h >= 0.0f && h < 1.0f) {
            r = 1.0f;
            g = x;
            b = 0.0f;
        } else if (h >= 1.0f && h < 2.0f) {
            r = x;
            g = 1.0f;
            b = 0.0f;
        } else if (h >= 2.0f && h < 3.0f) {
            r = 0.0f;
            g = 1.0f;
            b = x;
        } else if (h >= 3.0f && h < 4.0f) {
            r = 0.0f;
            g = x;
            b = 1.0f;
        } else if (h >= 4.0f && h < 5.0f) {
            r = x;
            g = 0.0f;
            b = 1.0f;
        } else {
            r = 1.0f;
            g = 0.0f;
            b = x;
        }

        color = glm::vec3(r*0.6, g*0.6, b*0.6);

        obj->setColor(color);

    }

    if(obj->hasSphereTex()) {
        glm::vec3 normal = obj->normal(ray.hit);
        float texcoords = atan2(normal.x, normal.z) / (2*PI) + 0.5;
        float texcoordt = normal.y * 0.5 + 0.5;

        if (texcoords >= 0 && texcoords <= 1 && texcoordt >= 0 && texcoordt <= 1) {
            color = texture.getColorAt(texcoordt, texcoords);
            obj->setColor(color);
        }
    }

    color = obj->lighting(lightPos,-ray.dir,ray.hit);						//Object's colour

    glm::vec3 lightVec = lightPos - ray.hit;
    Ray shadowRay(ray.hit, lightVec);
    shadowRay.closestPt(sceneObjects);
    float lightDist = glm::length(lightVec);
    SceneObject *shadowObject = sceneObjects[shadowRay.index];
    if(shadowRay.index > -1 && shadowRay.dist < lightDist && shadowRay.index != ray.index) {
        if (shadowObject->isTransparent() || shadowObject->isRefractive()) color = 0.5f * obj->getColor();
        else color = 0.1f * obj->getColor(); //0.2 = ambient scale factor
    }

    if(obj->isRefractive() && step < MAX_STEPS) {
        float eta = obj->getRefractiveIndex();
        float rho = obj->getRefractionCoeff();
        glm::vec3 normalVec = obj->normal(ray.hit);
        glm::vec3 refrVec = glm::refract(ray.dir, normalVec, eta);
        Ray refrRay(ray.hit, refrVec);
        refrRay.closestPt(sceneObjects);
        glm::vec3 exitNormal = obj->normal(refrRay.hit);
        glm::vec3 exitVec = glm::refract(refrVec, -exitNormal, 1.0f/eta);
        Ray exitRay(refrRay.hit, exitVec);
        glm::vec3 refractedColor = trace(exitRay, step + 1);
        color = color + (rho * refractedColor);
    }


    if(obj->isReflective() && step < MAX_STEPS) {
        float rho = obj->getReflectionCoeff();
        glm::vec3 normalVec = obj->normal(ray.hit);
        glm::vec3 reflectedDir = glm::reflect(ray.dir, normalVec);
        Ray reflectedRay(ray.hit, reflectedDir);
        glm::vec3 reflectedColor = trace(reflectedRay, step + 1);
        color = color + (rho * reflectedColor);
    }

    if(obj->isTransparent() && step < MAX_STEPS) {
        float rho = obj->getTransparencyCoeff();
        Ray transparentRay(ray.hit, ray.dir);
        glm::vec3 transparentColor = trace(transparentRay, step + 1);
        color = color + (rho * transparentColor);
    }

    if(FOG) {
        color = fog(color, ray);
    }

    return color;
}

//---Anti-aliasing  --------------------------------------------------------------------
// Takes the grid point of a pixel and its width and height along with camera position.
// Traces four rays to find the average colour of the pixel
//------------------------------------------------------8---------------------------------
glm::vec3 antiAliase(float xp, float yp, float cellX, float cellY, glm::vec3 eye)
{
    glm::vec3 dir1(xp + 0.25 * cellX, yp + 0.25 * cellY, -EDIST); //direction of first ray
    glm::vec3 dir2(xp + 0.75 * cellX, yp + 0.25 * cellY, -EDIST); //direction of second ray
    glm::vec3 dir3(xp + 0.75 * cellX, yp + 0.75 * cellY, -EDIST); //direction of third ray
    glm::vec3 dir4(xp + 0.25 * cellX, yp + 0.75 * cellY, -EDIST); //direction of fourth ray

    Ray ray1 = Ray(eye, dir1);
    Ray ray2 = Ray(eye, dir2);
    Ray ray3 = Ray(eye, dir3);
    Ray ray4 = Ray(eye, dir4);

    glm::vec3 col1 = trace(ray1, 1); //Trace the ray and get the colour value
    glm::vec3 col2 = trace(ray2, 1);
    glm::vec3 col3 = trace(ray3, 1);
    glm::vec3 col4 = trace(ray4, 1);

    glm::vec3 avg = (col1+col2+col3+col4) / 4.0f;
    return avg;
}

//---The main display module -----------------------------------------------------------
// In a ray tracing application, it just displays the ray traced image by drawing
// each cell as a quad.
//---------------------------------------------------------------------------------------
void display()
{
	float xp, yp;  //grid point
	float cellX = (XMAX - XMIN) / NUMDIV;  //cell width
	float cellY = (YMAX - YMIN) / NUMDIV;  //cell height
	glm::vec3 eye(0., 0., 0.);

	glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	glBegin(GL_QUADS);  //Each cell is a tiny quad.

	for (int i = 0; i < NUMDIV; i++)	//Scan every cell of the image plane
	{
		xp = XMIN + i * cellX;
		for (int j = 0; j < NUMDIV; j++)
		{
			yp = YMIN + j * cellY;

            glm::vec3 col;
            if (ANTIALIASING) {
                col = antiAliase(xp,yp,cellX,cellY,eye);
            } else
            {
                glm::vec3 dir(xp + 0.5 * cellX, yp + 0.5 * cellY, -EDIST);	//direction of the primary ray
                Ray ray = Ray(eye, dir);
                col = trace(ray, 1); //Trace the primary ray and get the colour value
            }

			glColor3f(col.r, col.g, col.b);
			glVertex2f(xp, yp);				//Draw each cell with its color value
            glVertex2f(xp + cellX, yp);
			glVertex2f(xp + cellX, yp + cellY);
			glVertex2f(xp, yp + cellY);
		}
	}


    glEnd();
    glFlush();
}

//---This function initializes the Cornell box -------------------------------------
//   It creates plane scene objects to show a cornell box
//     and adds them to the list of scene objects.
//----------------------------------------------------------------------------------
void initBox()
{
    Plane *floor = new Plane(glm::vec3(-30., -30, 0),     //Point A
                             glm::vec3(30., -30, 0),      //Point B
                             glm::vec3(30., -30, -200),     //Point C
                             glm::vec3(-30., -30, -200));   //Point D
    floor->setColor(glm::vec3(0.8, 0.75, 0.7));
    floor->setSpecularity(false);
    sceneObjects.push_back(floor);

    Plane *back = new Plane(glm::vec3(-30., -30, -200),     //Point A
                             glm::vec3(30., -30, -200),      //Point B
                             glm::vec3(30., 30, -200),     //Point C
                             glm::vec3(-30., 30, -200));   //Point D
    back->setColor(glm::vec3(0.7, 0.5, 0.4));
    back->setSpecularity(false);
    sceneObjects.push_back(back);

    Plane *left = new Plane(glm::vec3(-30., -30, 0),     //Point A
                             glm::vec3(-30., -30, -200),      //Point B
                             glm::vec3(-30., 30, -200),     //Point C
                             glm::vec3(-30., 30, 0));   //Point D
    left->setColor(glm::vec3(0, 0.5, 0.5));
    left->setSpecularity(false);
    sceneObjects.push_back(left);

    Plane *right = new Plane(glm::vec3(30., -30, -200),     //Point A
                             glm::vec3(30., -30, 0),      //Point B
                             glm::vec3(30., 30, 0),     //Point C
                             glm::vec3(30., 30, -200));   //Point D
    right->setColor(glm::vec3(0.5, 0, 0.5));
    right->setSpecularity(true);
    sceneObjects.push_back(right);

    Plane *roof = new Plane(glm::vec3(-30., 30, -200),     //Point A
                             glm::vec3(30., 30, -200),      //Point B
                             glm::vec3(30., 30, 0),     //Point C
                             glm::vec3(-30., 30, 0));   //Point D
    roof->setColor(glm::vec3(0.8, 0.75, 0.7));
    roof->setSpecularity(false);
    sceneObjects.push_back(roof);

    Plane *front = new Plane(glm::vec3(30., -30, 0),     //Point A
                             glm::vec3(-30., -30, 0),      //Point B
                             glm::vec3(-30., 30, 0),     //Point C
                             glm::vec3(30., 30, 0));   //Point D
    front->setColor(glm::vec3(0.2, 0.27, 0.16));
    front->setSpecularity(false);
    sceneObjects.push_back(front);


    Plane *lamp = new Plane(glm::vec3(-5, 29.9, -140),     //Point A
                             glm::vec3(5, 29.9, -140),      //Point B
                             glm::vec3(5, 29.9, -130),     //Point C
                             glm::vec3(-5, 29.9, -130));   //Point D
    lamp->setColor(glm::vec3(1, 1, 1));
    lamp->setSpecularity(false);
    sceneObjects.push_back(lamp);
}

void initCones()
{
    for (int i = -1; i < 2; i++) {
        Cone *cone = new Cone(glm::vec3(i*20, -15, -145.0), 0.5,1);
        cone->setColor(glm::vec3(0.2,0.1,0.4));
        cone->setSpecularity(true);
        cone->setShininess(5);
        sceneObjects.push_back(cone);
    }
}

void initSpheres()
{
    Sphere *transparentSphere = new Sphere(glm::vec3(0, -22, -145.0), 5.0);
    transparentSphere->setColor(glm::vec3(0, 0, 0.5));   //Set colour to blue
    transparentSphere->setSpecularity(true);
    transparentSphere->setReflectivity(true, 0.1);
    transparentSphere->setTransparency(true, 0.8);
    sceneObjects.push_back(transparentSphere);		 //Add sphere to scene objects

    Sphere *refractiveSphere = new Sphere(glm::vec3(20, -22, -145.0), 5.0);
    refractiveSphere->setColor(glm::vec3(0.2, 0, 0));
    refractiveSphere->setSpecularity(false);
    refractiveSphere->setRefractivity(true, 0.9, 1.02);
    refractiveSphere->setReflectivity(true, 0.1);
    sceneObjects.push_back(refractiveSphere);		 //Add sphere to scene objects

    Sphere *matteSphere = new Sphere(glm::vec3(-20, -22, -145.0), 5.0);
    matteSphere->setColor(glm::vec3(1, 1, 1));   //Set colour to blue
    matteSphere->setSpecularity(false);
    matteSphere->setSphereTex(true);
    sceneObjects.push_back(matteSphere);		 //Add sphere to scene objects
}

void initMirrors()
{
    Plane *fMirrorFrame = new Plane(glm::vec3(-30, -26, -190.1),     //Point A
                                 glm::vec3(30, -26, -190.1),      //Point B
                                 glm::vec3(30, -9, -190.1),     //Point C
                                 glm::vec3(-30, -9, -190.1));   //Point D
    fMirrorFrame->setColor(glm::vec3(0.83, 0.69, 0.22));
    fMirrorFrame->setSpecularity(true);
    fMirrorFrame->setShininess(7);
    sceneObjects.push_back(fMirrorFrame);

    Plane *fMirror = new Plane(glm::vec3(-29, -25, -190),     //Point A
                             glm::vec3(29, -25, -190),      //Point B
                             glm::vec3(29, -10, -190),     //Point C
                             glm::vec3(-29, -10, -190));   //Point D
    fMirror->setColor(glm::vec3(0, 0, 0));
    fMirror->setSpecularity(false);
    fMirror->setReflectivity(true, 1);
    sceneObjects.push_back(fMirror);

    Plane *bMirrorFrame = new Plane(glm::vec3(30, -26, -9.9),     //Point A
                                 glm::vec3(-30, -26, -9.9),      //Point B
                                 glm::vec3(-30, -9, -9.9),     //Point C
                                 glm::vec3(30, -9, -9.9));   //Point D
    bMirrorFrame->setColor(glm::vec3(0.83, 0.69, 0.22));
    bMirrorFrame->setSpecularity(true);
    bMirrorFrame->setShininess(7);
    sceneObjects.push_back(bMirrorFrame);

    Plane *bMirror = new Plane(glm::vec3(29, -25, -10),     //Point A
                             glm::vec3(-29, -25, -10),      //Point B
                             glm::vec3(-29, -10, -10),     //Point C
                             glm::vec3(29, -10, -10));   //Point D
    bMirror->setColor(glm::vec3(0, 0, 0));
    bMirror->setSpecularity(false);
    bMirror->setReflectivity(true, 1);
    sceneObjects.push_back(bMirror);
}

//---This function initializes the scene ------------------------------------------- 
//   Specifically, it creates scene objects (spheres, planes, cones, cylinders etc)
//     and add them to the list of scene objects.
//   It also initializes the OpenGL 2D orthographc projection matrix for drawing the
//     the ray traced image.
//----------------------------------------------------------------------------------
void initialize()
{
    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(XMIN, XMAX, YMIN, YMAX);

    glClearColor(0, 0, 0, 1);

    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    texture = TextureBMP("watermelon.bmp");

    initBox();
    initCones();
    initSpheres();
    initMirrors();

}


int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB );
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(500, 10);
    glutCreateWindow("Raytracing");

    glutDisplayFunc(display);
    initialize();

    glutMainLoop();
    return 0;
}
