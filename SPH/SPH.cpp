#include "SPH.h"
#include <cmath>

SPH::SPH( AbstractObject* parent, const Geometry& container, float smoothingRadius, float viscosity, float pressure, float surfaceTension,
          unsigned int nbCellX, unsigned int nbCellY, unsigned int nbCellZ, unsigned int nbCubeX,
          unsigned int nbCubeY, unsigned int nbCubeZ, unsigned int nbParticles, float restDensity,
          float totalVolume, float maxDTime, const QVector3D& gravity )
    : AbstractObject( parent )
    , _container( container )
    , _coeffPoly6( 0 )
    , _coeffSpiky( 0 )
    , _coeffVisc( 0 )
    , _restDensity( restDensity )
    , _smoothingRadius( smoothingRadius )
    , _smoothingRadius2( smoothingRadius * smoothingRadius )
    , _viscosity( viscosity )
    , _pressure( pressure )
    , _surfaceTension( surfaceTension )
    , _maxDeltaTime( maxDTime )
    , _gravity( gravity )
    , _particles( nbParticles )
    , _grid( inflatedContainerBoundingBox(), nbCellX, nbCellY, nbCellZ, smoothingRadius )
    , _marchingTetrahedra( inflatedContainerBoundingBox(), nbCubeX, nbCubeY, nbCubeZ )
    , _renderMode( RenderParticles )
    , _material( QColor( 128, 128, 128, 255 ) )
{
    initializeCoefficients();
    initializeParticles( totalVolume );
}

SPH::~SPH()
{
}

void SPH::animate( const TimeState& timeState )
{
    float deltaTime = timeState.deltaTime();

    // Clamp 'dt' to avoid instabilities
    if ( deltaTime > _maxDeltaTime )
        deltaTime = _maxDeltaTime;

    computeDensities();
    computeForces();
    moveParticles( deltaTime );
}

void SPH::render( GLShader& shader )
{
    shader.setMaterial( _material );

    switch( _renderMode )
    {
        case RenderParticles : _particles.render( globalTransformation(), shader ); break;
        case RenderImplicitSurface : _marchingTetrahedra.render( globalTransformation(), shader, *this ); break;
    }
}

void SPH::changeRenderMode()
{
    if ( _renderMode == RenderParticles )
        _renderMode = RenderImplicitSurface;
    else
        _renderMode = RenderParticles;
}

void SPH::changeMaterial()
{
    if ( _material.refractiveIndex() == 1 )
        _material = Material( 1.33 );
    else
        _material = Material( QColor( 128, 128, 128, 255 ) );
}

void SPH::resetVelocities()
{
    for ( int i=0 ; i<_particles.size() ; ++i )
        _particles[i].setVelocity( QVector3D() );
}

BoundingBox SPH::inflatedContainerBoundingBox() const
{
    BoundingBox boundingBox = _container.boundingBox();
    QVector3D center = ( boundingBox.minimum() + boundingBox.maximum() ) * 0.5;

    return BoundingBox( ( boundingBox.minimum() - center ) * 1.2 + center,
                        ( boundingBox.maximum() - center ) * 1.2 + center );
}

void SPH::initializeCoefficients()
{
	// H^n
    float h6 = _smoothingRadius2 * _smoothingRadius2 * _smoothingRadius2;
    float h9 = h6 * _smoothingRadius2 * _smoothingRadius;

	// Poly6
    _coeffPoly6 = 315.0 / ( 64.0 * M_PI * h9 );

	// Spiky
    _coeffSpiky = 3.0 * 15.0 / ( M_PI * h6 );

	// Visc
    _coeffVisc = 45.0 / ( M_PI * h6 );
}

void SPH::initializeParticles( float totalVolume )
{
    float totalMass = totalVolume * _restDensity;
    float mass = totalMass / _particles.size();

    // set particle position and mass
    for ( int i=0 ; i<_particles.size() ; ++i )
	{
        _particles[i].setMass( mass );
        _particles[i].setDensity( _restDensity );
        _particles[i].setVolume( mass / _restDensity );
        _particles[i].setPosition( _container.randomInteriorPoint() );
        _particles[i].setCellIndex( _grid.cellIndex( _particles[i].position() ) );

        _grid.addParticle( _particles[i].cellIndex(), i );
    }
}

