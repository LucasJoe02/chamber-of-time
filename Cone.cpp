/*----------------------------------------------------------
* COSC363  Ray Tracer
*
*  The Cone class
*  This is a subclass of Object, and hence implements the
*  methods intersect() and normal().
-------------------------------------------------------------*/

#include "Cone.h"
#include <math.h>

/**
* Cone's intersection method.  The input is a ray.
*/
float Cone::intersect(glm::vec3 p0, glm::vec3 dir)
{
    float x0 = p0.x;
    float y0 = p0.y;
    float z0 = p0.z;
    float dx = dir.x;
    float dy = dir.y;
    float dz = dir.z;
    float xc = center.x;
    float yc = center.y;
    float zc = center.z;
    float h = height;
    float r = radius;

    float a = dx*dx + dz*dz - dy*dy*(r/h)*(r/h);
    float b = 2 * (dx*x0-dx*xc + dz*z0-dz*zc - (r/h)*(r/h)*(-dy*h+dy*y0-dy*yc));
    float c = -(r/h)*(r/h)*(h-y0+yc)*(h-y0+yc) + (x0-xc)*(x0-xc) + (z0-zc)*(z0-zc);

    float discriminant = b*b - 4 * a * c;
    if(discriminant < 0.001) return -1.0;    //includes zero and negative values

    float t1 = (-b - sqrt(discriminant)) / (2 * a);
    float t2 = (-b + sqrt(discriminant)) / (2 * a);

    float t1y = y0 + dy*t1;
    float t2y = y0 + dy*t2;
    float maxY = yc+h;

    if (t1 < 0 || t1y > maxY)
    {
        return (t2 > 0 && t2y < maxY) ? t2 : -1;
    }
    else return t1;

}

/**
* Returns the unit normal vector at a given point.
* Assumption: The input point p lies on the cone.
*/
glm::vec3 Cone::normal(glm::vec3 p)
{
    float alpha = atan((p.x - center.x)/(p.z - center.z));

    glm::vec3 n(sin(alpha)*cos(theta),sin(theta),cos(alpha)*cos(theta));
    return n;
}
