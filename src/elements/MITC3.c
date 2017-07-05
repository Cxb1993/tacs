#include "MITC3.h"
#include "TACSElementAlgebra.h"
#include "FElibrary.h"

/*
  Rigid-body dynamics routines for TACS
  
  Copyright (c) 2015-2017 Graeme Kennedy. All rights reserved. 
*/

/*
  Write the relative error for components of a vector for a
  finite-difference or complex-step study to a given file

  input:
  fp:         the output file 
  descript:   description of the component
  a:          analytic values
  b:          set of finite-difference/complex-step values
  size:       the number of values
  rel_err:    relative error tolerance
*/
static void writeErrorComponents( FILE *fp, const char *descript,
                                  TacsScalar *a, TacsScalar *fd, 
                                  int size, double rel_err=0.0 ){
  int print_flag = 1;
  for ( int i = 0; i < size; i++ ){
    double rel = 0.0;
    if (a[i] != 0.0){
      rel = fabs(TacsRealPart((a[i] - fd[i])/a[i]));
    }
    else {
      rel = fabs(TacsRealPart((a[i] - fd[i])));
    }

    if (rel > rel_err || a[i] != a[i] || fd[i] != fd[i]){
      if (print_flag){
        fprintf(fp, "%*s[   ] %15s %15s %15s\n",
                (int)strlen(descript), "Val", "Analytic", 
                "Approximate", "Rel. Error");
        print_flag = 0;
      }
      if (a[i] != 0.0){
        fprintf(fp, "%s[%3d] %15.6e %15.6e %15.4e\n", 
                descript, i, TacsRealPart(a[i]), TacsRealPart(fd[i]), 
                fabs(TacsRealPart((a[i] - fd[i])/a[i])));
      }
      else {
        fprintf(fp, "%s[%3d] %15.6e %15.6e\n", 
                descript, i, TacsRealPart(a[i]), TacsRealPart(fd[i]));
      }
    }
  }  
}

/* 
   Return the number of displacements
*/
int MITC3::numDisplacements(){ return NUM_DISPS; }

/*
  Return the number of stresses
*/
int MITC3::numStresses(){ return NUM_STRESSES; }
  
/*
  Return the number of extras
*/
int MITC3::numExtras(){ return NUM_EXTRAS; }

/*
  Return the number of FE nodes
*/
int MITC3::numNodes(){ return NUM_NODES; }

/*
  Return the ElementType
*/
ElementType MITC3::getElementType(){ return TACS_SHELL; }

/* 
   Set up the internal static data for the names of the element,
   displacements, stresses, strains and extra variables, respectively.
*/
const char * MITC3::elemName = "MITC3";
  
const char * MITC3::dispNames[] = { "u0", "v0", "w0", 
                                    "eta", "epsx", "epsy", "epsz", "lam" };
  
const char * MITC3::stressNames[] = { "sx0", "sx1", "sy1", 
                                      "st0", "sxy0", "sxz0" };

const char * MITC3::strainNames[] = { "sx0", "sx1", "sy1", 
                                      "st0", "sxy0", "sxz0" };
  
const char * MITC3::extraNames[] = { "lambda", "buckling",
                                     "dv1", "dv2" };

/*
  Returns the elementName
*/
const char * MITC3::elementName(){ 
  return elemName;
}

/*
  Returns the displacement names
*/
const char * MITC3::displacementName( int i ){
  if (i >= 0 && i < NUM_DISPS){
    return dispNames[i];
  }
  return "";
}

/*
  Returns the name of the stresses
*/
const char * MITC3::stressName( int i ){ 
  if (i >= 0 && i < NUM_STRESSES){
    return stressNames[i];
  }
  return "";
}

/*
  Returns the name of the strains
*/
const char * MITC3::strainName( int i ){
  if (i >= 0 && i < NUM_STRESSES){
    return strainNames[i];
  }
  return "";
}

/*
  Returns the extra names
*/
const char * MITC3::extraName( int i ){
  if (i >= 0 && i < NUM_EXTRAS){
    return extraNames[i];
  }
  return "";
}

/*
  Set the design variable values
*/
void MITC3::setDesignVars( const TacsScalar dvs[], int numDVs ){
  stiff->setDesignVars(dvs, numDVs);
}

/*
  Get the design variable values
*/
void MITC3::getDesignVars( TacsScalar dvs[], int numDVs ){
  stiff->getDesignVars(dvs, numDVs);
}

/*
  Get the design variable range
*/
void MITC3::getDesignVarRange( TacsScalar lb[], 
                               TacsScalar ub[], int numDVs ){
  stiff->getDesignVarRange(lb, ub, numDVs);
}

/*
  Evaluate the shape functions of the element given the u-coordinate
  of the point

  input:
  u:   the parametric coordinate
  
  output:
  N:   the shape functions
  Na:   the derivative of the shape functions w.r.t. u
*/
static inline void computeShapeFunc( const double u,
                                     double N[],
                                     double Na[] ){
  // Compute the shape functions and their derivatives
  N[0] = -0.5*u*(1.0 - u);
  N[1] = (1.0 - u)*(1.0 + u);
  N[2] = 0.5*(1.0 + u)*u;

  Na[0] = -0.5 + u;
  Na[1] = -2.0*u;
  Na[2] = 0.5 + u;
}

/*
  Compute the inner product of a set of shape functions (or
  their derivative) with a 3*NUM_NODES array of nodes, variables
  etc.

  input:
  Na:   the shape function
  X:    the variables 

  ouput:
  Xa:   the inner product = Na^{T}*X
*/
static inline void innerProduct( const double Na[],
                                 const TacsScalar X[],
                                 TacsScalar Xa[] ){
  Xa[0] = (Na[0]*X[0] + Na[1]*X[3] + Na[2]*X[6]);
  Xa[1] = (Na[0]*X[1] + Na[1]*X[4] + Na[2]*X[7]);
  Xa[2] = (Na[0]*X[2] + Na[1]*X[5] + Na[2]*X[8]);
}

/*
  Compute the inner product of a set of shape functions (or
  their derivative) with a 8*NUM_NODES array of nodes, variables
  etc.

  input:
  Na:   the shape function
  X:    the variables 

  ouput:
  Xa:   the inner product = Na^{T}*X
*/
static inline void innerProduct8( const double Na[],
                                  const TacsScalar X[],
                                  TacsScalar Xa[] ){
  Xa[0] = (Na[0]*X[0] + Na[1]*X[8] + Na[2]*X[16]);
  Xa[1] = (Na[0]*X[1] + Na[1]*X[9] + Na[2]*X[17]);
  Xa[2] = (Na[0]*X[2] + Na[1]*X[10] + Na[2]*X[18]);
}

/*
  Compute the normal implied by the interpolation of the normal at
  each node to an arbitrary point in the element. This will not be a
  normal vector, in general, but is required to obtain strain-free
  rigid-body motion.

  input:
  N:    the shape functions for the element
  Xr:   the frames at each node

  output:
  n1:   first normal to the beam curve
  n2:   second normal to the beam curve
*/
static inline void computeFrameNormals( const double N[],
                                        const TacsScalar Xr[],
                                        TacsScalar n1[],
                                        TacsScalar n2[] ){
  n1[0] = (N[0]*Xr[1] + N[1]*Xr[10] + N[2]*Xr[19]);
  n1[1] = (N[0]*Xr[4] + N[1]*Xr[13] + N[2]*Xr[22]);
  n1[2] = (N[0]*Xr[7] + N[1]*Xr[16] + N[2]*Xr[25]);

  n2[0] = (N[0]*Xr[2] + N[1]*Xr[11] + N[2]*Xr[20]);
  n2[1] = (N[0]*Xr[5] + N[1]*Xr[14] + N[2]*Xr[23]);
  n2[2] = (N[0]*Xr[8] + N[1]*Xr[17] + N[2]*Xr[26]);
}

/*
  Compute the derivative of the frame normal through-thickness
  to account for the variation of the frame
*/
static inline void computeFrameRateNormals( const double Na[],
                                            const TacsScalar Xr[],
                                            const TacsScalar Xdinv[],
                                            TacsScalar z1Xdinv[],
                                            TacsScalar z2Xdinv[] ){
  TacsScalar dn1[3];
  dn1[0] = -(Na[0]*Xr[1] + Na[1]*Xr[10] + Na[2]*Xr[19]);
  dn1[1] = -(Na[0]*Xr[4] + Na[1]*Xr[13] + Na[2]*Xr[22]);
  dn1[2] = -(Na[0]*Xr[7] + Na[1]*Xr[16] + Na[2]*Xr[25]);

  TacsScalar dn2[3];
  dn2[0] = -(Na[0]*Xr[2] + Na[1]*Xr[11] + Na[2]*Xr[20]);
  dn2[1] = -(Na[0]*Xr[5] + Na[1]*Xr[14] + Na[2]*Xr[23]);
  dn2[2] = -(Na[0]*Xr[8] + Na[1]*Xr[17] + Na[2]*Xr[26]);

  // Compute -Xdinv*dn1*e1^{T}*Xdinv
  TacsScalar tmp[3];
  matMult(Xdinv, dn1, tmp);
  z1Xdinv[0] = tmp[0]*Xdinv[0];
  z1Xdinv[1] = tmp[0]*Xdinv[1];
  z1Xdinv[2] = tmp[0]*Xdinv[2];
  
  z1Xdinv[3] = tmp[1]*Xdinv[0];
  z1Xdinv[4] = tmp[1]*Xdinv[1];
  z1Xdinv[5] = tmp[1]*Xdinv[2];

  z1Xdinv[6] = tmp[2]*Xdinv[0];
  z1Xdinv[7] = tmp[2]*Xdinv[1];
  z1Xdinv[8] = tmp[2]*Xdinv[2];

  // Compute -Xdinv*dn2*e3^{T}*Xdinv
  matMult(Xdinv, dn2, tmp);
  z2Xdinv[0] = tmp[0]*Xdinv[0];
  z2Xdinv[1] = tmp[0]*Xdinv[1];
  z2Xdinv[2] = tmp[0]*Xdinv[2];

  z2Xdinv[3] = tmp[1]*Xdinv[0];
  z2Xdinv[4] = tmp[1]*Xdinv[1];
  z2Xdinv[5] = tmp[1]*Xdinv[2];

  z2Xdinv[6] = tmp[2]*Xdinv[0];
  z2Xdinv[7] = tmp[2]*Xdinv[1];
  z2Xdinv[8] = tmp[2]*Xdinv[2];
}

/*
  Compute the derivative of the frame normal
*/
static inline void addFrameNormalSens( const TacsScalar n1d[],
                                       const TacsScalar n2d[],
                                       const double N[],
                                       TacsScalar Xrd[] ){
  Xrd[1] += N[0]*n1d[0];
  Xrd[4] += N[0]*n1d[1];
  Xrd[7] += N[0]*n1d[2];
  Xrd[10] += N[1]*n1d[0];
  Xrd[13] += N[1]*n1d[1];
  Xrd[16] += N[1]*n1d[2];
  Xrd[19] += N[2]*n1d[0];
  Xrd[22] += N[2]*n1d[1];
  Xrd[25] += N[2]*n1d[2];

  Xrd[2] += N[0]*n2d[0];
  Xrd[5] += N[0]*n2d[1];
  Xrd[8] += N[0]*n2d[2];
  Xrd[11] += N[1]*n2d[0];
  Xrd[14] += N[1]*n2d[1];
  Xrd[17] += N[1]*n2d[2];
  Xrd[20] += N[2]*n2d[0];
  Xrd[23] += N[2]*n2d[1];
  Xrd[26] += N[2]*n2d[2];
}

/*
  Compute the tying shape functions for the element for the
  out-of-plane shear components.
  
  This uses the quadrature points from a two-point Gauss quadrature
  scheme as the tying points. The tying point shape functions for the
  g13 and g23 shear strains are numbered as follows:

  g13               g23
  +--0-----1--+     +--0-----1--+

  input:
  u:    the parametric coordinate
  
  output:
  N13:  the shape functions associated with the g13 shear strain
*/
static inline void computeTyingFunc( const double u,
                                     double N13[] ){
  // The tying point offset
  const double t = 0.577350269189626;
  const double tinv = 1.0/t;

  // Compute the shape functions for the reduced dimension
  N13[0] = 0.5*tinv*(t - u);
  N13[1] = 0.5*tinv*(t + u);
}

/*
  Given the input vectors that are the derivative of the position
  vector along the u/v directions, and the orthogonal normal vector,
  assemble the results into a reference frame.  Note that this is not
  an orthonormal frame.

  input:
  Xa:      the first in-plane direction
  Xb:      the second in-plane direction
  normal:  the normal direction perpendicular to Xa and Xb
  
  output:
  Xr:      the reference frame associated with the given point
*/
static inline void assembleFrame( const TacsScalar Xa[],
                                  const TacsScalar Xb[],
                                  const TacsScalar normal[],
                                  TacsScalar Xr[] ){
  // Fill in the values of Xr
  Xr[0] = Xa[0];
  Xr[3] = Xa[1];
  Xr[6] = Xa[2];
  
  Xr[1] = Xb[0];
  Xr[4] = Xb[1];
  Xr[7] = Xb[2];
  
  // Add the values for the normal
  Xr[2] = normal[0];
  Xr[5] = normal[1];
  Xr[8] = normal[2];
}