float SPH::densityKernel( float r2 ) const
{
    float diff = _smoothingRadius2 - r2;

    return _coeffPoly6 * diff * diff * diff;
}

float SPH::densitykernelGradient( float r2 ) const
{
    float diff = _smoothingRadius2 - r2;

    return -3 * _coeffPoly6 * diff * diff;
}

float SPH::pressureKernel( float r ) const
{
    if ( r == 0 )
		return 0;

    float diff = _smoothingRadius - r;

    return _coeffSpiky * diff * diff / r;
}

float SPH::viscosityKernel( float r ) const
{
    return _coeffVisc * ( _smoothingRadius - r );
}

float SPH::pressure( float density ) const
{
    return density / _restDensity - 1;
}

void SPH::computeDensities()
{
    // For each particle
    #pragma omp parallel for schedule( guided )
    for ( int i=0 ; i<_particles.size() ; ++i )
	{
        float density = 0;
        float correction = 0;
        Particle& particle = _particles[i];
        const QVector<unsigned int>& neighborhood = _grid.neighborhood( particle.cellIndex() );

		// For each neighbor cell
        for ( int j=0 ; j<neighborhood.size() ; ++j )
		{
            const QVector<unsigned int>& neighbors = _grid.cellParticles( neighborhood[j] );

			// For each particle in a neighboring cell
            for ( int k=0 ; k<neighbors.size() ; ++k )
			{
                const Particle& neighbor = _particles[neighbors[k]];
                QVector3D difference = particle.position() - neighbor.position();
                float r2 = difference.lengthSquared();

				// If the neighboring particle is inside a sphere of radius 'h'
                if ( r2 < _smoothingRadius2 )
				{
					// Add density contribution
                    float kernelMass = densityKernel( r2 ) * neighbor.mass();
                    density += kernelMass;
                    correction += kernelMass / neighbor.density();
				}
			}
		}

        particle.setDensity( density / correction );
        particle.setVolume( particle.mass() / particle.density() );
        particle.setPressure( pressure( density ) );
    }
}

void SPH::computeForces()
{
	// Compute gravity vector
    QVector3D gravity = localTransformation().inverted().mapVector( _gravity );

    // For each particle
    #pragma omp parallel for schedule( guided )
    for ( int i=0 ; i<_particles.size() ; ++i )
	{
        QVector3D pressureForce;
        QVector3D viscosityForce;
        QVector3D tensionForce;
        float correction = 0;

        Particle& particle = _particles[i];
        const QVector<unsigned int>& neighborhood = _grid.neighborhood( particle.cellIndex() );

		// For each neighbor cell
        for ( int j=0 ; j<neighborhood.size() ; ++j )
		{
            const QVector<unsigned int>& neighbors = _grid.cellParticles( neighborhood[j] );

			// For each particle in a neighboring cell
            for ( int k=0 ; k<neighbors.size() ; ++k )
			{
                const Particle& neighbor = _particles[neighbors[k]];
                QVector3D difference = particle.position() - neighbor.position();
                float r2 = difference.lengthSquared();

				// If the neighboring particle is inside a sphere of radius 'h'
                if ( r2 < _smoothingRadius2 )
				{
                    float r = ::sqrt( r2 );
                    float volume = neighbor.volume();
                    float meanPressure = ( neighbor.pressure() + particle.pressure() ) * 0.5;

					// Add forces contribution
                    pressureForce -= difference * ( pressureKernel( r ) * meanPressure * volume );
                    viscosityForce += ( neighbor.velocity() - particle.velocity() ) * ( viscosityKernel( r ) * volume );

                    float kernelRR = densityKernel( r2 );
                    tensionForce += difference * kernelRR; // * Mass_b / Mass_a, but in our case, this equals 1
                    correction += kernelRR * volume;
				}
			}
		}

		// Normalize results and apply uniform coefficients;
        pressureForce *= _pressure / correction;
        viscosityForce *= _viscosity / correction;
        tensionForce *= _surfaceTension / correction;

        // Compute the sum of all forces and convert it to an acceleration
        particle.setAcceleration( ( viscosityForce - pressureForce - tensionForce ) / particle.density() + gravity );
    }
}

