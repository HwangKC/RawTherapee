/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <curves.h>
#include <math.h>
#include <vector>
#include <mytime.h>
#include <string.h>

#define CLIPD(a) ((a)>0.0?((a)<1.0?(a):1.0):0.0)
#define CLIP(a) ((a)<65535 ? (a) : (65535))


namespace rtengine {

Curve::Curve () {
	x = 0;
	y = 0;
	ypp = 0;
}

void Curve::AddPolygons ()
{
	if (firstPointIncluded) {
		poly_x.push_back(x1);
		poly_y.push_back(y1);
	}
	for (int k=1; k<(nbr_points-1); k++) {
		double t = k*increment;
		double t2 = t*t;
		double tr = 1.-t;
		double tr2 = tr*tr;
		double tr2t = tr*2*t;

		// adding a point to the polyline
		poly_x.push_back( tr2*x1 + tr2t*x2 + t2*x3);
		poly_y.push_back( tr2*y1 + tr2t*y2 + t2*y3);
	}
	// adding the last point of the sub-curve
	poly_x.push_back(x3);
	poly_y.push_back(y3);
}


void CurveFactory::complexsgnCurve (double saturation, bool satlimit, double satlimthresh, const std::vector<double>& curvePoints, float* outCurve, int skip) {
	
	//colormult = chroma_scale for Lab manipulations
	
	// check if contrast curve is needed
	bool needsaturation = (saturation<-0.0001 || saturation>0.0001);
	
	// curve without contrast
	double* dcurve = new double[65536];
	
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	std::vector<double> satcurvePoints;
	satcurvePoints.push_back((double)DCT_NURBS);
	if (saturation>0) {
		double satslope = (0.5+2*saturation/500.0)/(0.5-2*saturation/500.0);
		double scale = (satlimthresh/100.1);
		if (!satlimit) scale=100/100.1;
		
		satcurvePoints.push_back(0); //black point.  Value in [0 ; 1] range
		satcurvePoints.push_back(0); //black point.  Value in [0 ; 1] range
		
		//if (satlimit) {
			satcurvePoints.push_back(0.5-0.5*scale); //toe point
			satcurvePoints.push_back(0.5-0.5*scale); //value at toe point
			
			satcurvePoints.push_back(0.5-(0.5/satslope)*scale); //toe point
			satcurvePoints.push_back(0.5-0.5*scale); //value at toe point
			
			satcurvePoints.push_back(0.5+(0.5/satslope)*scale); //shoulder point
			satcurvePoints.push_back(0.5+0.5*scale); //value at shoulder point
						
			satcurvePoints.push_back(0.5+0.5*scale); //shoulder point
			satcurvePoints.push_back(0.5+0.5*scale); //value at shoulder point			
		/*} else {
			satcurvePoints.push_back(0.25+saturation/500.0); //toe point
			satcurvePoints.push_back(0.25-saturation/500.0); //value at toe point
			
			satcurvePoints.push_back(0.75-saturation/500.0); //shoulder point
			satcurvePoints.push_back(0.75+saturation/500.0); //value at shoulder point
		}*/
		
		satcurvePoints.push_back(1); // white point
		satcurvePoints.push_back(1); // value at white point
	} else {
		satcurvePoints.push_back(0); 
		satcurvePoints.push_back(-(saturation/200.0)); 
		
		satcurvePoints.push_back(1); 
		satcurvePoints.push_back(1+saturation/200.0); 
	}
	DiagonalCurve* satcurve = NULL;
	satcurve = new DiagonalCurve (satcurvePoints, CURVES_MIN_POLY_POINTS/skip); // Actually, CURVES_MIN_POLY_POINTS = 1000,
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	
	// create a curve if needed
	DiagonalCurve* tcurve = NULL;
	if (curvePoints.size()>0 && curvePoints[0]!=0)
		tcurve = new DiagonalCurve (curvePoints, CURVES_MIN_POLY_POINTS/skip);
	
	for (int i=0; i<=0xffff; i+= i<0xffff-skip ? skip : 1 ) {
		
		// change to [0,1] range
		double val = (double)i / 65535.0;
		
		// apply saturation curve
		if (needsaturation)
			val = satcurve->getVal (val);
		
		// apply custom/parametric/NURBS curve, if any
		if (tcurve) {
			val = tcurve->getVal (val);
		}
		
		// store result in a temporary array
		dcurve[i] = CLIPD(val);
	}
	delete tcurve;
	
	// if skip>1, let apply linear interpolation in the skipped points of the curve
	int prev = 0;
	for (int i=1; i<=0xffff-skip; i++) {
		if (i%skip==0) {
			prev+=skip;
			continue;
		}
		dcurve[i] = ( dcurve[prev] * (skip - i%skip) + dcurve[prev+skip] * (i%skip) ) / skip;
	}
	 
	for (int i=0; i<=0xffff; i++) { 
		outCurve[i] = (65535.0 * dcurve[i]);
		//if (i%1000==0) printf("satcurve[%d]= %f\n",i,outCurve[i]);
	}
	delete [] dcurve;
	delete satcurve;
}

	
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	void CurveFactory::complexCurve (double ecomp, double black, double hlcompr, double hlcomprthresh, \
									 double shcompr, double br, double contr, double defmul, double gamma_, bool igamma_, \
									 const std::vector<double>& curvePoints, unsigned int* histogram, \
									 float* hlCurve, float* shCurve, int* outCurve, \
									 unsigned int* outBeforeCCurveHistogram, int skip) {
		
		double def_mul = pow (2.0, defmul);
		
		//printf ("def_mul= %f ecomp= %f black= %f  hlcompr= %f shcompr= %f br= %f contr= %f defmul= %f  gamma= %f, skip= %d \n",def_mul,ecomp,black,hlcompr,shcompr,br,contr,defmul,gamma_,skip);
		
		// compute parameters of the gamma curve
		double start = exp(gamma_*log( -0.099 / ((1.0/gamma_-1.0)*1.099 )));
		double slope = 1.099 * pow (start, 1.0/gamma_-1) - 0.099/start;
		double mul = 1.099;
		double add = 0.099;
		
		// a: slope of the curve, black: starting point at the x axis
		double a = pow (2.0, ecomp);
		
		// curve without contrast
		double* dcurve = new double[65536];
		
		// check if contrast curve is needed
		bool needcontrast = contr>0.00001 || contr<-0.00001;
		
		// check if inverse gamma is needed at the end
		bool needigamma = igamma_ && gamma_>0;
		
		// create a curve if needed
		DiagonalCurve* tcurve = NULL;
		if (curvePoints.size()>0 && curvePoints[0]!=0)
			tcurve = new DiagonalCurve (curvePoints, CURVES_MIN_POLY_POINTS/skip);
		
		// clear array that stores histogram valid before applying the custom curve
		if (outBeforeCCurveHistogram)
			memset (outBeforeCCurveHistogram, 0, 256*sizeof(int));
		
		//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		// tone curve base. a: slope (from exp.comp.), b: black, def_mul: max. x value (can be>1), hr,sr: highlight,shadow recovery
		//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
		std::vector<double> brightcurvePoints;
		brightcurvePoints.push_back((double)DCT_NURBS);
		
		brightcurvePoints.push_back(0); //black point.  Value in [0 ; 1] range
		brightcurvePoints.push_back(0); //black point.  Value in [0 ; 1] range
		
		if(br>0) {
			brightcurvePoints.push_back(0.1); //toe point
			brightcurvePoints.push_back(0.1+br/150.0); //value at toe point
			
			brightcurvePoints.push_back(0.7); //shoulder point
			brightcurvePoints.push_back(MIN(1.0,0.7+br/300.0)); //value at shoulder point
		} else {
			brightcurvePoints.push_back(0.1-br/150.0); //toe point
			brightcurvePoints.push_back(0.1); //value at toe point
			
			brightcurvePoints.push_back(MIN(1.0,0.7-br/300.0)); //shoulder point
			brightcurvePoints.push_back(0.7); //value at shoulder point
		}
		brightcurvePoints.push_back(1); // white point
		brightcurvePoints.push_back(1); // value at white point
		
		DiagonalCurve* brightcurve = NULL;
		brightcurve = new DiagonalCurve (brightcurvePoints, CURVES_MIN_POLY_POINTS/skip); // Actually, CURVES_MIN_POLY_POINTS = 1000,
		//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
		float exp_scale = a*def_mul ;
		float scale = 65536.0;
		float comp = (ecomp + defmul)*hlcompr/100.0;
		int shoulder = (int)((scale/exp_scale)*(hlcomprthresh/200.0));
		
		for (int i=0; i<=0xffff; i++) {
			
			// change to [0,1] range
			double val = (double)i / 65535.0;
			
			// apply default multiplier (that is >1 if highlight recovery is on)
			// val *= def_mul;
			
			// apply base curve, thus, exposure compensation and black point with shadow and highlight protection
			//val = basecurve (val*def_mul, a, 0, def_mul, hlcompr/100.0, 0);
			
			//hlCurve[i] = (65535.0 * CLIPD(val));
			
			if ((hlcompr>0)&&(exp_scale>1.0))
			{
				if (i>shoulder) {
					float Y = (float)(i-shoulder)*exp_scale/(scale-shoulder);
					float R = (float)(i-shoulder)*comp/(scale-shoulder);
					hlCurve[i] = log(1+Y*comp)/R;
				} else {
					hlCurve[i]=exp_scale;
				}
			} else {
				hlCurve[i]=exp_scale;
			}
			
			//%%%%%%%%%%%%%%%%%%%%%%%%%%
			// change to [0,1] range
			val = (double)i / 65535.0;
			
			val = basecurve (val, 1, black, 1, 0, 1.5*shcompr/100.0);
			
			shCurve[i] = (65535.0 * CLIPD(val));
			
			//%%%%%%%%%%%%%%%%%%%%%%%%%%
			// change to [0,1] range
			val = (double)i / 65535.0;
			
			// gamma correction
			if (gamma_>0) 
				val = gamma (val, gamma_, start, slope, mul, add);
			
			// apply brightness curve
			//val = brightness (val, br/100.0);
			val = brightcurve->getVal (val);
			
			// store result in a temporary array
			dcurve[i] = CLIPD(val);
		}
		
		//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
		//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
		if (needcontrast) {  
			// compute mean luminance of the image with the curve applied
			int sum = 0;
			double avg = 0; 
			//double sqavg = 0;
			for (int i=0; i<=0xffff; i++) {
				avg += dcurve[(int)shCurve[CLIP((int)(hlCurve[i]*i))]] * histogram[i];
				//sqavg += dcurve[i]*dcurve[i] * histogram[i];
				sum += histogram[i];
			}
			avg /= sum;
			//sqavg /= sum;
			//double stddev = sqrt(sqavg-avg*avg);
			
			//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
			std::vector<double> contrastcurvePoints;
			contrastcurvePoints.push_back((double)DCT_NURBS);
			
			contrastcurvePoints.push_back(0); //black point.  Value in [0 ; 1] range
			contrastcurvePoints.push_back(0); //black point.  Value in [0 ; 1] range
			
			contrastcurvePoints.push_back(avg-avg*(0.6-contr/250.0)); //toe point
			contrastcurvePoints.push_back(avg-avg*(0.6+contr/250.0)); //value at toe point
			
			contrastcurvePoints.push_back(avg+(1-avg)*(0.6-contr/250.0)); //shoulder point
			contrastcurvePoints.push_back(avg+(1-avg)*(0.6+contr/250.0)); //value at shoulder point
			
			contrastcurvePoints.push_back(1); // white point
			contrastcurvePoints.push_back(1); // value at white point
			
			DiagonalCurve* contrastcurve = NULL;
			contrastcurve = new DiagonalCurve (contrastcurvePoints, CURVES_MIN_POLY_POINTS/skip); // Actually, CURVES_MIN_POLY_POINTS = 1000,
			//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
			
			// apply contrast enhancement
			for (int i=0; i<=0xffff; i++) {
				dcurve[i]  = contrastcurve->getVal (dcurve[i]);
			}
			delete contrastcurve;
		}
		//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
		for (int i=0; i<=0xffff; i++) {
			float val;
			
			// apply custom/parametric/NURBS curve, if any
			if (tcurve) {
				if (outBeforeCCurveHistogram) {
					float hval = dcurve[(int)shCurve[CLIP((int)(hlCurve[i]*i))]];
					//if (needigamma)
					//	hval = igamma2 (hval);
					int hi = (int)(255.0*CLIPD(hval));
					outBeforeCCurveHistogram[hi]+=histogram[i] ;
				}
				val = tcurve->getVal (dcurve[i]);
			} else {
				val = (dcurve[i]);
			}
			
			// if inverse gamma is needed, do it (standard sRGB inverse gamma is applied)
			if (needigamma)
				val = igamma (val, gamma_, start, slope, mul, add);
			
			outCurve[i] = (int) (65535.0 * val + 0.5);
		}
		
		
		delete [] dcurve;
		delete tcurve;
		delete brightcurve; 
		/*if (outBeforeCCurveHistogram) {
			for (int i=0; i<256; i++) printf("i= %d bchist= %d \n",i,outBeforeCCurveHistogram[i]);
		}*/
		
	}


	//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



int *CurveFactory::gammatab = 0;
int *CurveFactory::igammatab_srgb = 0;
int *CurveFactory::gammatab_srgb = 0;

void CurveFactory::init () {

  gammatab = new int[65536];
  igammatab_srgb = new int[65536];
  gammatab_srgb = new int[65536];

  for (int i=0; i<65536; i++)
    gammatab_srgb[i] = (int)(65535 * gamma2 (i/65535.0));
  for (int i=0; i<65536; i++)
    igammatab_srgb[i] = (int)(65535 * igamma2 (i/65535.0));
  for (int i=0; i<65536; i++)
    gammatab[i] = (int)(65535 * pow (i/65535.0, 0.454545));
    
/*    FILE* f = fopen ("c.txt", "wt");
    for (int i=0; i<256; i++)
        fprintf (f, "%g %g\n", i/255.0, clower (i/255.0, 2.0, 1.0));
    fclose (f);*/
}

void CurveFactory::cleanup () {

  delete [] gammatab;
  delete [] igammatab_srgb;
  delete [] gammatab_srgb;
}

}