/*
  Compute the 3x4 matrix from the following matrix-matrix product:

  A = J*S = 2(I - n*n^{T})*[ -eps | (eta*I - eps^{x}) ] 

  Note that this is the product of the inertia contribution from the
  director and the angular rate kinematic S matrix.

  input:
  n:     the surface normal vector
  eta:   the quaternion scalar
  eps:   the quaternion vector
  
  output:
  A:     the product of the matrix (I - n*n^{T}) with S
*/
static inline void computeNormalRateProduct( const TacsScalar n[], 
                                             const TacsScalar eta,
                                             const TacsScalar eps[],
                                             TacsScalar A[] ){
  // Compute the rate matrix
  TacsScalar S[12];
  computeSRateMat(eta, eps, S);
  
  // Pre-multiply the S matrix by (I - n*n^{T})
  TacsScalar ns = 0.0;

  // Compute the first column of A
  ns = n[0]*S[0] + n[1]*S[4] + n[2]*S[8];
  A[0] = S[0] - n[0]*ns;
  A[4] = S[4] - n[1]*ns;
  A[8] = S[8] - n[2]*ns;

  // Compute the second column of A
  ns = n[0]*S[1] + n[1]*S[5] + n[2]*S[9];
  A[1] = S[1] - n[0]*ns;
  A[5] = S[5] - n[1]*ns;
  A[9] = S[9] - n[2]*ns;

  // Compute the third column of A
  ns = n[0]*S[2] + n[1]*S[6] + n[2]*S[10];
  A[2] = S[2] - n[0]*ns;
  A[6] = S[6] - n[1]*ns;
  A[10] = S[10] - n[2]*ns;

  // Compute the third column of A
  ns = n[0]*S[3] + n[1]*S[7] + n[2]*S[11];
  A[3] = S[3] - n[0]*ns;
  A[7] = S[7] - n[1]*ns;
  A[11] = S[11] - n[2]*ns;
}

/*
  Constructor for the MITC3 element class

  input:
  stiff:      the stiffness object
  gravity:    the gravity vector
  vInit:      the initial velocity
  omegaInit:  the initial angular velocity
*/
MITC3::MITC3( TimoshenkoStiffness *_stiff, 
              TACSGibbsVector *_gravity,
              TACSGibbsVector *_vInit,
              TACSGibbsVector *_omegaInit ){
  // Set the stiffness 
  stiff = _stiff;
  stiff->incref();

  // Copy over the vectors (if they are defined)
  gravity = _gravity;
  vInit = _vInit;
  omegaInit = _omegaInit;
  if (gravity){ gravity->incref(); }
  if (vInit){ vInit->incref(); }
  if (omegaInit){ omegaInit->incref(); }

  // Get the Gauss quadrature points and weights
  FElibrary::getGaussPtsWts(ORDER, &gaussPts, &gaussWts);
}

/*
  Free/decref the objects
*/
MITC3::~MITC3(){
  stiff->decref();
  if (gravity){ gravity->decref(); }
  if (vInit){ vInit->decref(); }
  if (omegaInit){ omegaInit->decref(); }
}

/*
  Retrieve the initial values of the design variables
*/
void MITC3::getInitConditions( TacsScalar vars[], 
                               TacsScalar dvars[],
                               TacsScalar ddvars[],
                               const TacsScalar X[] ){
  memset(vars, 0, 8*NUM_NODES*sizeof(TacsScalar));
  memset(dvars, 0, 8*NUM_NODES*sizeof(TacsScalar));
  memset(ddvars, 0, 8*NUM_NODES*sizeof(TacsScalar));

  // The initial quaternions are eta = 1.0, eps = 0
  for ( int i = 0; i < NUM_NODES; i++ ){
    vars[8*i + 3] = 1.0;
  }

  // If the initial velocity is defined
  if (vInit){
    const TacsScalar *v0;
    vInit->getVector(&v0);
    for ( int i = 0; i < NUM_NODES; i++ ){
      dvars[8*i] = v0[0];
      dvars[8*i+1] = v0[1];
      dvars[8*i+2] = v0[2];
    }
  }
  
  // If the initial angular velocity is defined
  if (omegaInit){
    const TacsScalar *omega;
    omegaInit->getVector(&omega);
    
    for ( int i = 0; i < NUM_NODES; i++ ){
      // dot{u} = v + omega^{x}*r
      crossProductAdd(1.0, omega, &X[3*i], &dvars[8*i]);
      
      // d{eps}/dt = 0.5*omega
      dvars[8*i+4] = 0.5*omega[0];
      dvars[8*i+5] = 0.5*omega[1];
      dvars[8*i+6] = 0.5*omega[2];

      // ddot{u} = omega^{x}*omega^{x}*r
      // Note that the second derivative of the quaternions is zero since
      // there is no angular acceleration
      TacsScalar omegar[3];
      crossProduct(1.0, omega, &X[3*i], omegar);
      crossProduct(1.0, omega, omegar, &ddvars[8*i]);
    }
  }
}

/*
  The following function evaluates the kinetic energy and potential
  and elastic energies of the element.

  These can be used to verify that the equations of motion are
  implemented correctly, since the element implements a method based
  on Lagrange's equations of motion.

  Te = int_{A}(rho0*dot{U}^{T}*dot{U} + 
  .            rho1*(n1^{T}*(omega^{T}*omega - omega*omega^{T})*n1) +
  .            rho2*(n2^{T}*(omega^{T}*omega - omega*omega^{T})*n2) +
  .            2*rho3*(n1^{T}*(omega^{T}*omega - omega*omega^{T})*n2)) dA
  
  n1^{T}n1 = 1, n2^{T}n2 = 1, and n1^{T}*n2 = 0 so this simplifies to:

  Te = int_{A}(rho0*dot{U}^{T}*dot{U} + 
  .            rho1*(omega^{T}*omega - n1^{T}*omega*omega^{T}*n1) +
  .            rho2*(omega^{T}*omega - n2^{T}*omega*omega^{T}*n2) -
  .            2*rho3*(n1^{T}*omega*omega^{T}*n2)) dA
  
  input:
  time:   the simulation time
  vars:   the values of the variables
  dvars:  the time-derivative of the variables

  output:
  Te:     the kinetic energy
  Pe:     the potential energy
*/
void MITC3::computeEnergies( double time,
                             TacsScalar *_Te, 
                             TacsScalar *_Pe,
                             const TacsScalar X[],
                             const TacsScalar vars[],
                             const TacsScalar dvars[] ){
  // Set the gravity vector - if one exists
  TacsScalar g[3] = {0.0, 0.0, 0.0};
  if (gravity){
    const TacsScalar *_g;
    gravity->getVector(&_g);
    g[0] = _g[0];  g[1] = _g[1];  g[2] = _g[2];
  }

  // Compute the reference frames at the nodes
  TacsScalar Xr[9*NUM_NODES];
  computeFrames(Xr, X);

  // Compute the directors at the nodes
  TacsScalar d1[3*NUM_NODES], d2[3*NUM_NODES];
  computeDirectors(d1, d2, vars, Xr);

  // Compute the angular velocity at the nodes
  TacsScalar omega[3*NUM_NODES];
  computeAngularVelocity(omega, vars, dvars);

  // Compute the strain at the tying points
  TacsScalar g12[2], g13[2];
  computeTyingStrain(g12, g13, X, Xr, vars, d1, d2);

  // Initialize the velocities
  TacsScalar Te = 0.0, Pe = 0.0;

  // Evaluate the kinetic energy of the element
  for ( int i = 0; i < ORDER; i++ ){
      // Set the Gauss quadrature points
    const double u = gaussPts[i];
  
    // Evaluate the shape functions
    double N[NUM_NODES], Na[NUM_NODES];
    computeShapeFunc(u, N, Na);

    // Use the local frame to compute the 
    TacsScalar n1[3], n2[3];
    computeFrameNormals(N, Xr, n1, n2);
      
    // Assemble the frame at the current point
    TacsScalar Xa[3], Xd[9], Xdinv[9];
    innerProduct(Na, X, Xa);
    assembleFrame(Xa, n1, n2, Xd);
    TacsScalar det = inv3x3(Xd, Xdinv);
    det *= gaussWts[i];

    // Evaluate the areal mass properties
    TacsScalar rho[4];
    stiff->getPointwiseMass(&u, rho);

    // The following is used to evaluate the kinetic energy
    // ---------------------------------------------------
    // Evaluate the velocity at the quadrature point
    TacsScalar v0[3];
    innerProduct8(N, dvars, v0);

    // Compute the value of omega at the current point
    TacsScalar omeg[3];
    innerProduct(N, omega, omeg);
    
    // Compute the dot product with the normal
    TacsScalar omegn1 = vecDot(omeg, n1);
    TacsScalar omegn2 = vecDot(omeg, n2);
    
    // Add the contributions to the kinetic energy
    Te += 0.5*det*(rho[0]*vecDot(v0, v0) + 
                   (rho[1] + rho[2])*vecDot(omeg, omeg) - 
                   (rho[1]*omegn1*omegn1 + 
                    rho[2]*omegn2*omegn2 +
                    2.0*rho[3]*omegn1*omegn2));
    
    // The following code is used to evaluate the potential energy
    // -----------------------------------------------------------

    // Compute d(Xdinv)/dz1 and d(Xdinv)/dz2
    TacsScalar z1Xdinv[9], z2Xdinv[9];
    computeFrameRateNormals(Na, Xr, Xdinv, z1Xdinv, z2Xdinv);

    // Compute the transformation to local coordinates
    TacsScalar T[9];
    computeTransform(T, Xa);

    // Compute the derivative of U along the axial direction and
    // evaluate the director at the current point
    TacsScalar Ua[3], d1u[3], d2u[3];
    innerProduct8(Na, vars, Ua);
    innerProduct(N, d1, d1u);
    innerProduct(N, d2, d2u);
  
    // Derivatives of the displacement w.r.t. the beam parameters
    TacsScalar Ur[9];
    assembleFrame(Ua, d1u, d2u, Ur);
    
    // Derivative of the directors d1 and d2 along the axial direction
    TacsScalar d1a[3], d2a[3];
    innerProduct(Na, d1, d1a);
    innerProduct(Na, d2, d2a);
    
    // Compute the displacement-based strain
    TacsScalar e[6];
    evalStrain(e, Ur, d1a, d2a, Xdinv, z1Xdinv, z2Xdinv, T);
    
    // Add the contribution from the tying strain
    double N12[2];
    computeTyingFunc(u, N12);
    addTyingStrain(e, N12, g12, g13);

    // Compute the stress based on the strain values
    TacsScalar s[6];
    stiff->calculateStress(&u, e, s);

    // Compute the terms for the potential energy due to gravity
    TacsScalar U[3];
    innerProduct8(N, vars, U);

    // Add the product of the stress times strain to the
    // potential energy computation
    Pe += 0.5*det*(strainProduct(s, e) +
                   2.0*rho[0]*vecDot(g, U));
  }

  *_Te = 0.0; // Te;
  *_Pe = Pe;
}