void SPH::moveParticles( float deltaTime )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Pour chaque particule, utilisez son accélération
    // précédemment obtenue pour calculer sa nouvelle
    // vélocité puis sa nouvelle position à l'aide
    // de la méthode d'intégration semi-explite d'Euler.
    //
    // Vous devrez aussi vous assurer qu'il n'y a pas
    // d'intersection avec la paroi (_container) du
    // contenant. S'il y a intersection, corriger la
    // vélocité et la position en conséquence.
    //
    // Une fois que la particule a atteint sa destination
    // finale, vérifiez si la particule a changé de
    // cellule de la grille régulière à l'aide de la
    // classe 'Grid'. Si oui, changez-la de cellule avec
    // les méthodes 'removeParticle' et 'addParticle' et
    // mettez à jour son index.
    ////////////////////////////////////////////////////

    for(unsigned int particleId = 0, particleIdMax = _particles.size(); particleId < particleIdMax; ++particleId)
    {
        Particle& particle          = _particles[particleId];
        QVector3D currentPosition   = particle.position();
        QVector3D nextSpeed         = particle.velocity() + (deltaTime * particle.acceleration());
        QVector3D nextPosition      = currentPosition + (deltaTime * nextSpeed);

       /* Below, we'll check for collision with container. */

        QVector3D totalDisplacement     = nextPosition - currentPosition;
        QVector3D remainingDisplacement = totalDisplacement;

        Intersection collisionDetectionIntersection;
        QVector3D collisionDetectionNormal;
        QVector3D collisionDetectionPosition    = currentPosition;
        QVector3D collisionDetectionDirection   = totalDisplacement.normalized();
        Ray       collisionDetectionRay(collisionDetectionPosition, collisionDetectionDirection);

        while(_container.intersect(collisionDetectionRay, collisionDetectionIntersection))
        {
            if((collisionDetectionDirection * collisionDetectionIntersection.rayParameterT()).length() >= remainingDisplacement.length())
            {
                /* If the collision is further away than the new position, let's break, otherwise we'll get in an endless loop. */

                break;
            }

            collisionDetectionPosition  = collisionDetectionIntersection.position();
            collisionDetectionNormal    = collisionDetectionIntersection.normal();

            /* Let's remove the displacement inside the container. */
            remainingDisplacement       = nextPosition - collisionDetectionPosition;

            /* Let's remove the component in the direction of the collision. */
            totalDisplacement           = remainingDisplacement
                - (QVector3D::dotProduct(remainingDisplacement, collisionDetectionNormal) * collisionDetectionNormal);
            nextSpeed                  -= QVector3D::dotProduct(nextSpeed, collisionDetectionNormal) * collisionDetectionNormal;

            /* Let's move a tiny bit away from the edge (epsilon). */
            collisionDetectionPosition -= collisionDetectionNormal * 0.0003;

            nextPosition                = collisionDetectionPosition + totalDisplacement;

            /* Let's update variables for collision detection. */
            collisionDetectionDirection = totalDisplacement.normalized();
            collisionDetectionRay       = Ray(collisionDetectionPosition, collisionDetectionDirection);
        }

        /* Let's update speed and position. */
        particle.setVelocity(nextSpeed);
        particle.setPosition(nextPosition);

        /* Let's update cell if required. */
        unsigned int currentCellId  = particle.cellIndex();
        unsigned int nextCellId     = _grid.cellIndex(nextPosition);

        if (currentCellId != nextCellId)
        {
            particle.setCellIndex(nextCellId);

            _grid.removeParticle(currentCellId, particleId);
            _grid.addParticle(nextCellId, particleId);
        }
    }
}

