/**
 * @file 	integrator.c
 * @brief 	Mikkola integration scheme.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
 * @detail	This file implements the Mikkola integration scheme.  
 * 
 * @section 	LICENSE
 * Copyright (c) 2015 Hanno Rein
 *
 * This file is part of rebound.
 *
 * rebound is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rebound is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "particle.h"
#include "main.h"
#include "gravity.h"
#include "boundaries.h"

// These variables have no effect for leapfrog.
int integrator_force_is_velocitydependent 	= 1;
double integrator_epsilon 			= 0;
double integrator_min_dt 			= 0;


double integrator_megno_Ys;
double integrator_megno_Yss;
double integrator_megno_cov_Yt;	// covariance of <Y> and t
double integrator_megno_var_t;  // variance of t 
double integrator_megno_mean_t; // mean of t
double integrator_megno_mean_Y; // mean of Y
long   integrator_megno_n; 	// number of covariance updates
void integrator_megno_init(double delta){
	int _N_megno = N;
	integrator_megno_Ys = 0.;
	integrator_megno_Yss = 0.;
	integrator_megno_cov_Yt = 0.;
	integrator_megno_var_t = 0.;
	integrator_megno_n = 0;
	integrator_megno_mean_Y = 0;
	integrator_megno_mean_t = 0;
        for (int i=0;i<_N_megno;i++){ 
                struct particle megno = {
			.m  = particles[i].m,
			.x  = delta*tools_normal(1.),
			.y  = delta*tools_normal(1.),
			.z  = delta*tools_normal(1.),
			.vx = delta*tools_normal(1.),
			.vy = delta*tools_normal(1.),
			.vz = delta*tools_normal(1.) };
                particles_add(megno);
        }
	N_megno = _N_megno;
}
double integrator_megno(){ // Returns the MEGNO <Y>
	if (t==0.) return 0.;
	return integrator_megno_Yss/t;
}
double integrator_lyapunov(){ // Returns the largest Lyapunov characteristic number (LCN), or maximal Lyapunov exponent
	if (t==0.) return 0.;
	return integrator_megno_cov_Yt/integrator_megno_var_t;
}
double integrator_megno_deltad_delta2(){
        double deltad = 0;
        double delta2 = 0;
        for (int i=N-N_megno;i<N;i++){
                deltad += particles[i].vx * particles[i].x; 
                deltad += particles[i].vy * particles[i].y; 
                deltad += particles[i].vz * particles[i].z; 
                deltad += particles[i].ax * particles[i].vx; 
                deltad += particles[i].ay * particles[i].vy; 
                deltad += particles[i].az * particles[i].vz; 
                delta2 += particles[i].x  * particles[i].x; 
                delta2 += particles[i].y  * particles[i].y;
                delta2 += particles[i].z  * particles[i].z;
                delta2 += particles[i].vx * particles[i].vx; 
                delta2 += particles[i].vy * particles[i].vy;
                delta2 += particles[i].vz * particles[i].vz;
        }
        return deltad/delta2;
}
void integrator_megno_calculate_acceleration(){
#pragma omp parallel for schedule(guided)
	for (int i=N-N_megno; i<N; i++){
	for (int j=N-N_megno; j<N; j++){
		if (i==j) continue;
		const double dx = particles[i-N/2].x - particles[j-N/2].x;
		const double dy = particles[i-N/2].y - particles[j-N/2].y;
		const double dz = particles[i-N/2].z - particles[j-N/2].z;
		const double r = sqrt(dx*dx + dy*dy + dz*dz + softening*softening);
		const double r3inv = 1./(r*r*r);
		const double r5inv = 3./(r*r*r*r*r);
		const double ddx = particles[i].x - particles[j].x;
		const double ddy = particles[i].y - particles[j].y;
		const double ddz = particles[i].z - particles[j].z;
		const double Gm = G * particles[j].m;
		
		// Variational equations
		particles[i].ax += Gm * (
			+ ddx * ( dx*dx*r5inv - r3inv )
			+ ddy * ( dx*dy*r5inv )
			+ ddz * ( dx*dz*r5inv )
			);

		particles[i].ay += Gm * (
			+ ddx * ( dy*dx*r5inv )
			+ ddy * ( dy*dy*r5inv - r3inv )
			+ ddz * ( dy*dz*r5inv )
			);

		particles[i].az += Gm * (
			+ ddx * ( dz*dx*r5inv )
			+ ddy * ( dz*dy*r5inv )
			+ ddz * ( dz*dz*r5inv - r3inv )
			);
	}
	}
}

// Fast inverse factorial lookup table
static const double invfactorial[] = {1., 1., 1./2., 1./6., 1./24., 1./120., 1./720., 1./5040., 1./40320., 1./362880., 1./3628800., 1./39916800., 1./479001600., 1./6227020800., 1./87178291200., 1./1307674368000., 1./20922789888000., 1./355687428096000., 1./6402373705728000., 1./121645100408832000., 1./2432902008176640000., 1./51090942171709440000., 1./1124000727777607680000., 1./25852016738884976640000., 1./620448401733239439360000., 1./15511210043330985984000000., 1./403291461126605635584000000., 1./10888869450418352160768000000., 1./304888344611713860501504000000., 1./8841761993739701954543616000000., 1./265252859812191058636308480000000., 1./8222838654177922817725562880000000., 1./263130836933693530167218012160000000., 1./8683317618811886495518194401280000000., 1./295232799039604140847618609643520000000.};

double ipow(double base, unsigned int exp) {
	double result = 1;
	while (exp) {
		if (exp & 1)
		    result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

double c_n_series(unsigned int n, double z){
	double c_n = 0.;
	for (unsigned int j=0;j<13;j++){
		double term = ipow(-z,j)*invfactorial[n+2*j];
		c_n += term;
		if (fabs(term/c_n) < 1e-17) break; // Stop if new term smaller than machine precision
	}
	return c_n;
}

double c(unsigned int n, double z){
	if (z>0.5){
		double z4 = z/4.;
		// Speed up conversion with 4-folding formula
		switch(n){
			case 0:
			{
				double cn4 = c(3,z4)*(1.+c(1,z4))/8.;
				double cn2 = 1./2.-z*cn4;
				double cn0 = 1.-z*cn2;
				return cn0;
			}
			case 1:
			{
				double cn5 = (c(5,z4)+c(4,z4)+c(3,z4)*c(2,z4))/16.;
				double cn3 = 1./6.-z*cn5;
				double cn1 = 1.-z*cn3;
				return cn1;
			}
			case 2:
			{
				double cn4 = c(3,z4)*(1.+c(1,z4))/8.;
				double cn2 = 1./2.-z*cn4;
				return cn2;
			}
			case 3:
			{
				double cn5 = (c(5,z4)+c(4,z4)+c(3,z4)*c(2,z4))/16.;
				double cn3 = 1./6.-z*cn5;
				return cn3;
			}
			case 4:
			{
				double cn4 = c(3,z4)*(1.+c(1,z4))/8.;
				return cn4;
			}
			case 5:
			{
				double cn5 = (c(5,z4)+c(4,z4)+c(3,z4)*c(2,z4))/16.;
				return cn5;
			}
		}
	}
	return c_n_series(n,z);
}

double integrator_G(unsigned int n, double beta, double X){
	return pow(X,n)*c(n,beta*X*X);
}

struct particle* p_j  = NULL;
double* eta = NULL;
double _M(int i){
	return G*(eta[i]); // Hanno 1
	//return G*(eta[i-1]); // Hanno2 
	//return G*(eta[i-1]*particles[i].m*eta[i-1]/eta[i]/(eta[i-1]+particles[i].m*eta[i-1]/eta[i])); // reduced mass jacobi
	//return G*(eta[i-1]*particles[i].m/(eta[i-1]+particles[i].m)); // reduced mass
	//return G*(eta[i]/eta[i-1]*particles[0].m);   // SSD
}
void kepler_step(int i,double _dt){
	double M = _M(i);
	struct particle p1 = p_j[i];

	double r0 = sqrt(p1.x*p1.x + p1.y*p1.y + p1.z*p1.z);
	double v2 =  p1.vx*p1.vx + p1.vy*p1.vy + p1.vz*p1.vz;
	double beta = 2.*M/r0 - v2;
	double eta0 = p1.x*p1.vx + p1.y*p1.vy + p1.z*p1.vz;
	double zeta0 = M - beta*r0;
	

	double X = 0;  // TODO: find a better initial estimate.
	double G1,G2,G3;
	for (int n_hg=0;n_hg<20;n_hg++){
		G2 = integrator_G(2,beta,X);
		G3 = integrator_G(3,beta,X);
		G1 = X-beta*G3;
		double s   = r0*X + eta0*G2 + zeta0*G3-_dt;
		double sp  = r0 + eta0*G1 + zeta0*G2;
		double dX  = -s/sp; // Newton's method
		
		//double G0 = 1.-beta*G2;
		//double spp = r0 + eta0*G0 + zeta0*G1;
		//double dX  = -(s*sp)/(sp*sp-0.5*s*spp); // Householder 2nd order formula
		X+=dX;
		if (fabs(dX/X)<1e-15) break; // TODO: Make sure loop does not get stuck (add maximum number of iterations) 
	}

	double r = r0 + eta0*G1 + zeta0*G2;
	double f = 1.-M*G2/r0;
	double g = _dt - M*G3;
	double fd = -M*G1/(r0*r); 
	double gd = 1.-M*G2/r; 

	p_j[i].x = f*p1.x + g*p1.vx;
	p_j[i].y = f*p1.y + g*p1.vy;
	p_j[i].z = f*p1.z + g*p1.vz;

	p_j[i].vx = fd*p1.x + gd*p1.vx;
	p_j[i].vy = fd*p1.y + gd*p1.vy;
	p_j[i].vz = fd*p1.z + gd*p1.vz;
	
	//Variations
	if (N_megno){
		struct particle dp1 = p_j[i+N_megno];
		double dr0 = (dp1.x*p1.x + dp1.y*p1.y + dp1.z*p1.z)/r0;
		double dbeta = -2.*M*dr0/(r0*r0) - 2.* (dp1.vx*p1.vx + dp1.vy*p1.vy + dp1.vz*p1.vz);
		double deta0 = dp1.x*p1.vx + dp1.y*p1.vy + dp1.z*p1.vz
			     + p1.x*dp1.vx + p1.y*dp1.vy + p1.z*dp1.vz;
		double dzeta0 = -beta*dr0 - r0*dbeta;
		double G0 = integrator_G(0,beta,X);
		double G4 = integrator_G(4,beta,X);
		double G5 = integrator_G(5,beta,X);
		double G3beta = 0.5*(3.*G5-X*G4);
		double G2beta = 0.5*(2.*G4-X*G3);
		double G1beta = 0.5*(G3-X*G2);
		double tbeta = eta0*G2beta + zeta0*G3beta;
		double dX = -1./r*(X*dr0 + G2*deta0+G3*dzeta0+tbeta*dbeta);
		double dG1 = G0*dX + G1beta*dbeta; 
		double dG2 = G1*dX + G2beta*dbeta;
		double dG3 = G2*dX + G3beta*dbeta;
		double dr = dr0 + G1*deta0 + G2*dzeta0 + eta0*dG1 + zeta0*dG2;
		double df = M*G2*dr0/(r0*r0) - M*dG2/r0;
		double dg = -M*dG3;
		double dfd = -M*dG1/(r0*r) + M*G1*(dr0/r0+dr/r)/(r*r0);
		double dgd = -M*dG2/r + M*G2*dr/(r*r);
		
		p_j[i+N_megno].x = f*dp1.x + g*dp1.vx + df*p1.x + dg*p1.vx;
		p_j[i+N_megno].y = f*dp1.y + g*dp1.vy + df*p1.y + dg*p1.vy;
		p_j[i+N_megno].z = f*dp1.z + g*dp1.vz + df*p1.z + dg*p1.vz;

		p_j[i+N_megno].vx = fd*dp1.x + gd*dp1.vx + dfd*p1.x + dgd*p1.vx;
		p_j[i+N_megno].vy = fd*dp1.y + gd*dp1.vy + dfd*p1.y + dgd*p1.vy;
		p_j[i+N_megno].vz = fd*dp1.z + gd*dp1.vz + dfd*p1.z + dgd*p1.vz;
	}

}

void integrator_to_jacobi_posvel(){
	double s_x = particles[0].m * particles[0].x;
	double s_y = particles[0].m * particles[0].y;
	double s_z = particles[0].m * particles[0].z;
	double s_vx = particles[0].m * particles[0].vx;
	double s_vy = particles[0].m * particles[0].vy;
	double s_vz = particles[0].m * particles[0].vz;
	for (int i=1;i<N;i++){
		p_j[i].x = particles[i].x - s_x/eta[i-1];
		p_j[i].y = particles[i].y - s_y/eta[i-1];
		p_j[i].z = particles[i].z - s_z/eta[i-1];
		p_j[i].vx = particles[i].vx - s_vx/eta[i-1];
		p_j[i].vy = particles[i].vy - s_vy/eta[i-1];
		p_j[i].vz = particles[i].vz - s_vz/eta[i-1];
		s_x += particles[i].m * particles[i].x;
		s_y += particles[i].m * particles[i].y;
		s_z += particles[i].m * particles[i].z;
		s_vx += particles[i].m * particles[i].vx;
		s_vy += particles[i].m * particles[i].vy;
		s_vz += particles[i].m * particles[i].vz;
	}
	p_j[0].x = s_x / eta[N-1];
	p_j[0].y = s_y / eta[N-1];
	p_j[0].z = s_z / eta[N-1];
	p_j[0].vx = s_vx / eta[N-1];
	p_j[0].vy = s_vy / eta[N-1];
	p_j[0].vz = s_vz / eta[N-1];
}


void integrator_to_jacobi_acc(){
	double s_ax = particles[0].m * particles[0].ax;
	double s_ay = particles[0].m * particles[0].ay;
	double s_az = particles[0].m * particles[0].az;
	for (int i=1;i<N;i++){
		p_j[i].ax = particles[i].ax - s_ax/eta[i-1];
		p_j[i].ay = particles[i].ay - s_ay/eta[i-1];
		p_j[i].az = particles[i].az - s_az/eta[i-1];
		s_ax += particles[i].m * particles[i].ax;
		s_ay += particles[i].m * particles[i].ay;
		s_az += particles[i].m * particles[i].az;
	}
	p_j[0].ax = s_ax / eta[N-1];
	p_j[0].ay = s_ay / eta[N-1];
	p_j[0].az = s_az / eta[N-1];
}

void integrator_to_heliocentric_posvel(){
	double s_x = 0.;
	double s_y = 0.;
	double s_z = 0.;
	double s_vx = 0.;
	double s_vy = 0.;
	double s_vz = 0.;
	for (int i=N-1;i>0;i--){
		particles[i].x = p_j[0].x + eta[i-1]/eta[i] * p_j[i].x - s_x;
		particles[i].y = p_j[0].y + eta[i-1]/eta[i] * p_j[i].y - s_y;
		particles[i].z = p_j[0].z + eta[i-1]/eta[i] * p_j[i].z - s_z;
		particles[i].vx = p_j[0].vx + eta[i-1]/eta[i] * p_j[i].vx- s_vx;
		particles[i].vy = p_j[0].vy + eta[i-1]/eta[i] * p_j[i].vy- s_vy;
		particles[i].vz = p_j[0].vz + eta[i-1]/eta[i] * p_j[i].vz- s_vz;
		s_x += particles[i].m/eta[i] * p_j[i].x;
		s_y += particles[i].m/eta[i] * p_j[i].y;
		s_z += particles[i].m/eta[i] * p_j[i].z;
		s_vx += particles[i].m/eta[i] * p_j[i].vx;
		s_vy += particles[i].m/eta[i] * p_j[i].vy;
		s_vz += particles[i].m/eta[i] * p_j[i].vz;
	}
	particles[0].x = p_j[0].x - s_x;
	particles[0].y = p_j[0].y - s_y;
	particles[0].z = p_j[0].z - s_z;
	particles[0].vx = p_j[0].vx - s_vx;
	particles[0].vy = p_j[0].vy - s_vy;
	particles[0].vz = p_j[0].vz - s_vz;
}

void integrator_to_heliocentric_pos(){
	double s_x = 0.;
	double s_y = 0.;
	double s_z = 0.;
	for (int i=N-1;i>0;i--){
		particles[i].x = p_j[0].x + eta[i-1]/eta[i] * p_j[i].x - s_x;
		particles[i].y = p_j[0].y + eta[i-1]/eta[i] * p_j[i].y - s_y;
		particles[i].z = p_j[0].z + eta[i-1]/eta[i] * p_j[i].z - s_z;
		s_x += particles[i].m/eta[i] * p_j[i].x;
		s_y += particles[i].m/eta[i] * p_j[i].y;
		s_z += particles[i].m/eta[i] * p_j[i].z;
	}
	particles[0].x = p_j[0].x - s_x;
	particles[0].y = p_j[0].y - s_y;
	particles[0].z = p_j[0].z - s_z;
}

void integrator_interaction(double _dt){
	for (int i=1;i<N;i++){
		// Eq 132
		double rj3 = pow(p_j[i].x*p_j[i].x + p_j[i].y*p_j[i].y + p_j[i].z*p_j[i].z,3./2.);
		double M = _M(i);
		double prefac1 = M/rj3;
		p_j[i].vx += _dt * (p_j[i].ax + prefac1*p_j[i].x);
		p_j[i].vy += _dt * (p_j[i].ay + prefac1*p_j[i].y);
		p_j[i].vz += _dt * (p_j[i].az + prefac1*p_j[i].z);
	}
}

void integrator_part1(){
	if (p_j==NULL){
		p_j = malloc(sizeof(struct particle)*N);
		eta = malloc(sizeof(double)*N);
		eta[0] = particles[0].m;
		for (int i=1;i<N;i++){
			eta[i] = eta[i-1] + particles[i].m;
		}
	}
	integrator_to_jacobi_posvel();

	for (int i=1;i<N;i++){
		kepler_step(i, dt/2.);
	}
	p_j[0].x += dt/2.*p_j[0].vx;
	p_j[0].y += dt/2.*p_j[0].vy;
	p_j[0].z += dt/2.*p_j[0].vz;

	if (integrator_force_is_velocitydependent){
		integrator_to_heliocentric_posvel();
	}else{
		integrator_to_heliocentric_pos();
	}
	t+=dt/2.;
}

void integrator_part2(){
	integrator_to_jacobi_acc();
	integrator_interaction(dt);

	for (int i=1;i<N;i++){
		kepler_step(i, dt/2.);
	}
	p_j[0].x += dt/2.*p_j[0].vx;
	p_j[0].y += dt/2.*p_j[0].vy;
	p_j[0].z += dt/2.*p_j[0].vz;

	t+=dt/2.;
	integrator_to_heliocentric_posvel();
}
	