/*
  Get the residuals of the equations of motion.

  The equations of motion for this element are derived using
  Lagrange's equations with constraints. Each node imposes a
  constraint that its own quaternions satisfy the required unit norm
  constraint.

  The equations of motion can be divided into two parts: (1) the
  motion of the deformed surface and (2) the motion of the normals of
  the surface (or directors since they are no longer normal to the
  deformed surface during deformation). The linear motion takes the
  form:

  M*ddot{u} + dU/dx - fg = 0

  where M is a mass matrix. The rotational degrees of freedom satisfy
  the following equations of motion:

  S^{T}*J*d{omega} + 2*dot{S}^{T}*J*omega + dU/dq - A^{T}*lamb = 0

  where J = (I - n*n^{T}) is a rotational inertia term and the
  constraints produce the term A^{T}*lamb. 

  input:
  X:      the element nodal coordinates
  vars:   the element degrees of freedom
  dvars:  the first time derivative of the degrees of freedom
  ddvars: the second time derivative of the degrees of freedom

  output:
  res:    the residuals
*/
void MITC3::addResidual( double time,
                         TacsScalar res[],
                         const TacsScalar X[],
                         const TacsScalar vars[],
                         const TacsScalar dvars[],
                         const TacsScalar ddvars[] ){
  // Set the gravity vector - if one exists
  TacsScalar g[3] = {0.0, 0.0, 0.0};
  if (gravity){
    const TacsScalar *_g;
    gravity->getVector(&_g);
    g[0] = _g[0];  g[1] = _g[1];  g[2] = _g[2];
  }

  // Compute the reference frames at the nodes
  TacsScalar Xr[9*NUM_NODES];
  computeFrames(Xr, X);

  // Compute the directors at the nodes
  TacsScalar d1[3*NUM_NODES], d2[3*NUM_NODES];
  TacsScalar d1dq[12*NUM_NODES], d2dq[12*NUM_NODES];
  computeDirectors(d1, d2, vars, Xr);
  computeDirectorDeriv(d1dq, d2dq, vars, Xr);

  // Compute the angular velocity and acceleration at the nodes
  TacsScalar omega[3*NUM_NODES], domega[3*NUM_NODES];
  computeAngularVelocity(omega, vars, dvars);
  computeAngularAccel(domega, vars, dvars, ddvars);

  // Compute the strain at the tying points
  TacsScalar g12[2], g13[2];
  TacsScalar B12[2*8*NUM_NODES], B13[2*8*NUM_NODES];
  computeTyingBmat(g12, g13, B12, B13,
                   X, Xr, vars, d1, d2, d1dq, d2dq);

  // Evaluate the kinetic energy of the element
  for ( int i = 0; i < ORDER; i++ ){
      // Set the Gauss quadrature points
    const double u = gaussPts[i];
  
    // Evaluate the shape functions
    double N[NUM_NODES], Na[NUM_NODES];
    computeShapeFunc(u, N, Na);

    // Use the local frame to compute the 
    TacsScalar n1[3], n2[3];
    computeFrameNormals(N, Xr, n1, n2);
      
    // Assemble the frame at the current point
    TacsScalar Xa[3], Xd[9], Xdinv[9];
    innerProduct(Na, X, Xa);
    assembleFrame(Xa, n1, n2, Xd);
    TacsScalar det = inv3x3(Xd, Xdinv);
    det *= gaussWts[i];

    // Evaluate the areal mass properties
    TacsScalar rho[4];
    stiff->getPointwiseMass(&u, rho);




    // Compute d(Xdinv)/dz1 and d(Xdinv)/dz2
    TacsScalar z1Xdinv[9], z2Xdinv[9];
    computeFrameRateNormals(Na, Xr, Xdinv, z1Xdinv, z2Xdinv);

    // Compute the transformation to local coordinates
    TacsScalar T[9];
    computeTransform(T, Xa);

    // Compute the derivative of U along the axial direction and
    // evaluate the director at the current point
    TacsScalar Ua[3], d1u[3], d2u[3];
    innerProduct8(Na, vars, Ua);
    innerProduct(N, d1, d1u);
    innerProduct(N, d2, d2u);
  
    // Derivatives of the displacement w.r.t. the beam parameters
    TacsScalar Ur[9];
    assembleFrame(Ua, d1u, d2u, Ur);
    
    // Derivative of the directors d1 and d2 along the axial direction
    TacsScalar d1a[3], d2a[3];
    innerProduct(Na, d1, d1a);
    innerProduct(Na, d2, d2a);

    // Compute the displacement-based strain
    TacsScalar e[6], B[6*8*NUM_NODES];
    evalBmat(e, B, N, Na, Ur, d1a, d2a,
             Xdinv, z1Xdinv, z2Xdinv, T, d1dq, d2dq);
 
    // Add the contribution from the tying strain
    double N12[2];
    computeTyingFunc(u, N12);
    addTyingStrain(e, N12, g12, g13);
    addTyingBmat(B, N12, B12, B13);

    // Compute the stress based on the strain values
    TacsScalar s[6];
    stiff->calculateStress(&u, e, s);

    // Add the contribution to the residual
    TacsScalar *r = res;
    const TacsScalar *b = B;;
    for ( int ii = 0; ii < NUM_NODES; ii++ ){
      r[0] += det*(strainProduct(s, &b[0]) - rho[0]*N[ii]*g[0]);
      r[1] += det*(strainProduct(s, &b[6]) - rho[0]*N[ii]*g[1]);
      r[2] += det*(strainProduct(s, &b[12]) - rho[0]*N[ii]*g[2]);
      r[3] += det*strainProduct(s, &b[18]);
      r[4] += det*strainProduct(s, &b[24]);
      r[5] += det*strainProduct(s, &b[30]);
      r[6] += det*strainProduct(s, &b[36]);

      r += 8;
      b += 48;
    }
  }
  
  // Set the scaling for the constraints
  TacsScalar scale = 1.0;

  // Add the constraints from the quaternion parametrization
  for ( int i = 0; i < NUM_NODES; i++ ){
    const TacsScalar *q = &vars[8*i+3];
    TacsScalar lamb = vars[8*i+7];

    // Add the result to the governing equations
    res[8*i+3] += 2.0*scale*q[0]*lamb;
    res[8*i+4] += 2.0*scale*q[1]*lamb;
    res[8*i+5] += 2.0*scale*q[2]*lamb;
    res[8*i+6] += 2.0*scale*q[3]*lamb;
    
    // Enforce the quaternion constraint
    res[8*i+7] += scale*(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3] - 1.0);
  }
}

/*
  Add the Jacobian for the governing equations

  The following code computes the Jacobian of the equations of motion
  involving a linear combination of the Jacobian w.r.t. the variables
  and their first and second time derivatives as follows:

  J = alpha*dR/dq + beta*dR/d(dot{q}) + gamma*dR/d(ddot{q})

  The alpha, beta, and gamma coefficients are used for the values and
  their first and second time-derivatives respectively.  This function
  is used to assemble the element-contribution to the multibody system
  Jacobian.

  input:
  alpha:   the variable coefficient
  beta:    the first-derivative coefficient
  gamma:   the second-derivative coefficient
  X:       the nodal locations (of the initial configuration)
  vars:    the variable values
  dvars:   the time derivative of the variable values
  ddvars:  the second time derivative of the variable values

  output:
  J:       the Jacobian matrix
*/
void MITC3::addJacobian( double time, TacsScalar J[],
                         double alpha, double beta, double gamma,
                         const TacsScalar X[],
                         const TacsScalar vars[],
                         const TacsScalar dvars[],
                         const TacsScalar ddvars[] ){
  // Compute the reference frames at the nodes
  TacsScalar Xr[9*NUM_NODES];
  computeFrames(Xr, X);

  // Compute the directors at the nodes
  TacsScalar d1[3*NUM_NODES], d2[3*NUM_NODES];
  TacsScalar d1dq[12*NUM_NODES], d2dq[12*NUM_NODES];
  computeDirectors(d1, d2, vars, Xr);
  computeDirectorDeriv(d1dq, d2dq, vars, Xr);

  // Compute the angular velocity and acceleration at the nodes
  TacsScalar omega[3*NUM_NODES], domega[3*NUM_NODES];
  computeAngularVelocity(omega, vars, dvars);
  computeAngularAccel(domega, vars, dvars, ddvars);

  // Compute the strain at the tying points
  TacsScalar g12[2], g13[2];
  TacsScalar B12[2*8*NUM_NODES], B13[2*8*NUM_NODES];
  computeTyingBmat(g12, g13, B12, B13,
                   X, Xr, vars, d1, d2, d1dq, d2dq);

  // The weights that are used for the geometric stiffness from
  // the tying strain
  TacsScalar w12[2], w13[2];
  memset(w12, 0, 2*sizeof(TacsScalar));
  memset(w13, 0, 2*sizeof(TacsScalar));

  // Evaluate the kinetic energy of the element
  for ( int i = 0; i < ORDER; i++ ){
      // Set the Gauss quadrature points
    const double u = gaussPts[i];
  
    // Evaluate the shape functions
    double N[NUM_NODES], Na[NUM_NODES];
    computeShapeFunc(u, N, Na);

    // Use the local frame to compute the 
    TacsScalar n1[3], n2[3];
    computeFrameNormals(N, Xr, n1, n2);
      
    // Assemble the frame at the current point
    TacsScalar Xa[3], Xd[9], Xdinv[9];
    innerProduct(Na, X, Xa);
    assembleFrame(Xa, n1, n2, Xd);
    TacsScalar det = inv3x3(Xd, Xdinv);
    det *= gaussWts[i];

    // Evaluate the areal mass properties
    TacsScalar rho[4];
    stiff->getPointwiseMass(&u, rho);

    /*
    // Add the contributions from the linear motion
    for ( int ii = 0; ii < NUM_NODES; ii++ ){
      const TacsScalar scale = gamma*h*N[ii]*N[jj]*rho[0];
      // Add the contributions from the rectilinear velocity
      J[8*NUM_NODES*(8*ii) + 8*jj] += scale;
      J[8*NUM_NODES*(8*ii+1) + 8*jj+1] += scale;
      J[8*NUM_NODES*(8*ii+2) + 8*jj+2] += scale;
    }
  
      // Compute the value of omega at the current point
      TacsScalar omeg[3], domeg[3];
      innerProduct(N, omega, omeg);
      innerProduct(N, domega, domeg);

      // Remove the normal component from angular velocity/accel.
      // w = (I - fn*fn^{T})*omega
      TacsScalar tmp, w[3], dw[3];
      tmp = vecDot(omeg, fn);
      w[0] = omeg[0] - tmp*fn[0];
      w[1] = omeg[1] - tmp*fn[1];
      w[2] = omeg[2] - tmp*fn[2];

      tmp = vecDot(domeg, fn);
      dw[0] = domeg[0] - tmp*fn[0];
      dw[1] = domeg[1] - tmp*fn[1];
      dw[2] = domeg[2] - tmp*fn[2];

      // Add the contributions from the rotational DOF
      for ( int ii = 0; ii < NUM_NODES; ii++ ){
        // Compute and store the product of (I - n*n^{T}) with the
        // angular rate matrices
        TacsScalar JSii[12], JdSii[12];
        computeNormalRateProduct(fn, vars[8*ii+3], 
                                 &vars[8*ii+4], JSii);
        computeNormalRateProduct(fn, dvars[8*ii+3], 
                                 &dvars[8*ii+4], JdSii);

        // Set the pointer to the Jacobian entries that will
        // be added
        TacsScalar *Jp = &J[(8*NUM_NODES+1)*(8*ii+3)];
        const int ldj = 8*NUM_NODES;

        // Add the diagonal terms
        const TacsScalar dscale = h*N[ii]*rho[1];
        addSRateMatTransDeriv(alpha*dscale, dw, Jp, ldj);
        addSRateMatTransDeriv(2.0*beta*dscale, w, Jp, ldj);

        // Add the result to the Jacobian matrix
        const TacsScalar *q = vars, *dq = dvars, *ddq = ddvars;
        for ( int jj = 0; jj < NUM_NODES; jj++ ){
          // Set the common scaling factor for all terms
          const TacsScalar scale = h*N[ii]*N[jj]*rho[1];

          // Set the pointer to the Jacobian entries that will
          // be added
          TacsScalar *Jp = &J[8*NUM_NODES*(8*ii+3) + 8*jj+3];
          
          // Compute S = S(q)
          TacsScalar Sjj[12];
          computeSRateMat(q[3], &q[4], Sjj);

          // Compute dot{S} = S(dot{q})
          TacsScalar dSjj[12];
          computeSRateMat(dq[3], &dq[4], dSjj);

          // Compute ddot{S} = S(ddot{q})
          TacsScalar ddSjj[12];
          computeSRateMat(ddq[3], &ddq[4], ddSjj);

          // Add the Jacobian terms from the DOF:
          // T(dw) - S^{T}*J*S(ddot{q}) - 2*dot{S}^{T}*J*dot{S}
          addBlock3x4Product(-alpha*scale, JSii, ddSjj, Jp, ldj);
          addBlock3x4Product(-2.0*alpha*scale, JdSii, dSjj, Jp, ldj);

          // Add the Jacobian terms from the first time derivatives:
          // 2*dot{S}^{T}*J*S
          addBlock3x4Product(2.0*beta*scale, JdSii, Sjj, Jp, ldj);

          // Add the Jacobian terms from the second time derivatives:
          // S^{T}*J*S
          addBlock3x4Product(gamma*scale, JSii, Sjj, Jp, ldj);

          q += 8;
          dq += 8;
          ddq += 8;
        }
      }
    */

    // This code is expensive, and is only required when alpha 
    // is non-zero
    if (alpha != 0.0){
      // Everything left in this function is proportional to
      // h*alpha, so we scale h by alpha
      det *= alpha;



      // Compute d(Xdinv)/dz1 and d(Xdinv)/dz2
      TacsScalar z1Xdinv[9], z2Xdinv[9];
      computeFrameRateNormals(Na, Xr, Xdinv, z1Xdinv, z2Xdinv);

      // Compute the transformation to local coordinates
      TacsScalar T[9];
      computeTransform(T, Xa);

      // Compute the derivative of U along the axial direction and
      // evaluate the director at the current point
      TacsScalar Ua[3], d1u[3], d2u[3];
      innerProduct8(Na, vars, Ua);
      innerProduct(N, d1, d1u);
      innerProduct(N, d2, d2u);
  
      // Derivatives of the displacement w.r.t. the beam parameters
      TacsScalar Ur[9];
      assembleFrame(Ua, d1u, d2u, Ur);
    
      // Derivative of the directors d1 and d2 along the axial direction
      TacsScalar d1a[3], d2a[3];
      innerProduct(Na, d1, d1a);
      innerProduct(Na, d2, d2a);

      // Compute the displacement-based strain
      TacsScalar e[6], B[6*8*NUM_NODES];
      evalBmat(e, B, N, Na, Ur, d1a, d2a,
               Xdinv, z1Xdinv, z2Xdinv, T, d1dq, d2dq);
 
      // Add the contribution from the tying strain
      double N12[2];
      computeTyingFunc(u, N12);
      addTyingStrain(e, N12, g12, g13);
      addTyingBmat(B, N12, B12, B13);

      // Compute the stress based on the strain values
      TacsScalar s[6];
      stiff->calculateStress(&u, e, s);

      /*
      // Add to the weights
      addTyingGmatWeights(w13, w23, h, s,
                          N13, N23, Xdinv, T);
      */

      // Add the geometric stiffness terms
      addGmat(J, det, s, N, Na, Ur, d1a, d2a, 
              Xdinv, z1Xdinv, z2Xdinv, T, Xr, d1dq, d2dq);
        
      // Add the contribution to the residual
      for ( int ii = 0; ii < NUM_NODES; ii++ ){
        for ( int ik = 0; ik < 7; ik++ ){
          // Compute the stress from the 8*i + ik component
          TacsScalar sbii[8];
          stiff->calculateStress(&u, &B[6*(8*ii + ik)], sbii);
          
          const TacsScalar *b = B;
          TacsScalar *Jp = &J[8*NUM_NODES*(8*ii + ik)];
          for ( int jj = 0; jj < NUM_NODES; jj++ ){
            Jp[0] += det*strainProduct(sbii, &b[0]);
            Jp[1] += det*strainProduct(sbii, &b[6]);
            Jp[2] += det*strainProduct(sbii, &b[12]);
            Jp[3] += det*strainProduct(sbii, &b[18]);
            Jp[4] += det*strainProduct(sbii, &b[24]);
            Jp[5] += det*strainProduct(sbii, &b[30]);
            Jp[6] += det*strainProduct(sbii, &b[36]);
            
            Jp += 8;
            b += 48;
          }
        }
      }
    }
  }

  /*
  // Add the geometric stiffness terms from the tying strain
  addTyingGmat(J, w13, w23, X, Xr, vars, dir, dirdq);
  */

  // Set the scaling for the constraints
  TacsScalar scale = 2.0*alpha;

  // Add the constraints from the quaternions
  for ( int i = 0; i < NUM_NODES; i++ ){
    const TacsScalar *q = &vars[8*i+3];
    const int ldj = 8*NUM_NODES;

    TacsScalar *Jp = &J[(8*NUM_NODES+1)*(8*i + 3)];
    TacsScalar lamb = vars[8*i+7];
    
    // Add the constraint terms
    Jp[4] += scale*q[0];
    Jp[4+ldj] += scale*q[1];
    Jp[4+2*ldj] += scale*q[2];
    Jp[4+3*ldj] += scale*q[3];

    // Enforce the quaternion constraint
    Jp[4*ldj] += scale*q[0];
    Jp[4*ldj+1] += scale*q[1];
    Jp[4*ldj+2] += scale*q[2];
    Jp[4*ldj+3] += scale*q[3];

    // Add the terms to the diagonal
    Jp[0] += scale*lamb;
    Jp[ldj+1] += scale*lamb;
    Jp[2*(ldj+1)] += scale*lamb;
    Jp[3*(ldj+1)] += scale*lamb;
  }
}

