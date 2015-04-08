#include "MarchingTetrahedra.h"
#include <QtOpenGL>

MarchingTetrahedra::MarchingTetrahedra( const BoundingBox& boundingBox, unsigned int nbCubeX, unsigned int nbCubeY, unsigned int nbCubeZ )
    : _boundingBox( boundingBox )
{
    QVector3D boxExtent = boundingBox.maximum() - boundingBox.minimum();

    _nbCubes[0] = nbCubeX;
    _nbCubes[1] = nbCubeY;
    _nbCubes[2] = nbCubeZ;
    _cubeSize[0] = boxExtent.x() / _nbCubes[0];
    _cubeSize[1] = boxExtent.y() / _nbCubes[1];
    _cubeSize[2] = boxExtent.z() / _nbCubes[2];

    // Allocate vertex value and position vector
    unsigned int nbCubes = ( nbCubeX + 1 ) * ( nbCubeY + 1 ) * ( nbCubeZ + 1 );
    _vertexValues.resize( nbCubes );
    _vertexNormals.resize( nbCubes );
    _vertexPositions.resize( nbCubes );

    computeVertexPositions();
}

void MarchingTetrahedra::render( const QMatrix4x4& transformation, GLShader& shader, ImplicitSurface& implicitSurface )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Ceci est la fonction d'entrée pour débuter le
    // calcul de la surface implicite. Il s'agit de
    // calculer les valeurs et les normales aux sommets
    // (computeVertexInfo) et de faire le rendu de chacun
    // des cubes de la grille (renderCube).
    //
    // Le nombre de cubes en:
    // x: _nbCubes[0]
    // y: _nbCubes[1]
    // z: _nbCubes[2]
    ////////////////////////////////////////////////////

    computeVertexInfo(implicitSurface);

    _nbGLVertices = 0;

    for (unsigned int z = 0, zMax = _nbCubes[2]; z < zMax; ++z)
    {
        for (unsigned int y = 0, yMax = _nbCubes[1]; y < yMax; ++y)
        {
            for (unsigned int x = 0, xMax = _nbCubes[0]; x < xMax; ++x)
            {
                /* Let's render each triangle. */

                renderCube(x, y, z);
            }
        }
    }

    renderTriangles( transformation, shader );
}

void MarchingTetrahedra::computeVertexPositions()
{
    unsigned int currentVertex = 0;

	// Precompute the position of each vertex of the grid
    for ( unsigned int z=0 ; z<_nbCubes[2]+1 ; ++z )
        for ( unsigned int y=0 ; y<_nbCubes[1]+1 ; ++y )
            for ( unsigned int x=0 ; x<_nbCubes[0]+1 ; ++x, ++currentVertex )
                _vertexPositions[currentVertex] = vertexPosition( x, y, z );
}

void MarchingTetrahedra::computeVertexInfo( ImplicitSurface& implicitSurface )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Pour chaque sommet de la grille, remplir les
    // variables membres '_vertexValues' et '_vertexNormals'
    // à l'aide de la position du vertex '_vertexPositions'
    // et de la classe 'implicitSurface'.
    //
    // Les tableaux sont indexés par un seul nombre. Utilisez
    // 'vertexIndex' ou incrémentez une variable manuellement.
    // Le nombre de sommets en:
    // x: '_nbCubes[0]+1'
    // y: '_nbCubes[1]+1'
    // z: '_nbCubes[2]+1'
    // Si vous utilisez une variable que vous incrémentez
    // manuellement, portez bien attention à l'ordre d'imbrication
    // des boucles pour qu'elles correspondent bien à la
    // la fonction 'vertexIndex'.
    ////////////////////////////////////////////////////

    for (unsigned int z = 0, zMax = _nbCubes[2]; z < zMax; ++z)
    {
        for (unsigned int y = 0, yMax = _nbCubes[1]; y <yMax; ++y)
        {
            for (unsigned int x = 0, xMax = _nbCubes[0]; x < xMax; ++x)
            {
                unsigned int index = vertexIndex(x, y, z);

                implicitSurface.surfaceInfo(_vertexPositions[index], _vertexValues[index], _vertexNormals[index]);
            }
        }
    }
}