void SPH::surfaceInfo( const QVector3D& position, float& value, QVector3D& normal )
{
    ////////////////////////////////////////////////////
    // IFT3355 - À compléter
    //
    // Vous devez calculer la valeur de la fonction 'f'
    // et l'approxmation de la normale à la surface au
    // point 'position'. Cette fonction est appelée
    // par la classe 'MarchingTetrahedra' à chaque sommet
    // de sa grille.
    //
    // Basez-vous sur les calculs tels que 'computeDensities'
    // et 'computeForces' pour savoir comment accéder aux
    // particules voisines.
    ////////////////////////////////////////////////////

    /* These variables will be incremented with values calculated based on nearby particles. */
    float density   = 0;
    float gradientX = 0;
    float gradientY = 0;
    float gradientZ = 0;

    float positionX  = position.x();
    float positionY  = position.y();
    float positionZ  = position.z();

    const QVector<unsigned int>& cells = _grid.neighborhood(_grid.cellIndex(position));

    /* Let's loop through nearby cells. */
    for (unsigned int cellId = 0, cellMax = cells.size(); cellId < cellMax; ++cellId)
    {
        const QVector<unsigned int>& cellParticles = _grid.cellParticles(cells[cellId]);

        /* Let's loop through particles in the current nearby cell. */
        for (unsigned int particleId = 0, particleIdMax = cellParticles.size(); particleId < particleIdMax; ++particleId)
        {
            const Particle&  particle           = _particles[cellParticles[particleId]];
            const QVector3D& particlePosition   = particle.position();
            float            particleMass       = particle.mass();
            float            squaredDistance    = (position - particlePosition).lengthSquared();

            if (squaredDistance < _smoothingRadius2)
            {
                /* Nearby particle is inside radius, let's adjust density and normal components. */

                density += particleMass * densityKernel(squaredDistance);

                /* Let's calculate the gradient. We have to calculate the derivative of the
                 * density for x/y/z components separately.
                 *
                 * We defined the implicit function as f(p) = P(p)/P0 - (1 - a) where P is
                 * the density function and P0 is the resting density.
                 *
                 * The gradient of f, noted \/f, is (df/dx, df/dy, df/dz) where 'd' should be
                 * seen as the Greek letter delta to represent a partial derivative. Let's now
                 * work only with 'x' to simplify. Also, we'll forget about the summation to
                 * focus on a single nearby particle. Let's not forget that the derivative of a
                 * summation is a summation of derivates.
                 *
                 * The derivative of 'f' for one nearby particle becomes:
                 *     df/dx = d[(1/P0) * m * W((p - pi)^2) - (1 - a)]/dx
                 *         where 'm' is the mass of the nearby particle, 'p' is the position passed
                 *         to the method and 'pi' is the position of the nearby particle.
                 *
                 *     Let's rewrite 'p - pi':
                 *     df/dx = d[(1/P0) * m * W((x - xi)^2 + (y - yi)^2 + (z - zi)^2) - (1 - a)]/dx
                 *         where 'x', 'y', 'z' are components of the position passed to the method
                 *         and 'xi', 'yi', 'zi' are components of the nearby particle position.
                 *
                 *     Let's apply derivative rules:
                 *     df/dx = d[(1/P0) * m * W((x - xi)^2 + (y - yi)^2 + (z - zi)^2)]/dx - d[(1 - a)]/dx
                 *           = d[(1/P0) * m * W((x - xi)^2 + (y - yi)^2 + (z - zi)^2)]/dx - 0
                 *           = d[(1/P0) * m * W((x - xi)^2 + (y - yi)^2 + (z - zi)^2)]/dx
                 *           = (1/P0) * m * d[W((x - xi)^2 + (y - yi)^2 + (z - zi)^2)]/dx
                 *
                 *     Here, we can apply the chain rule: f(g(x)) = f'(g(x)) * g'(x)
                 *         - 'f(...)' = 'W(...)'
                 *         - 'g(x)'   = (x - xi)^2 + (y - yi)^2 + (z - zi)^2
                 *
                 *     We already have W'(...), it's densitykernelGradient()
                 *
                 *     So we get:
                 *           densitykernelGradient((x - xi)^2 + (y - yi)^2 + (z - zi)^2) * g'(x)
                 *           = densitykernelGradient((x - xi)^2 + (y - yi)^2 + (z - zi)^2) * 2 * (x - xi)
                 *           (because '(y - yi)^2 + (z - zi)^2' is considered as a constant and its
                 *           derivative is 0)
                 *
                 *     Finally:
                 *     df/dx = (1/P0) * m * densitykernelGradient((x - xi)^2 + (y - yi)^2 + (z - zi)^2) * 2 * (x - xi)
                 */
                float common    = particleMass * densitykernelGradient(squaredDistance);
                gradientX      -= 2 * (positionX - particlePosition.x()) * common;
                gradientY      -= 2 * (positionY - particlePosition.y()) * common;
                gradientZ      -= 2 * (positionZ - particlePosition.z()) * common;
            }
        }
    }

    /* We take (1/P0) out of the summation and do it here. */
    normal.setX(gradientX / _restDensity);
    normal.setY(gradientY / _restDensity);
    normal.setZ(gradientZ / _restDensity);

    normal.normalize();

    float a = 0.3;
    value   = density / _restDensity - (1 - a);
}