/*
  Add the derivative of the product of the adjoint variables with the
  residuals to the given design variable vector.

  input:
  time:     the simulation time
  scale:    the scalar factor applied to the result
  fdvSens:  the derivative (times scale) is accumulated in this array
  dvLen:    the design array length
  psi:      the adjoint vector
  X:        the node locations
  vars:     the variable values
  dvars:    the time derivatives of the variable values
  ddvars:   the second time derivative of the variable values
*/
void MITC3::addAdjResProduct( double time, double scale,
                              TacsScalar fdvSens[], int dvLen,
                              const TacsScalar psi[],
                              const TacsScalar X[],
                              const TacsScalar vars[],
                              const TacsScalar dvars[],
                              const TacsScalar ddvars[] ){}

/*
  Compute the derivative of the product of the adjoint variables and
  the residual vector.

  This code uses a reverse-mode method, which makes the derivative
  code faster, but a bit more difficult to follow.
  
  input:
  time:      the simulation time
  scale:     scale the derivative vector by this factor
  psi:       the adjoint variables associated with this element
  vars:      the state variables
  dvars:     the first derivative of the state variables
  ddvars:    the second derivative of the state variables

  output:
  fXptSens:  add the derivative to this vector
*/
void MITC3::addAdjResXptProduct( double time, double scale,
                                 TacsScalar fXptSens[],
                                 const TacsScalar psi[],
                                 const TacsScalar X[],
                                 const TacsScalar vars[],
                                 const TacsScalar dvars[],
                                 const TacsScalar ddvars[] ){}

/*
  Given the nodal degrees of freedom and their time-derivatives,
  compute the angular velocity at each node
*/
void MITC3::computeAngularVelocity( TacsScalar omega[],
                                    const TacsScalar vars[],
                                    const TacsScalar dvars[] ){
  for ( int i = 0; i < NUM_NODES; i++ ){
    TacsScalar eta = vars[3];
    const TacsScalar *eps = &vars[4];
    TacsScalar deta = dvars[3];
    const TacsScalar *deps = &dvars[4];

    // omega = -2*eps^{x}*deps + 2*eta*deps - eps*deta
    crossProduct(-2.0, eps, deps, omega);
    vecAxpy(2.0*eta, deps, omega);
    vecAxpy(-2.0*deta, eps, omega);

    omega += 3;
    vars += 8;
    dvars += 8;
  }
}

/*
  Given the nodal degrees of freedom and their first and second
  time-derivatives, compute the angular acceleration at the nodes.
*/
void MITC3::computeAngularAccel( TacsScalar domega[],
                                 const TacsScalar vars[],
                                 const TacsScalar dvars[],
                                 const TacsScalar ddvars[] ){
  for ( int i = 0; i < NUM_NODES; i++ ){
    // Set pointers to the values 
    TacsScalar eta = vars[3];
    const TacsScalar *eps = &vars[4];
    TacsScalar ddeta = ddvars[3];
    const TacsScalar *ddeps = &ddvars[4];

    // domega = S(q)*ddot{q}
    crossProduct(-2.0, eps, ddeps, domega);
    vecAxpy(2.0*eta, ddeps, domega);
    vecAxpy(-2.0*ddeta, eps, domega);

    domega += 3;
    vars += 8;
    dvars += 8;
    ddvars += 8;
  }
}

/*
  Given the tangent to the beam, compute the local frame

  input:
  Xa:   tangent to the first parametric direction

  output:
  T:    the transformation matrix

  returns:  the norm of the tangent vector
*/
TacsScalar MITC3::computeTransform( TacsScalar T[],
                                    const TacsScalar Xa[] ){
  // Get the reference axis
  const TacsScalar *axis = stiff->getRefAxis();

  // Compute the reference frame
  TacsScalar t[3];
  TacsScalar tnorm = sqrt(Xa[0]*Xa[0] + Xa[1]*Xa[1] + Xa[2]*Xa[2]);
  TacsScalar tinv = 1.0/tnorm;
  t[0] = tinv*Xa[0];
  t[1] = tinv*Xa[1];
  t[2] = tinv*Xa[2];

  // Compute the first direction in the plane
  TacsScalar n1[3];
  TacsScalar tdot = vecDot(t, axis);
  n1[0] = axis[0] - tdot*t[0];
  n1[1] = axis[1] - tdot*t[1];
  n1[2] = axis[2] - tdot*t[2];

  // Compute the norm
  TacsScalar n1inv = 1.0/sqrt(n1[0]*n1[0] + n1[1]*n1[1] + n1[2]*n1[2]);
  n1[0] *= n1inv;
  n1[1] *= n1inv;
  n1[2] *= n1inv;

  // Compute the cross product
  TacsScalar n2[3];
  crossProduct(1.0, t, n1, n2);
  
  // Assemble the frame
  assembleFrame(t, n1, n2, T);

  return tnorm;
}

/*
  At each node in the finite-element compute the derivatives of the
  coordinate directions and assemble a locally-aligned reference
  frame.

  Each locally-aligned reference frame Xr[] consists of a 3x3 matrix
  stored in row-major order where the first direction is aligned along
  the axial direction, the second direction is normal to the axial
  direction along th

  Xr = [X,xi1; X,xi2; n]

  input:
  X:    the initial nodal locations
  
  output:
  Xr:   the locally-aligned frames
*/
void MITC3::computeFrames( TacsScalar Xr[],
                           const TacsScalar X[] ){
  for ( int i = 0; i < ORDER; i++ ){
    // Find the u/v values at the node locations
    double u = -1.0 + 2.0*i/(ORDER-1.0);
    
    // Evaluate the shape functions
    double N[NUM_NODES], Na[NUM_NODES];
    computeShapeFunc(u, N, Na);

    // Compute the derivative along the shape function directions
    TacsScalar Xa[3];
    innerProduct(Na, X, Xa);

    // Compute the transformation matrix
    computeTransform(Xr, Xa);
    
    // Increment the pointer to the frames
    Xr += 9;  
  }
}

/*
  Based on the values of the element variables, determine the director
  values.
  
  The director is the difference between the deformed normal direction
  and the initial normal. The directors are used to determine the
  through-thickness strain distribution in the finite-element model.
  The advantage of the director approach is that it enables a
  geometrically exact displacement representation and also allows for
  shell-shell intersections at 90 degree angles.

  input:
  vars:   the element variable values
  Xr:     the local frames at each node in the mesh

  output:
  d:      the director at each node in the finite-element
*/
void MITC3::computeDirectors( TacsScalar d1[],
                              TacsScalar d2[],
                              const TacsScalar vars[],
                              const TacsScalar Xr[] ){
  for ( int i = 0; i < NUM_NODES; i++ ){
    // Set the pointer to the
    const TacsScalar *q = &vars[3];

    // Compute C = rot - I
    TacsScalar C[9];
    C[0] =-2.0*(q[2]*q[2] + q[3]*q[3]);
    C[1] = 2.0*(q[1]*q[2] + q[3]*q[0]);
    C[2] = 2.0*(q[1]*q[3] - q[2]*q[0]);

    C[3] = 2.0*(q[2]*q[1] - q[3]*q[0]);
    C[4] =-2.0*(q[1]*q[1] + q[3]*q[3]);
    C[5] = 2.0*(q[2]*q[3] + q[1]*q[0]);

    C[6] = 2.0*(q[3]*q[1] + q[2]*q[0]);
    C[7] = 2.0*(q[3]*q[2] - q[1]*q[0]);
    C[8] =-2.0*(q[1]*q[1] + q[2]*q[2]);

    // Compute d1 = C^{T}*n1
    d1[0] = C[0]*Xr[1] + C[3]*Xr[4] + C[6]*Xr[7];
    d1[1] = C[1]*Xr[1] + C[4]*Xr[4] + C[7]*Xr[7];
    d1[2] = C[2]*Xr[1] + C[5]*Xr[4] + C[8]*Xr[7];

    // Compute d2 = C^{T}*n2
    d2[0] = C[0]*Xr[2] + C[3]*Xr[5] + C[6]*Xr[8];
    d2[1] = C[1]*Xr[2] + C[4]*Xr[5] + C[7]*Xr[8];
    d2[2] = C[2]*Xr[2] + C[5]*Xr[5] + C[8]*Xr[8];

    d1 += 3; // Each director is a 3-vector
    d2 += 3; // Each director is a 3-vector
    Xr += 9; // Increment over each frame
    vars += 8; // 8 variables per node
  }
}

/*
  Add the derivative of the directors to the local frame
*/
void MITC3::addDirectorsSens( TacsScalar Xrd[],
                              const TacsScalar d1d[],
                              const TacsScalar d2d[],
                              const TacsScalar vars[] ){
  for ( int i = 0; i < NUM_NODES; i++ ){
    // Set the pointer to the
    const TacsScalar *q = &vars[3];

    // Compute C = rot - I
    TacsScalar C[9];
    C[0] =-2.0*(q[2]*q[2] + q[3]*q[3]);
    C[1] = 2.0*(q[1]*q[2] + q[3]*q[0]);
    C[2] = 2.0*(q[1]*q[3] - q[2]*q[0]);

    C[3] = 2.0*(q[2]*q[1] - q[3]*q[0]);
    C[4] =-2.0*(q[1]*q[1] + q[3]*q[3]);
    C[5] = 2.0*(q[2]*q[3] + q[1]*q[0]);

    C[6] = 2.0*(q[3]*q[1] + q[2]*q[0]);
    C[7] = 2.0*(q[3]*q[2] - q[1]*q[0]);
    C[8] =-2.0*(q[1]*q[1] + q[2]*q[2]);

    Xrd[1] += C[0]*d1d[0] + C[1]*d1d[1] + C[2]*d1d[2];
    Xrd[4] += C[3]*d1d[0] + C[4]*d1d[1] + C[5]*d1d[2];
    Xrd[7] += C[6]*d1d[0] + C[7]*d1d[1] + C[8]*d1d[2];

    Xrd[2] += C[0]*d2d[0] + C[1]*d2d[1] + C[2]*d2d[2];
    Xrd[5] += C[3]*d2d[0] + C[4]*d2d[1] + C[5]*d2d[2];
    Xrd[8] += C[6]*d2d[0] + C[7]*d2d[1] + C[8]*d2d[2];

    d1d += 3; // Each director is a 3-vector
    d2d += 3; // Each director is a 3-vector
    Xrd += 9; // Increment over each frame
    vars += 8; // 8 variables per node
  }
}