void MarchingTetrahedra::renderCube( unsigned int x, unsigned int y, unsigned int z )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Divisez votre cube en six tétraèdres en utilisant
    // les sommets du cube, et faire appel à 'renderTetrahedron'
    // pour le rendu de chacun d'eux. Il faut calculer l'index
    // de chaque sommet, et non leurs valeurs (x,y,z) entières.
    ////////////////////////////////////////////////////


    /*   y: bottom
        y1: top
         x: left
        x1: right
         z: front
        z1: rear */

    unsigned int x1 = x + 1;
    unsigned int y1 = y + 1;
    unsigned int z1 = z + 1;

    /* Bottom, left */
    unsigned int bottomLeftFront    = vertexIndex(x, y, z);
    unsigned int bottomLeftRear     = vertexIndex(x, y, z1);

    /* Bottom, right */
    unsigned int bottomRightFront   = vertexIndex(x1, y, z);
    unsigned int bottomRightRear    = vertexIndex(x1, y, z1);

    /* Top, left */
    unsigned int topLeftFront   = vertexIndex(x, y1, z);
    unsigned int topLeftRear    = vertexIndex(x, y1, z1);

    /* Top, right */
    unsigned int topRightFront  = vertexIndex(x1, y1, z);
    unsigned int topRightRear   = vertexIndex(x1, y1, z1);

    /* Let's divide the cube in 6 tetrahedons. 'bottomLeftFront' and 'topRightRear' are common to all 6 tetrahedrons. */
    renderTetrahedron(bottomLeftFront,  topLeftFront,       topRightFront,      topRightRear);
    renderTetrahedron(bottomLeftFront,  bottomRightFront,   topRightFront,      topRightRear);
    renderTetrahedron(bottomLeftFront,  topLeftFront,       topLeftRear,        topRightRear);
    renderTetrahedron(bottomLeftFront,  bottomLeftRear,     topLeftRear,        topRightRear);
    renderTetrahedron(bottomLeftFront,  bottomLeftRear,     bottomRightRear,    topRightRear);
    renderTetrahedron(bottomLeftFront,  bottomRightFront,   bottomRightRear,    topRightRear);
}

void MarchingTetrahedra::renderTetrahedron( unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4 )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // En utilisant les valeurs aux sommets, voyez dans
    // quel cas de rendu vous vous trouvez. Faites appel
    // à 'renderTriangle' ou 'renderQuad' dépendant du cas
    // et réordonnancez les sommets pour que le (ou les)
    // sommet de signe différent soit au début. Les noms
    // de paramètres 'in' et 'out' ne sont que des indicateurs
    // pour différencier les signes, et non un indicateur
    // absolu de ce qui est à l'intérieur du liquide ou
    // à l'extérieur.
    ////////////////////////////////////////////////////

    /* Below, we'll check signs. We'll use boolean to express the sign of a variable:
     *     - true if > 0
     *     - false otherwise */
    bool isVertex1Positive  = (_vertexValues[p1] > 0);
    bool isVertex2Positive  = (_vertexValues[p2] > 0);
    bool isVertex3Positive  = (_vertexValues[p3] > 0);
    bool isVertex4Positive  = (_vertexValues[p4] > 0);

    /* Let's create some variables which appear more than once in 'if' conditions below. Please note
     * that we do not need to create variables like 'is2Not1' because it is the same as 'is1Not2'. */
    bool is1Not2    = (isVertex1Positive != isVertex2Positive);
    bool is1Not3    = (isVertex1Positive != isVertex3Positive);
    bool is1Not4    = (isVertex1Positive != isVertex4Positive);

    bool is2Not3    = (isVertex2Positive != isVertex3Positive);
    bool is2Not4    = (isVertex2Positive != isVertex4Positive);

    bool is3Not4    = (isVertex3Positive != isVertex4Positive);

    /* The four next 'if' are for tetrahedrons: if one sign if different from three ofther signs. */
    if(is1Not2 && is1Not3 && is1Not4)
    {
        /* 1 != 2 ; 1 != 3 ; 1 != 4*/

        renderTriangle(p1, p2, p3, p4);
    }
    else if(is1Not2 && is2Not3 && is2Not4)
    {
        /* 2 != 1 ; 2 != 3 ; 2 != 4 */

        renderTriangle(p2, p1, p3, p4);
    }
    else if(is1Not3 && is2Not3 && is3Not4)
    {
        /* 3 != 1 ; 3 != 2 ; 3 != 4 */

        renderTriangle(p3, p1, p2, p4);
    }
    else if(is1Not4 && is2Not4 && is3Not4)
    {
        /* 4 != 1 ; 4 != 2 ; 4 != 3 */

        renderTriangle(p4, p1, p2, p3);
    }

    /* Now we check if two signs are different from the remaining two other signs. If so, we create a "quad". */

    else if((isVertex1Positive == isVertex2Positive) && is1Not3 && (isVertex3Positive == isVertex4Positive))
    {
        /* 1 == 2 ; 1 != 3 ; 3 == 4 */

        renderQuad(p1, p2, p3, p4);
    }
    else if((isVertex1Positive == isVertex3Positive) && is1Not2 && (isVertex2Positive == isVertex4Positive))
    {
        /* 1 == 3 ; 1 != 2 ; 2 == 4 */

        renderQuad(p1, p3, p2, p4);
    }
    else if((isVertex1Positive == isVertex4Positive) && is1Not2 && (isVertex2Positive == isVertex3Positive))
    {
        /* 1 == 4 ; 1 != 2 ; 2 == 3 */

        renderQuad(p1, p4, p2, p3);
    }

    /* 1 == 2 == 3 == 4 => don't draw */
}