/*
  Compute the derivative of the director values w.r.t. the rotation
  matrix parametrization. 

  This code computes the derivative of (C^{T} - I)*n w.r.t. the
  quaternions at each node in the finite-element. This is required
  to compute the governing equations of motion for the element.

  input:
  vars:  the element variables
  Xr:    the local frame for each node in the mesh

  output:
  ddq:   the derivative of the directors w.r.t. the quaterions
*/
void MITC3::computeDirectorDeriv( TacsScalar d1dq[],
                                  TacsScalar d2dq[],
                                  const TacsScalar vars[],
                                  const TacsScalar Xr[] ){
  for ( int i = 0; i < NUM_NODES; i++ ){
    // Set the pointer to the
    const TacsScalar *q = &vars[3];

    // Compute the derivative of the rotation matrix w.r.t.
    // the quaternions
    TacsScalar Q[9];
    Q[0] = 0.0;
    Q[1] = 2.0*q[3];
    Q[2] =-2.0*q[2];

    Q[3] =-2.0*q[3];
    Q[4] = 0.0;
    Q[5] = 2.0*q[1];

    Q[6] = 2.0*q[2];
    Q[7] =-2.0*q[1];
    Q[8] = 0.0;

    // Compute ddq = D^{T}*n1
    d1dq[0] = Q[0]*Xr[1] + Q[3]*Xr[4] + Q[6]*Xr[7];
    d1dq[1] = Q[1]*Xr[1] + Q[4]*Xr[4] + Q[7]*Xr[7];
    d1dq[2] = Q[2]*Xr[1] + Q[5]*Xr[4] + Q[8]*Xr[7];
    d1dq += 3;

    // Compute ddq = D^{T}*n2
    d2dq[0] = Q[0]*Xr[2] + Q[3]*Xr[5] + Q[6]*Xr[8];
    d2dq[1] = Q[1]*Xr[2] + Q[4]*Xr[5] + Q[7]*Xr[8];
    d2dq[2] = Q[2]*Xr[2] + Q[5]*Xr[5] + Q[8]*Xr[8];
    d2dq += 3;

    // Derivative w.r.t. q[1]
    Q[0] = 0.0;
    Q[1] = 2.0*q[2];
    Q[2] = 2.0*q[3];

    Q[3] = 2.0*q[2];
    Q[4] =-4.0*q[1];
    Q[5] = 2.0*q[0];

    Q[6] = 2.0*q[3];
    Q[7] =-2.0*q[0];
    Q[8] =-4.0*q[1];

    // Compute ddq = D^{T}*n1
    d1dq[0] = Q[0]*Xr[1] + Q[3]*Xr[4] + Q[6]*Xr[7];
    d1dq[1] = Q[1]*Xr[1] + Q[4]*Xr[4] + Q[7]*Xr[7];
    d1dq[2] = Q[2]*Xr[1] + Q[5]*Xr[4] + Q[8]*Xr[7];
    d1dq += 3;

    // Compute ddq = D^{T}*n2
    d2dq[0] = Q[0]*Xr[2] + Q[3]*Xr[5] + Q[6]*Xr[8];
    d2dq[1] = Q[1]*Xr[2] + Q[4]*Xr[5] + Q[7]*Xr[8];
    d2dq[2] = Q[2]*Xr[2] + Q[5]*Xr[5] + Q[8]*Xr[8];
    d2dq += 3;

    // Derivative w.r.t. q[2]
    Q[0] =-4.0*q[2];
    Q[1] = 2.0*q[1];
    Q[2] =-2.0*q[0];

    Q[3] = 2.0*q[1];
    Q[4] = 0.0;
    Q[5] = 2.0*q[3];

    Q[6] = 2.0*q[0];
    Q[7] = 2.0*q[3];
    Q[8] =-4.0*q[2];

    // Compute ddq = D^{T}*n1
    d1dq[0] = Q[0]*Xr[1] + Q[3]*Xr[4] + Q[6]*Xr[7];
    d1dq[1] = Q[1]*Xr[1] + Q[4]*Xr[4] + Q[7]*Xr[7];
    d1dq[2] = Q[2]*Xr[1] + Q[5]*Xr[4] + Q[8]*Xr[7];
    d1dq += 3;

    // Compute ddq = D^{T}*n2
    d2dq[0] = Q[0]*Xr[2] + Q[3]*Xr[5] + Q[6]*Xr[8];
    d2dq[1] = Q[1]*Xr[2] + Q[4]*Xr[5] + Q[7]*Xr[8];
    d2dq[2] = Q[2]*Xr[2] + Q[5]*Xr[5] + Q[8]*Xr[8];
    d2dq += 3;

    // Derivative w.r.t. q[3]
    Q[0] =-4.0*q[3];
    Q[1] = 2.0*q[0];
    Q[2] = 2.0*q[1];

    Q[3] =-2.0*q[0];
    Q[4] =-4.0*q[3];
    Q[5] = 2.0*q[2];

    Q[6] = 2.0*q[1];
    Q[7] = 2.0*q[2];
    Q[8] = 0.0;

    // Compute ddq = D^{T}*n1
    d1dq[0] = Q[0]*Xr[1] + Q[3]*Xr[4] + Q[6]*Xr[7];
    d1dq[1] = Q[1]*Xr[1] + Q[4]*Xr[4] + Q[7]*Xr[7];
    d1dq[2] = Q[2]*Xr[1] + Q[5]*Xr[4] + Q[8]*Xr[7];
    d1dq += 3;

    // Compute ddq = D^{T}*n2
    d2dq[0] = Q[0]*Xr[2] + Q[3]*Xr[5] + Q[6]*Xr[8];
    d2dq[1] = Q[1]*Xr[2] + Q[4]*Xr[5] + Q[7]*Xr[8];
    d2dq[2] = Q[2]*Xr[2] + Q[5]*Xr[5] + Q[8]*Xr[8];
    d2dq += 3;

    Xr += 9; // Increment over each frame
    vars += 8; // 8 variables per node
  }
}  

/*
  Given the derivatives of the displacements and transformation to the
  local coordinates, evaluate strain in the local reference frame.

  The expressions for the strain require the derivative of the
  displacements along the axial coordinate and the derivative of the
  directors d1 and d2 along the axial direction. The inputs are:

  Ur = [ U0,a; d1; d2 ]
  d1a = d(d1)/da
  d2a = d(d2)/da
  
  Where a is the normalized direction a = xi/||dR/dxi||_{2}
    
  input:
  Ur:      the derivative of the displacements 
  d1a:     the derivative of the director
  d2a:     the derivative of the second director
  Xdinv:   the Jacobian transformation
  z1Xdinv: the derivative of the transformation w.r.t. z1
  z2Xdinv: the derivative of the transformation w.r.t. z2
  T:       the transformation to the beam-aligned coordinates

  output:
  e:      the displacement-based components of the strain 
*/
void MITC3::evalStrain( TacsScalar e[], 
                        const TacsScalar Ur[],
                        const TacsScalar d1a[],
                        const TacsScalar d2a[],
                        const TacsScalar Xdinv[],
                        const TacsScalar z1Xdinv[],
                        const TacsScalar z2Xdinv[],
                        const TacsScalar T[] ){
  // Compute U0 = T^{T}*Ur*Xdinv*T
  TacsScalar U0[9], tmp[9];
  matMatMult(Ur, Xdinv, U0);
  matMatMult(U0, T, tmp);
  matTransMatMult(T, tmp, U0);
  
  // Compute the derivative of the directors along the axial direction
  // Td1a = T^{T}*(d1a*e1^{T}*Xdinv + Ur*z1Xdinv)*T*e1
  TacsScalar scale = Xdinv[0]*T[0] + Xdinv[1]*T[3] + Xdinv[2]*T[6];
  TacsScalar Td1a[3];
  tmp[0] = T[0];
  tmp[1] = T[3];
  tmp[2] = T[6];
  matMult(z1Xdinv, tmp, Td1a); // tmp = z1Xdinv*T*e1
  matMult(Ur, Td1a, tmp); // tmp = Ur*z1Xdinv*T*e1
  vecAxpy(scale, d1a, tmp);
  matMultTrans(T, tmp, Td1a);

  // Td2a = T^{T}*d2a*e1^{T}*Xdinv*T*e1
  TacsScalar Td2a[3];
  tmp[0] = T[0];
  tmp[1] = T[3];
  tmp[2] = T[6];
  matMult(z2Xdinv, tmp, Td2a); // tmp = z1Xdinv*T*e1
  matMult(Ur, Td2a, tmp); // tmp = Ur*z1Xdinv*T*e1
  vecAxpy(scale, d2a, tmp);
  matMultTrans(T, tmp, Td2a);

  // Compute the axial strain
  e[0] = U0[0] + 0.5*(U0[0]*U0[0] + U0[3]*U0[3] + U0[6]*U0[6]);

  // Compute the torsional component of the strain
  e[1] = Td1a[2] - Td2a[1] + 
    (Td1a[0]*U0[2] + Td1a[1]*U0[5] + Td1a[2]*U0[8]) -
    (Td2a[0]*U0[1] + Td2a[1]*U0[4] + Td2a[2]*U0[7]);

  // Compute the bending components of the strain
  e[2] = Td1a[0] + (U0[0]*Td1a[0] + U0[3]*Td1a[1] + U0[6]*Td1a[2]);
  e[3] = Td2a[0] + (U0[0]*Td2a[0] + U0[3]*Td2a[1] + U0[6]*Td2a[2]);
}

/*
  Given the derivative of the displacements and directors along
  the locate coordinates, compute the derivative of the strain
  with respect to the displacement/rotation variables
*/
void MITC3::evalBmat( TacsScalar e[],
                      TacsScalar B[],
                      const double N[],
                      const double Na[],
                      const TacsScalar Ur[],
                      const TacsScalar d1a[],
                      const TacsScalar d2a[],
                      const TacsScalar Xdinv[],
                      const TacsScalar z1Xdinv[],
                      const TacsScalar z2Xdinv[],
                      const TacsScalar T[],
                      const TacsScalar d1dq[],
                      const TacsScalar d2dq[] ){
  // Compute U0 = T^{T}*Ur*Xdinv*T
  TacsScalar U0[9], tmp[9];
  matMatMult(Ur, Xdinv, U0);
  matMatMult(U0, T, tmp);
  matTransMatMult(T, tmp, U0);

  // Compute S = Xdinv*T
  TacsScalar S[9];
  matMatMult(Xdinv, T, S);
  
  // Compute the derivative of the directors along the axial direction
  // Td1a = T^{T}*(d1a*e1^{T}*Xdinv + Ur*z1Xdinv)*T*e1
  TacsScalar Td1a[3], z1Te1[3];
  tmp[0] = T[0];
  tmp[1] = T[3];
  tmp[2] = T[6];
  matMult(z1Xdinv, tmp, z1Te1); // tmp = z1Xdinv*T*e1
  matMult(Ur, z1Te1, tmp); // tmp = Ur*z1Xdinv*T*e1
  vecAxpy(S[0], d1a, tmp);
  matMultTrans(T, tmp, Td1a);

  // Td2a = T^{T}*d2a*e1^{T}*Xdinv*T*e1
  TacsScalar Td2a[3], z2Te1[3];
  tmp[0] = T[0];
  tmp[1] = T[3];
  tmp[2] = T[6];
  matMult(z2Xdinv, tmp, z2Te1); // tmp = z1Xdinv*T*e1
  matMult(Ur, z2Te1, tmp); // tmp = Ur*z1Xdinv*T*e1
  vecAxpy(S[0], d2a, tmp);
  matMultTrans(T, tmp, Td2a);

  // Compute the axial strain
  e[0] = U0[0] + 0.5*(U0[0]*U0[0] + U0[3]*U0[3] + U0[6]*U0[6]);

  // Compute the torsional component of the strain
  e[1] = Td1a[2] - Td2a[1] + 
    (Td1a[0]*U0[2] + Td1a[1]*U0[5] + Td1a[2]*U0[8]) -
    (Td2a[0]*U0[1] + Td2a[1]*U0[4] + Td2a[2]*U0[7]);

  // Compute the bending components of the strain
  e[2] = Td1a[0] + (U0[0]*Td1a[0] + U0[3]*Td1a[1] + U0[6]*Td1a[2]);
  e[3] = Td2a[0] + (U0[0]*Td2a[0] + U0[3]*Td2a[1] + U0[6]*Td2a[2]);

  for ( int i = 0; i < NUM_NODES; i++ ){
    TacsScalar *b = &B[NUM_STRESSES*(8*i)];

    // Compute the derivatives w.r.t. displacement variables
    for ( int k = 0; k < 3; k++ ){
      // Note that dU = [d(U0[0])/dui | d(U0[3])/dui | d(U0[6])/dui ]
      TacsScalar dU[9];
      dU[0] = S[0]*T[3*k]*Na[i];
      dU[1] = S[1]*T[3*k]*Na[i];
      dU[2] = S[2]*T[3*k]*Na[i];

      dU[3] = S[0]*T[3*k+1]*Na[i];
      dU[4] = S[1]*T[3*k+1]*Na[i];
      dU[5] = S[2]*T[3*k+1]*Na[i];

      dU[6] = S[0]*T[3*k+2]*Na[i];
      dU[7] = S[1]*T[3*k+2]*Na[i];
      dU[8] = S[2]*T[3*k+2]*Na[i];
            
      TacsScalar dTd1a[3];
      dTd1a[0] = z1Te1[0]*T[3*k]*Na[i];
      dTd1a[1] = z1Te1[0]*T[3*k+1]*Na[i];
      dTd1a[2] = z1Te1[0]*T[3*k+2]*Na[i];

      TacsScalar dTd2a[3];
      dTd2a[0] = z2Te1[0]*T[3*k]*Na[i];
      dTd2a[1] = z2Te1[0]*T[3*k+1]*Na[i];
      dTd2a[2] = z2Te1[0]*T[3*k+2]*Na[i];

      // Compute the derivative
      b[0] = dU[0] + U0[0]*dU[0] + U0[3]*dU[3] + U0[6]*dU[6];
      b[1] =  dTd1a[2] - dTd2a[1] + 
        (Td1a[0]*dU[2] + Td1a[1]*dU[5] + Td1a[2]*dU[8]) -
        (Td2a[0]*dU[1] + Td2a[1]*dU[4] + Td2a[2]*dU[7]) +
        (dTd1a[0]*U0[2] + dTd1a[1]*U0[5] + dTd1a[2]*U0[8]) -
        (dTd2a[0]*U0[1] + dTd2a[1]*U0[4] + dTd2a[2]*U0[7]);
      b[2] = dTd1a[0] + (dU[0]*Td1a[0] + dU[3]*Td1a[1] + dU[6]*Td1a[2] +
                         U0[0]*dTd1a[0] + U0[3]*dTd1a[1] + U0[6]*dTd1a[2]);
      b[3] = dTd2a[0] + (dU[0]*Td2a[0] + dU[3]*Td2a[1] + dU[6]*Td2a[2] +
                         U0[0]*dTd2a[0] + U0[3]*dTd2a[1] + U0[6]*dTd2a[2]);
      b[4] = b[5] = 0.0;
      b += NUM_STRESSES;
    }
  
    // Compute the derivatives w.r.t. the quaternion parameters
    for ( int k = 0; k < 4; k++ ){
      tmp[0] = N[i]*(S[3]*d1dq[0] + S[6]*d2dq[0]);
      tmp[1] = N[i]*(S[4]*d1dq[0] + S[7]*d2dq[0]);
      tmp[2] = N[i]*(S[5]*d1dq[0] + S[8]*d2dq[0]);
      
      tmp[3] = N[i]*(S[3]*d1dq[1] + S[6]*d2dq[1]);
      tmp[4] = N[i]*(S[4]*d1dq[1] + S[7]*d2dq[1]);
      tmp[5] = N[i]*(S[5]*d1dq[1] + S[8]*d2dq[1]);
      
      tmp[6] = N[i]*(S[3]*d1dq[2] + S[6]*d2dq[2]);
      tmp[7] = N[i]*(S[4]*d1dq[2] + S[7]*d2dq[2]);
      tmp[8] = N[i]*(S[5]*d1dq[2] + S[8]*d2dq[2]);
      TacsScalar dU[9];
      matTransMatMult(T, tmp, dU);
      
      // Compute T^{T}*d1dq*(e1^{T}*Xdinv*T*e1 + e2^{T}*z1Xdinv*T*e1)
      tmp[0] = (S[0]*Na[i] + z1Te1[1]*N[i])*d1dq[0] + z1Te1[2]*N[i]*d2dq[0];
      tmp[1] = (S[0]*Na[i] + z1Te1[1]*N[i])*d1dq[1] + z1Te1[2]*N[i]*d2dq[1];
      tmp[2] = (S[0]*Na[i] + z1Te1[1]*N[i])*d1dq[2] + z1Te1[2]*N[i]*d2dq[2];
      TacsScalar dTd1a[3];
      matMultTrans(T, tmp, dTd1a);

      // Compute T^{T}*d2dq*(e1^{T}*Xdinv*T*e1 + e3^{T}*z2Xdinv*T*e1)
      tmp[0] = (S[0]*Na[i] + z2Te1[2]*N[i])*d2dq[0] + z2Te1[1]*N[i]*d1dq[0];
      tmp[1] = (S[0]*Na[i] + z2Te1[2]*N[i])*d2dq[1] + z2Te1[1]*N[i]*d1dq[1];
      tmp[2] = (S[0]*Na[i] + z2Te1[2]*N[i])*d2dq[2] + z2Te1[1]*N[i]*d1dq[2];
      TacsScalar dTd2a[3];
      matMultTrans(T, tmp, dTd2a);

      // Compute the derivative
      b[0] = dU[0] + U0[0]*dU[0] + U0[3]*dU[3] + U0[6]*dU[6];
      b[1] = dTd1a[2] - dTd2a[1] + 
        (Td1a[0]*dU[2] + Td1a[1]*dU[5] + Td1a[2]*dU[8]) -
        (Td2a[0]*dU[1] + Td2a[1]*dU[4] + Td2a[2]*dU[7]) +
        (dTd1a[0]*U0[2] + dTd1a[1]*U0[5] + dTd1a[2]*U0[8]) -
        (dTd2a[0]*U0[1] + dTd2a[1]*U0[4] + dTd2a[2]*U0[7]);
      b[2] = dTd1a[0] + (dU[0]*Td1a[0] + dU[3]*Td1a[1] + dU[6]*Td1a[2] +
                         U0[0]*dTd1a[0] + U0[3]*dTd1a[1] + U0[6]*dTd1a[2]);
      b[3] = dTd2a[0] + (dU[0]*Td2a[0] + dU[3]*Td2a[1] + dU[6]*Td2a[2] +
                         U0[0]*dTd2a[0] + U0[3]*dTd2a[1] + U0[6]*dTd2a[2]);
      b[4] = b[5] = 0.0;
      b += NUM_STRESSES;

      // Increment the pointers to the derivatives of the quaternions
      d1dq += 3;
      d2dq += 3;
    }

    // Zero the contribution from the multiplier
    b[0] = b[1] = b[2] = b[3] = b[4] = b[5] = 0.0;
  }
}

void MITC3::addGmat( TacsScalar J[],
                     const TacsScalar scale,
                     const TacsScalar s[],
                     const double N[],
                     const double Na[],
                     const TacsScalar Ur[],
                     const TacsScalar d1a[],
                     const TacsScalar d2a[],
                     const TacsScalar Xdinv[],
                     const TacsScalar z1Xdinv[],
                     const TacsScalar z2Xdinv[],
                     const TacsScalar T[],
                     const TacsScalar Xr[],
                     const TacsScalar d1dq[],
                     const TacsScalar d2dq[] ){
  // Compute S = Xdinv*T
  TacsScalar S[9];
  matMatMult(Xdinv, T, S);
  
  // Compute U0 = T^{T}*Ur*Xdinv*T
  TacsScalar U0[9], tmp[9];
  matMatMult(Ur, Xdinv, U0);
  matMatMult(U0, T, tmp);
  matTransMatMult(T, tmp, U0);

  // Pick out the first column of the transformation matrix
  TacsScalar t[3];
  t[0] = T[0];
  t[1] = T[3];
  t[2] = T[6];

  // Compute z1Xdinv*T*e1
  TacsScalar z1Te1[3], Td1a[3];
  matMult(z1Xdinv, t, z1Te1);
  matMult(Ur, z1Te1, tmp);
  vecAxpy(S[0], d1a, tmp);
  matMultTrans(T, tmp, Td1a);

  // Compute z1Xdinv*T*e1
  TacsScalar z2Te1[3], Td2a[3];
  matMult(z2Xdinv, t, z2Te1);
  matMult(Ur, z2Te1, tmp);
  vecAxpy(S[0], d2a, tmp);
  matMultTrans(T, tmp, Td2a);

  // Create the arrays to store the derivatives
  TacsScalar DU[9*7*NUM_NODES];
  TacsScalar DT1[3*7*NUM_NODES], DT2[3*7*NUM_NODES];

  // Set the pointers 
  TacsScalar *dU0 = DU;
  TacsScalar *dTd1a = DT1;
  TacsScalar *dTd2a = DT2;
  for ( int i = 0; i < NUM_NODES; i++ ){
    for ( int k = 0; k < 3; k++ ){
      // Compute the derivative w.r.t. u
      dU0[0] = S[0]*T[3*k]*Na[i];
      dU0[1] = S[1]*T[3*k]*Na[i];
      dU0[2] = S[2]*T[3*k]*Na[i];
      dU0[3] = S[0]*T[3*k+1]*Na[i];
      dU0[4] = S[1]*T[3*k+1]*Na[i];
      dU0[5] = S[2]*T[3*k+1]*Na[i];
      dU0[6] = S[0]*T[3*k+2]*Na[i];
      dU0[7] = S[1]*T[3*k+2]*Na[i];
      dU0[8] = S[2]*T[3*k+2]*Na[i];

      dTd1a[0] = z1Te1[0]*T[3*k]*Na[i];
      dTd1a[1] = z1Te1[0]*T[3*k+1]*Na[i];
      dTd1a[2] = z1Te1[0]*T[3*k+2]*Na[i];

      dTd2a[0] = z2Te1[0]*T[3*k]*Na[i];
      dTd2a[1] = z2Te1[0]*T[3*k+1]*Na[i];
      dTd2a[2] = z2Te1[0]*T[3*k+2]*Na[i];

      dU0 += 9;
      dTd1a += 3;
      dTd2a += 3;
    }

    // Compute the derivatives w.r.t. the quaternion parameters
    for ( int k = 0; k < 4; k++ ){
      tmp[0] = N[i]*(S[3]*d1dq[0] + S[6]*d2dq[0]);
      tmp[1] = N[i]*(S[4]*d1dq[0] + S[7]*d2dq[0]);
      tmp[2] = N[i]*(S[5]*d1dq[0] + S[8]*d2dq[0]);
      
      tmp[3] = N[i]*(S[3]*d1dq[1] + S[6]*d2dq[1]);
      tmp[4] = N[i]*(S[4]*d1dq[1] + S[7]*d2dq[1]);
      tmp[5] = N[i]*(S[5]*d1dq[1] + S[8]*d2dq[1]);
      
      tmp[6] = N[i]*(S[3]*d1dq[2] + S[6]*d2dq[2]);
      tmp[7] = N[i]*(S[4]*d1dq[2] + S[7]*d2dq[2]);
      tmp[8] = N[i]*(S[5]*d1dq[2] + S[8]*d2dq[2]);
      matTransMatMult(T, tmp, dU0);
      
      // Compute T^{T}*d1dq*(e1^{T}*Xdinv*T*e1 + e2^{T}*z1Xdinv*T*e1)
      tmp[0] = (S[0]*Na[i] + z1Te1[1]*N[i])*d1dq[0] + z1Te1[2]*N[i]*d2dq[0];
      tmp[1] = (S[0]*Na[i] + z1Te1[1]*N[i])*d1dq[1] + z1Te1[2]*N[i]*d2dq[1];
      tmp[2] = (S[0]*Na[i] + z1Te1[1]*N[i])*d1dq[2] + z1Te1[2]*N[i]*d2dq[2];
      matMultTrans(T, tmp, dTd1a);

      // Compute T^{T}*d2dq*(e1^{T}*Xdinv*T*e1 + e3^{T}*z2Xdinv*T*e1)
      tmp[0] = (S[0]*Na[i] + z2Te1[2]*N[i])*d2dq[0] + z2Te1[1]*N[i]*d1dq[0];
      tmp[1] = (S[0]*Na[i] + z2Te1[2]*N[i])*d2dq[1] + z2Te1[1]*N[i]*d1dq[1];
      tmp[2] = (S[0]*Na[i] + z2Te1[2]*N[i])*d2dq[2] + z2Te1[1]*N[i]*d1dq[2];
      matMultTrans(T, tmp, dTd2a);

      dU0 += 9;
      dTd1a += 3;
      dTd2a += 3;
      d1dq += 3;
      d2dq += 3;
    }
  }

  // Compute the derivatives of the strain w.r.t. the displacement
  // variables. This code takes advantage of the sparsity of the
  // derivatives to simplify the computations
  for ( int i = 0; i < 7*NUM_NODES; i++ ){
    const TacsScalar *dUi = &DU[9*i];
    const TacsScalar *dT1i = &DT1[3*i];
    const TacsScalar *dT2i = &DT2[3*i];

    for ( int j = i; j < 7*NUM_NODES; j++ ){
      const TacsScalar *dUj = &DU[9*j];
      const TacsScalar *dT1j = &DT1[3*j];
      const TacsScalar *dT2j = &DT2[3*j];

      // Compute the real indices
      int ii = 8*(i/7) + (i%7);
      int jj = 8*(j/7) + (j%7);
      int idx = 8*NUM_NODES*ii + jj;
      int sym = 8*NUM_NODES*jj + ii;

      TacsScalar b[4];
      b[0] = dUi[0]*dUj[0] + dUi[3]*dUj[3] + dUi[6]*dUj[6];
      b[1] =
        (dT1i[0]*dUj[2] + dT1i[1]*dUj[5] + dT1i[2]*dUj[8]) -
        (dT2i[0]*dUj[1] + dT2i[1]*dUj[4] + dT2i[2]*dUj[7]) +
        (dT1j[0]*dUi[2] + dT1j[1]*dUi[5] + dT1j[2]*dUi[8]) -
        (dT2j[0]*dUi[1] + dT2j[1]*dUi[4] + dT2j[2]*dUi[7]);
      b[2] = (dUi[0]*dT1j[0] + dUi[3]*dT1j[1] + dUi[6]*dT1j[2] +
              dUj[0]*dT1i[0] + dUj[3]*dT1i[1] + dUj[6]*dT1i[2]);
      b[3] = (dUi[0]*dT2j[0] + dUi[3]*dT2j[1] + dUi[6]*dT2j[2] +
              dUj[0]*dT2i[0] + dUj[3]*dT2i[1] + dUj[6]*dT2i[2]);

      TacsScalar Jadd = 
        scale*(b[0]*s[0] + b[1]*s[1] + b[2]*s[2] + b[3]*s[3]);
      
      // Add the values symmetrically
      J[idx] += Jadd;
      if (ii != jj){
        J[sym] += Jadd;
      }
    }
  }

  // Add the contributions from the second derivatives of the
  // quaternions
  for ( int i = 0; i < NUM_NODES; i++ ){    
    // Extract the normals from the frame
    TacsScalar n1[3];
    n1[0] = Xr[1];
    n1[1] = Xr[4];
    n1[2] = Xr[7];

    TacsScalar n2[3];
    n2[0] = Xr[2];
    n2[1] = Xr[5];
    n2[2] = Xr[8];
    Xr += 9;
    
    // Compute the second derivative w.r.t. the quaternion
    TacsScalar dC1dq[3*9], dC2dq[3*9];
    computeQtr2ndDeriv(n1, dC1dq);
    computeQtr2ndDeriv(n2, dC2dq);
    const TacsScalar *dC1 = dC1dq, *dC2 = dC2dq;

    // Compute the partials derivatives w.r.t. eta,eps 
    for ( int ii = 0; ii < 9; ii++ ){
      tmp[0] = N[i]*(S[3]*dC1[0] + S[6]*dC2[0]);
      tmp[1] = N[i]*(S[4]*dC1[0] + S[7]*dC2[0]);
      tmp[2] = N[i]*(S[5]*dC1[0] + S[8]*dC2[0]);
      
      tmp[3] = N[i]*(S[3]*dC1[1] + S[6]*dC2[1]);
      tmp[4] = N[i]*(S[4]*dC1[1] + S[7]*dC2[1]);
      tmp[5] = N[i]*(S[5]*dC1[1] + S[8]*dC2[1]);
      
      tmp[6] = N[i]*(S[3]*dC1[2] + S[6]*dC2[2]);
      tmp[7] = N[i]*(S[4]*dC1[2] + S[7]*dC2[2]);
      tmp[8] = N[i]*(S[5]*dC1[2] + S[8]*dC2[2]);
      TacsScalar dU[9];
      matTransMatMult(T, tmp, dU);
      
      // Compute T^{T}*d1dq*(e1^{T}*Xdinv*T*e1 + e2^{T}*z1Xdinv*T*e1)
      tmp[0] = (S[0]*Na[i] + z1Te1[1]*N[i])*dC1[0] + z1Te1[2]*N[i]*dC2[0];
      tmp[1] = (S[0]*Na[i] + z1Te1[1]*N[i])*dC1[1] + z1Te1[2]*N[i]*dC2[1];
      tmp[2] = (S[0]*Na[i] + z1Te1[1]*N[i])*dC1[2] + z1Te1[2]*N[i]*dC2[2];
      TacsScalar dTd1a[3];
      matMultTrans(T, tmp, dTd1a);

      // Compute T^{T}*d2dq*(e1^{T}*Xdinv*T*e1 + e3^{T}*z2Xdinv*T*e1)
      tmp[0] = (S[0]*Na[i] + z2Te1[2]*N[i])*dC2[0] + z2Te1[1]*N[i]*dC1[0];
      tmp[1] = (S[0]*Na[i] + z2Te1[2]*N[i])*dC2[1] + z2Te1[1]*N[i]*dC1[1];
      tmp[2] = (S[0]*Na[i] + z2Te1[2]*N[i])*dC2[2] + z2Te1[1]*N[i]*dC1[2];
      TacsScalar dTd2a[3];
      matMultTrans(T, tmp, dTd2a);

      TacsScalar b[4];
      b[0] = dU[0] + U0[0]*dU[0] + U0[3]*dU[3] + U0[6]*dU[6];
      b[1] = dTd1a[2] - dTd2a[1] + 
        (Td1a[0]*dU[2] + Td1a[1]*dU[5] + Td1a[2]*dU[8]) -
        (Td2a[0]*dU[1] + Td2a[1]*dU[4] + Td2a[2]*dU[7]) +
        (dTd1a[0]*U0[2] + dTd1a[1]*U0[5] + dTd1a[2]*U0[8]) -
        (dTd2a[0]*U0[1] + dTd2a[1]*U0[4] + dTd2a[2]*U0[7]);
      b[2] = dTd1a[0] + (dU[0]*Td1a[0] + dU[3]*Td1a[1] + dU[6]*Td1a[2] +
                         U0[0]*dTd1a[0] + U0[3]*dTd1a[1] + U0[6]*dTd1a[2]);
      b[3] = dTd2a[0] + (dU[0]*Td2a[0] + dU[3]*Td2a[1] + dU[6]*Td2a[2] +
                         U0[0]*dTd2a[0] + U0[3]*dTd2a[1] + U0[6]*dTd2a[2]);

      TacsScalar Jadd = 
        scale*(b[0]*s[0] + b[1]*s[1] + b[2]*s[2] + b[3]*s[3]);

      if (ii < 3){
        int iv = 8*i + 3;
        int jv = 8*i + 4 + ii;
        J[(8*NUM_NODES)*iv + jv] += Jadd;
        J[(8*NUM_NODES)*jv + iv] += Jadd;
      }
      else {
        if (ii == 3){
          int iv = 8*i + 4;
          J[(8*NUM_NODES+1)*iv] += Jadd;
        }
        else if (ii == 4){
          int iv = 8*i + 4, jv = 8*i + 5;
          J[(8*NUM_NODES)*iv + jv] += Jadd;
          J[(8*NUM_NODES)*jv + iv] += Jadd;
        }
        else if (ii == 5){
          int iv = 8*i + 4, jv = 8*i + 6;
          J[(8*NUM_NODES)*iv + jv] += Jadd;
          J[(8*NUM_NODES)*jv + iv] += Jadd;
        }
        else if (ii == 6){
          int iv = 8*i + 5;
          J[(8*NUM_NODES+1)*iv] += Jadd;
        }
        else if (ii == 7){
          int iv = 8*i + 5, jv = 8*i + 6;
          J[(8*NUM_NODES)*iv + jv] += Jadd;
          J[(8*NUM_NODES)*jv + iv] += Jadd;
        }
        else if (ii == 8){
          int iv = 8*i + 6;
          J[(8*NUM_NODES+1)*iv] += Jadd;
        }
      }

      dC1 += 3;
      dC2 += 3;
    }
  }
}