void MarchingTetrahedra::renderTriangle( unsigned int in1, unsigned int out2, unsigned int out3, unsigned int out4 )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Calculez l'interpolation des valeurs et des normales
    // pour les arêtes dont les sommets sont de signes
    // différents. N'oubliez pas de normaliser vos normales.
    //
    // Le triangle n'est pas rendu tout de suite. Il vous
    // faudra l'ajouter à la liste des triangles affichés
    // en utilisant la méthode 'addTriangle'
    ////////////////////////////////////////////////////

    QVector3D vectorIn   = _vertexPositions[in1];
    QVector3D normalIn   = _vertexNormals[in1];
    float valueIn        = _vertexValues[in1];

    /* Below, we interpolate vectors and normals accordings to the implicite surface values at specified vertices. */

    QVector3D point1    = interpolatePosition(vectorIn, _vertexPositions[out2], valueIn, _vertexValues[out2]);
    QVector3D point2    = interpolatePosition(vectorIn, _vertexPositions[out3], valueIn, _vertexValues[out3]);
    QVector3D point3    = interpolatePosition(vectorIn, _vertexPositions[out4], valueIn, _vertexValues[out4]);

    QVector3D normal1    = interpolatePosition(normalIn, _vertexNormals[out2], valueIn, _vertexValues[out2]);
    QVector3D normal2    = interpolatePosition(normalIn, _vertexNormals[out3], valueIn, _vertexValues[out3]);
    QVector3D normal3    = interpolatePosition(normalIn, _vertexNormals[out4], valueIn, _vertexValues[out4]);

    normal1.normalize();
    normal2.normalize();
    normal3.normalize();

    addTriangle(point1, point2, point3, normal1, normal2, normal3);

}

void MarchingTetrahedra::renderQuad( unsigned int in1, unsigned int in2, unsigned int out3, unsigned int out4 )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Calculer l'interpolation des valeurs et des normales
    // pour les arêtes dont les sommets sont de signes
    // différents. Vous aurez quatre sommets. Séparez le
    // quadrilatère en deux triangles et les ajouter à
    // la liste avec 'addTriangle'.
    ////////////////////////////////////////////////////

    QVector3D vectorIn1     = _vertexPositions[in1];
    QVector3D vectorIn2     = _vertexPositions[in2];
    QVector3D vectorOut3    = _vertexPositions[out3];
    QVector3D vectorOut4    = _vertexPositions[out4];

    QVector3D normalIn1     = _vertexNormals[in1];
    QVector3D normalIn2     = _vertexNormals[in2];
    QVector3D normalOut3    = _vertexNormals[out3];
    QVector3D normalOut4    = _vertexNormals[out4];

    float valueIn1  = _vertexValues[in1];
    float valueIn2  = _vertexValues[in2];
    float valueOut3 = _vertexValues[out3];
    float valueOut4 = _vertexValues[out4];

    /* Below, we interpolate vectors and normals accordings to the implicite surface values at specified vertices. */

    QVector3D point1    = interpolatePosition(vectorIn1, vectorOut3, valueIn1, valueOut3);
    QVector3D point2    = interpolatePosition(vectorIn1, vectorOut4, valueIn1, valueOut4);
    QVector3D point3    = interpolatePosition(vectorIn2, vectorOut3, valueIn2, valueOut3);
    QVector3D point4    = interpolatePosition(vectorIn2, vectorOut4, valueIn2, valueOut4);

    QVector3D normal1    = interpolatePosition(normalIn1, normalOut3, valueIn1, valueOut3);
    QVector3D normal2   = interpolatePosition(normalIn1, normalOut4, valueIn1, valueOut4);
    QVector3D normal3    = interpolatePosition(normalIn2, normalOut3, valueIn2, valueOut3);
    QVector3D normal4    = interpolatePosition(normalIn2, normalOut4, valueIn2, valueOut4);

    normal1.normalize();
    normal2.normalize();
    normal3.normalize();
    normal4.normalize();

    /* Let's split the "quad" into two triangles. */

    addTriangle(point1, point2, point3, normal1, normal2, normal3);
    addTriangle(point2, point3, point4, normal2, normal3, normal4);
}