/*
  Compute the value of the shear strain at the tying points in the
  element.

  input:
  X:       the initial values of the nodal coordinates 
  vars:    the values of the variables
  d1, d2:  the director values at every node in the element
  
  output:
  g12:     the values of the strain at the tying points
  g13:     the values of the strain at the tying points
*/
void MITC3::computeTyingStrain( TacsScalar g12[],
                                TacsScalar g13[],
                                const TacsScalar X[],
                                const TacsScalar Xr[],
                                const TacsScalar vars[],
                                const TacsScalar d1[],
                                const TacsScalar d2[] ){
  const double t = 0.577350269189626;
  const double upts[] = {-t,  t};

  for ( int pt = 0; pt < 2; pt++ ){
    // Evaluate the shape functions
    double N[NUM_NODES], Na[NUM_NODES];
    computeShapeFunc(upts[pt], N, Na);

    // Use the local frame to compute the 
    TacsScalar n1[3], n2[3];
    computeFrameNormals(N, Xr, n1, n2);

    // Assemble the frame at the current point
    TacsScalar Xa[3], Xd[9], Xdinv[9];
    innerProduct(Na, X, Xa);
    assembleFrame(Xa, n1, n2, Xd);
    inv3x3(Xd, Xdinv);

    // Compute the frame normals
    TacsScalar T[9];
    computeTransform(T, Xa);

    // Compute the derivative of U along the axial direction and
    // evaluate the director at the current point
    TacsScalar Ua[3];
    innerProduct8(Na, vars, Ua);

    // Compute the directors at the current location
    TacsScalar d1u[3], d2u[3];
    innerProduct(N, d1, d1u);
    innerProduct(N, d2, d2u);

    // Assemble the derivatives of the displacement w.r.t. the beam
    // parameters
    TacsScalar Ur[9];
    assembleFrame(Ua, d1u, d2u, Ur);

    // Compute U0 = T^{T}*Ur*Xdinv*T
    TacsScalar U0[9], tmp[9];
    matMatMult(Ur, Xdinv, U0);
    matMatMult(U0, T, tmp);
    matTransMatMult(T, tmp, U0);
  
    // Evaluate the strain at the tying point
    g12[pt] = U0[1] + U0[3] + U0[0]*U0[1] + U0[3]*U0[4] + U0[6]*U0[7];
    g13[pt] = U0[2] + U0[6] + U0[0]*U0[2] + U0[3]*U0[5] + U0[6]*U0[8];
  }
}

/*
  Compute the strain at the tying points and the derivative of the
  strain at the tying points to add into the B matrix
*/
void MITC3::computeTyingBmat( TacsScalar g12[],
                              TacsScalar g13[],
                              TacsScalar B12[],
                              TacsScalar B13[],
                              const TacsScalar X[],
                              const TacsScalar Xr[],
                              const TacsScalar vars[],
                              const TacsScalar d1[],
                              const TacsScalar d2[],
                              const TacsScalar dir1dq[],
                              const TacsScalar dir2dq[] ){
  const double t = 0.577350269189626;
  const double upts[] = {-t,  t};

  for ( int pt = 0; pt < 2; pt++ ){
    // Evaluate the shape functions
    double N[NUM_NODES], Na[NUM_NODES];
    computeShapeFunc(upts[pt], N, Na);

    // Use the local frame to compute the 
    TacsScalar n1[3], n2[3];
    computeFrameNormals(N, Xr, n1, n2);

    // Assemble the frame at the current point
    TacsScalar Xa[3], Xd[9], Xdinv[9];
    innerProduct(Na, X, Xa);
    assembleFrame(Xa, n1, n2, Xd);
    inv3x3(Xd, Xdinv);

    // Compute the local transformation
    TacsScalar T[9];
    computeTransform(T, Xa);

    // Compute S = Xdinv*T
    TacsScalar S[9];
    matMatMult(Xdinv, T, S);

    // Compute the derivative of U along the axial direction
    TacsScalar Ua[3];
    innerProduct8(Na, vars, Ua);

    // Compute the directors at the current location
    TacsScalar d1u[3], d2u[3];
    innerProduct(N, d1, d1u);
    innerProduct(N, d2, d2u);

    // Assemble the derivatives of the displacement
    TacsScalar Ur[9];
    assembleFrame(Ua, d1u, d2u, Ur);

    // Compute U0 = T^{T}*Ur*Xdinv*T
    TacsScalar U0[9], tmp[9];
    matMatMult(Ur, Xdinv, U0);
    matMatMult(U0, T, tmp);
    matTransMatMult(T, tmp, U0);

    // Evaluate the strain at the tying point
    g12[pt] = U0[1] + U0[3] + U0[0]*U0[1] + U0[3]*U0[4] + U0[6]*U0[7];
    g13[pt] = U0[2] + U0[6] + U0[0]*U0[2] + U0[3]*U0[5] + U0[6]*U0[8];
    
    TacsScalar *b12 = &B12[8*NUM_NODES*pt];
    TacsScalar *b13 = &B13[8*NUM_NODES*pt];
    const TacsScalar *d1dq = dir1dq;
    const TacsScalar *d2dq = dir2dq;
    for ( int i = 0; i < NUM_NODES; i++ ){
      for ( int k = 0; k < 3; k++ ){
        TacsScalar dU[9];
        dU[0] = S[0]*T[3*k]*Na[i];
        dU[1] = S[1]*T[3*k]*Na[i];
        dU[2] = S[2]*T[3*k]*Na[i];
        
        dU[3] = S[0]*T[3*k+1]*Na[i];
        dU[4] = S[1]*T[3*k+1]*Na[i];
        dU[5] = S[2]*T[3*k+1]*Na[i];
        
        dU[6] = S[0]*T[3*k+2]*Na[i];
        dU[7] = S[1]*T[3*k+2]*Na[i];
        dU[8] = S[2]*T[3*k+2]*Na[i];
      
        b12[0] = dU[1] + dU[3] + 
          (dU[0]*U0[1] + dU[3]*U0[4] + dU[6]*U0[7] +
           U0[0]*dU[1] + U0[3]*dU[4] + U0[6]*dU[7]);
        b13[0] = dU[2] + dU[6] +
          (dU[0]*U0[2] + dU[3]*U0[5] + dU[6]*U0[8] +
           U0[0]*dU[2] + U0[3]*dU[5] + U0[6]*dU[8]);
        b12++;  b13++;
      }

      for ( int k = 0; k < 4; k++ ){
        tmp[0] = N[i]*(S[3]*d1dq[0] + S[6]*d2dq[0]);
        tmp[1] = N[i]*(S[4]*d1dq[0] + S[7]*d2dq[0]);
        tmp[2] = N[i]*(S[5]*d1dq[0] + S[8]*d2dq[0]);
        
        tmp[3] = N[i]*(S[3]*d1dq[1] + S[6]*d2dq[1]);
        tmp[4] = N[i]*(S[4]*d1dq[1] + S[7]*d2dq[1]);
        tmp[5] = N[i]*(S[5]*d1dq[1] + S[8]*d2dq[1]);
        
        tmp[6] = N[i]*(S[3]*d1dq[2] + S[6]*d2dq[2]);
        tmp[7] = N[i]*(S[4]*d1dq[2] + S[7]*d2dq[2]);
        tmp[8] = N[i]*(S[5]*d1dq[2] + S[8]*d2dq[2]);
        TacsScalar dU[9];
        matTransMatMult(T, tmp, dU);
              
        b12[0] = dU[1] + dU[3] + 
          (dU[0]*U0[1] + dU[3]*U0[4] + dU[6]*U0[7] +
           U0[0]*dU[1] + U0[3]*dU[4] + U0[6]*dU[7]);
        b13[0] = dU[2] + dU[6] +
          (dU[0]*U0[2] + dU[3]*U0[5] + dU[6]*U0[8] +
           U0[0]*dU[2] + U0[3]*dU[5] + U0[6]*dU[8]);
        b12++;  b13++;

        d1dq += 3;
        d2dq += 3;
      }

      // Add the terms for the multipliers (zero)
      b12[0] = b13[0] = 0.0;
      b12++;  b13++;
    }
  }
}

/*
  Add the tying strain to the components of the strain
*/
void MITC3::addTyingStrain( TacsScalar e[],
                            const double N12[],
                            const TacsScalar g12[],
                            const TacsScalar g13[] ){
  e[4] = g12[0]*N12[0] + g12[1]*N12[1];
  e[5] = g13[0]*N12[0] + g13[1]*N12[1];
}

/*
  Add the tying components of the strain to the bmatrix
*/
void MITC3::addTyingBmat( TacsScalar B[],
                          const double N12[],
                          const TacsScalar B12[],
                          const TacsScalar B13[] ){
  const int offset = 8*NUM_NODES;
  for ( int k = 0; k < 8*NUM_NODES; k++ ){
    B[4] = B12[0]*N12[0] + B12[offset]*N12[1];
    B[5] = B13[0]*N12[0] + B13[offset]*N12[1];
    B += 6;
    B12++;
    B13++;
  }
}

/*
  Get the constitutive object
*/
TACSConstitutive *MITC3::getConstitutive(){
  return stiff;
}

/*
  Return the number of quadrature points
*/
int MITC3::getNumGaussPts(){ 
  return ORDER;
}

/*
  Return the quadrature points and weights
*/
double MITC3::getGaussWtsPts( const int num, double pt[] ){
  return gaussWts[num];
}

/*
  Get the values of the shape functions
*/
void MITC3::getShapeFunctions( const double pt[], double N[] ){
  double Na[3];
  computeShapeFunc(pt[0], N, Na);
}

/*
  Retrieve the determinant of the Jacobian transformation matrix
*/
TacsScalar MITC3::getDetJacobian( const double pt[],
                                  const TacsScalar X[] ){
  return 0.0;
}

/*
  Evaluate the strain at a parametric point within the element
*/
void MITC3::getStrain( TacsScalar e[],
                       const double pt[],
                       const TacsScalar X[],
                       const TacsScalar vars[] ){}

/*
  Add the derivative of the product of the array esens with the strain
  with respect to the state variables
*/
void MITC3::addStrainSVSens( TacsScalar sens[],
                             const double pt[], 
                             const TacsScalar scale,
                             const TacsScalar esens[], 
                             const TacsScalar X[],
                             const TacsScalar vars[] ){}

/*
  Determine the number of nodes and elements for visualization 
  generated by the data in this element. Note that higher-order
  elements are broken down into bi-linear elements for visualization.

  output:
  nelems:  the number of visualization elements
  nnodes:  the number of nodes used for visualization
  ncsr:    the number of entries in a CSR-type data structure used
  to store the connectivity
*/
void MITC3::addOutputCount( int * nelems, 
                            int * nnodes, int * ncsr ){
  *nelems += 2;
  *nnodes += 3;
  *ncsr += 4;
}

/*
  Get the output data from this element and place it in a real
  array for visualization later. The values generated for visualization
  are determined by a bit-wise selection variable 'out_type' which is 
  can be used to simultaneously write out different data. Note that this
  is why the bitwise operation & is used below. 

  The output may consist of the following:
  - the nodal locations
  - the displacements and rotations
  - the strains or strains within the element
  - extra variables that are used for optimization
  
  output:
  data:     the data to write to the file (eventually)

  input:
  out_type: the bit-wise variable used to specify what data to generate
  vars:     the element variables
  Xpts:     the element nodal locations
*/
void MITC3::getOutputData( unsigned int out_type,
                           double * data, int ld_data,
                           const TacsScalar Xpts[],
                           const TacsScalar vars[] ){
  for ( int n = 0; n < 3; n++ ){
    double pt[2];
    pt[0] = -1.0 + 1.0*n;

    TacsScalar strain[6], stress[6];
    getStrain(strain, pt, Xpts, vars);
      
    int index = 0;
    if (out_type & TACSElement::OUTPUT_NODES){
      for ( int k = 0; k < 3; k++ ){
        data[index+k] = TacsRealPart(Xpts[3*n+k]);
      }
      index += 3;
    }
    if (out_type & TACSElement::OUTPUT_DISPLACEMENTS){
      for ( int k = 0; k < NUM_DISPS; k++ ){
        data[index+k] = TacsRealPart(vars[NUM_DISPS*n+k]);
      }
      index += NUM_DISPS;
    }
    if (out_type & TACSElement::OUTPUT_STRAINS){
      // Add the term due to the potential energy
      for ( int k = 0; k < NUM_STRESSES; k++ ){
        data[index+k] = TacsRealPart(strain[k]);
      }
      index += NUM_STRESSES;
    }
    if (out_type & TACSElement::OUTPUT_STRESSES){
      // Evaluate the stiffness at the current point 
      // and then calculate the stress
      stiff->calculateStress(pt, strain, stress);
      
      for ( int k = 0; k < NUM_STRESSES; k++ ){
        data[index+k] = TacsRealPart(stress[k]);
      }
      index += NUM_STRESSES;
    }
    if (out_type & TACSElement::OUTPUT_EXTRAS){
      // Compute the failure value
      TacsScalar lambda;
      stiff->failure(pt, strain, &lambda);
      data[index] = TacsRealPart(lambda);
      
      // Compute the buckling constraint value
      TacsScalar bval;
      stiff->buckling(strain, &bval);
      data[index+1] = TacsRealPart(bval);
      
      data[index+2] = TacsRealPart(stiff->getDVOutputValue(0, pt));
      data[index+3] = TacsRealPart(stiff->getDVOutputValue(1, pt));
      
      index += NUM_EXTRAS;
    }
    if (out_type & TACSElement::OUTPUT_COORDINATES){
      index += 9;
    }
    
    data += ld_data;    
  }  
}

/*
  Get the element connectivity for visualization purposes. Since each
  element consists of a series of sub-elements used for visualization,
  we also need the connectivity of these visualization elements.

  output:
  con:  the connectivity of the local visualization elements contributed
  by this finite-element

  input:
  node:  the node offset number - so that this connectivity is more or 
  less global
*/
void MITC3::getOutputConnectivity( int * con, int node ){
  con[0] = node;
  con[1] = node+1;
  con[2] = node+1;
  con[3] = node+2;
}

/*
  Test the strain implementation for the element
*/
void MITC3::testStrain( const TacsScalar X[] ){
  TacsScalar vars[8*NUM_NODES];
  memset(vars, 0, sizeof(vars));

  // Pick a random set of values for the quaternions 
  TacsScalar q[4];
  q[1] = 0.25;
  q[2] = 0.5;
  q[3] = 0.125;
  q[0] = sqrt(1.0 - vecDot(&q[1], &q[1]));

  // Compute the rotation matrix C
  TacsScalar C[9];
  computeRotationMat(q[0], &q[1], C);

  // Set the rigid displacement
  TacsScalar u0[3] = {1.25, -2.5, -4.0};

  // Compute the variables and set the quaternion values
  for ( int k = 0; k < NUM_NODES; k++ ){
    // Compute the displacements
    matMultTrans(C, &X[3*k], &vars[8*k]);
    for ( int i = 0; i < 3; i++ ){
      vars[8*k+i] += u0[i] - X[3*k+i];
    }
    
    // Copy the values of the quaternions
    memcpy(&vars[8*k+3], q, 4*sizeof(TacsScalar));
  }

  // Now compute the strain for the rigid rotation
  const double u = -0.139;

  // Compute the reference frames at the nodes
  TacsScalar Xr[9*NUM_NODES];
  computeFrames(Xr, X);

  // Compute the directors at the nodes
  TacsScalar d1[3*NUM_NODES], d2[3*NUM_NODES];
  TacsScalar d1dq[12*NUM_NODES], d2dq[12*NUM_NODES];
  computeDirectors(d1, d2, vars, Xr);
  computeDirectorDeriv(d1dq, d2dq, vars, Xr);

  // Compute the strain at the tying points
  TacsScalar g12[2], g13[2];
  TacsScalar B12[2*8*NUM_NODES], B13[2*8*NUM_NODES];
  computeTyingBmat(g12, g13, B12, B13,
                   X, Xr, vars, d1, d2, d1dq, d2dq);

  // Evaluate the shape functions
  double N[NUM_NODES], Na[NUM_NODES];
  computeShapeFunc(u, N, Na);

  // Use the local frame to compute the 
  TacsScalar n1[3], n2[3];
  computeFrameNormals(N, Xr, n1, n2);

  // Assemble the frame at the current point
  TacsScalar Xa[3], Xd[9], Xdinv[9];
  innerProduct(Na, X, Xa);
  assembleFrame(Xa, n1, n2, Xd);
  inv3x3(Xd, Xdinv);

  // Compute d(Xdinv)/dz1 and d(Xdinv)/dz2
  TacsScalar z1Xdinv[9], z2Xdinv[9];
  computeFrameRateNormals(Na, Xr, Xdinv, z1Xdinv, z2Xdinv);

  // Compute the frame normals
  TacsScalar T[9];
  computeTransform(T, Xa);

  // Compute the derivative of U along the axial direction and
  // evaluate the director at the current point
  TacsScalar Ua[3], d1u[3], d2u[3];
  innerProduct8(Na, vars, Ua);
  innerProduct(N, d1, d1u);
  innerProduct(N, d2, d2u);
  
  // Derivatives of the displacement w.r.t. the beam parameters
  TacsScalar Ur[9];
  assembleFrame(Ua, d1u, d2u, Ur);
  
  // Derivative of the directors d1 and d2 along the axial direction
  TacsScalar d1a[3], d2a[3];
  innerProduct(Na, d1, d1a);
  innerProduct(Na, d2, d2a);

  // Compute the displacement-based strain
  TacsScalar e[6];
  evalStrain(e, Ur, d1a, d2a, Xdinv, z1Xdinv, z2Xdinv, T);
  
  // Add the contribution from the tying strain
  double N12[2];
  computeTyingFunc(u, N12);
  addTyingStrain(e, N12, g12, g13);

  // Set a fake finite-difference - this should be zero
  TacsScalar fd[6] = {0.0, 0.0, 0.0, 
                      0.0, 0.0, 0.0};

  // Write out the error components
  char descript[64];
  sprintf(descript, "strain after rigid rotation");
  writeErrorComponents(stdout, descript,
                       e, fd, 6);

  // Compute the bmatrix 
  TacsScalar B[6*8*NUM_NODES];
  evalBmat(e, B, N, Na, Ur, d1a, d2a,
           Xdinv, z1Xdinv, z2Xdinv, T, d1dq, d2dq);
  addTyingBmat(B, N12, B12, B13);

  // Compute the derivative of the strain w.r.t. to test the
  // implementation of the b matrix
  double dh = 1e-6;

  for ( int k = 0; k < 8*NUM_NODES; k++ ){
    TacsScalar vtmp = vars[k];
    vars[k] = vtmp + dh;

    // Compute the directors at the nodes
    computeDirectors(d1, d2, vars, Xr);

    // Compute the shear strain at the tying points
    computeTyingStrain(g12, g13, X, Xr, vars, d1, d2);

    // Compute the derivative of U along the axial direction and
    // evaluate the director at the current point
    innerProduct8(Na, vars, Ua);
    innerProduct(N, d1, d1u);
    innerProduct(N, d2, d2u);
    assembleFrame(Ua, d1u, d2u, Ur);

    innerProduct(Na, d1, d1a);
    innerProduct(Na, d2, d2a);
    evalStrain(fd, Ur, d1a, d2a, Xdinv, z1Xdinv, z2Xdinv, T);
  
    // Add the contribution from the tying strain
    addTyingStrain(fd, N12, g12, g13);

    for ( int i = 0; i < 6; i++ ){
      fd[i] = (fd[i] - e[i])/dh;
    }

    vars[k] = vtmp;
    
    // Write out the error components
    char descript[64];
    sprintf(descript, "B%d", k);
    writeErrorComponents(stdout, descript,
                         &B[6*k], fd, 6);
  }
}