QVector3D MarchingTetrahedra::interpolatePosition(const QVector3D& vector1, const QVector3D& vector2, float value1, float value2) const
{
    /* Here we approximate the point between 'vector1' and 'vector2' where the implicit surface value would be zero.
     *
     * Let's only use 'x' coordinates for an example.
     *
     * Let's say vector1 is at x=4 and vector2 is a x=-9.
     * Let's also say that value1=-1 and value2=5
     *
     * We have deltaX = vector2 - vector1 = -9 - 4 = - 13
     * We have deltaValue = value1 - value2 = -1 - 5 = -6
     *
     * If we substract value1 to itself, we necessarily get value=0. So that's why we use it in the denomiator of the fraction below.
     *
     * We must be careful with signs as we must go from somewhere between vector1 and vector2. By using 'vector2 - vector1' for deltaX
     * and 'value1 - value2' (and not 'value2 - value1') for deltaValue, we either get +/+ or -/- with only returns a positive number.
     * Then only deltaX is responsible to add a positive value or a negative value (subtraction).
     *
     * Let's take out example: value1 / deltaValue = -1/-6 = 1/6 (positive)
     * We now that we must go from x=4 to somewhere between that point and x=-9, so we must multiply the fraction above (1/6) to something
     * negative. Here, deltaX = -13 is negative, so problem solved.
     *
     * If signs were reversed, we would get deltaX = 9 - (-4) = 13 and deltaValue = 1 - (-5) = 6 (both were negative, both are now positive).
     * So we get -4 + (1 / 6) * 13, so we go from x=-4 to somewhere between that point and x=9.
     *
     * Without demonstrating it, the same thing happens if values and/or vector coordinates have the same sign. */

    return vector1 + (value1 / (value1 - value2)) * (vector2 - vector1);
}

QVector3D MarchingTetrahedra::vertexPosition( unsigned int x, unsigned int y, unsigned int z ) const
{
    return _boundingBox.minimum() + QVector3D( x * _cubeSize[0], y * _cubeSize[1], z * _cubeSize[2] );
}

unsigned int MarchingTetrahedra::vertexIndex( unsigned int x, unsigned int y, unsigned int z ) const
{
    return z * ( _nbCubes[0] + 1 ) * ( _nbCubes[1] + 1 ) + y * ( _nbCubes[0] + 1 ) + x;
}

void MarchingTetrahedra::addTriangle( const QVector3D& p0, const QVector3D& p1, const QVector3D& p2,
                                      const QVector3D& n0, const QVector3D& n1, const QVector3D& n2 )
{
    if ( _glVertices.size() <= _nbGLVertices )
    {
        _glVertices.resize( _glVertices.size() + 192 );
        _glNormals.resize( _glNormals.size() + 192 );
    }

    _glVertices[_nbGLVertices+0] = p0;
    _glVertices[_nbGLVertices+1] = p1;
    _glVertices[_nbGLVertices+2] = p2;
    _glNormals[_nbGLVertices+0] = n0;
    _glNormals[_nbGLVertices+1] = n1;
    _glNormals[_nbGLVertices+2] = n2;
    _nbGLVertices += 3;
}

void MarchingTetrahedra::renderTriangles( const QMatrix4x4& transformation, GLShader& shader )
{
    shader.setGlobalTransformation( transformation );

    shader.enableVertexAttributeArray();
    shader.enableNormalAttributeArray();
    shader.setVertexAttributeArray( _glVertices.data() );
    shader.setNormalAttributeArray( _glNormals.data() );

    glDrawArrays( GL_TRIANGLES, 0, _nbGLVertices );

    shader.disableVertexAttributeArray();
    shader.disableNormalAttributeArray();
}
