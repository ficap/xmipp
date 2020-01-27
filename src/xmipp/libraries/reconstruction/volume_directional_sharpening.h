/***************************************************************************
 *
 * Authors:    Jose Luis Vilas, 					  jlvilas@cnb.csic.es
 * 			   Carlos Oscar S. Sorzano            coss@cnb.csic.es (2016)
 *
 * Unidad de  Bioinformatica of Centro Nacional de Biotecnologia , CSIC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 *
 *  All comments concerning this program package may be sent to the
 *  e-mail address 'xmipp@cnb.csic.es'
 ***************************************************************************/

#ifndef _PROG_DIR_SHARPENING
#define _PROG_DIR_SHARPENING

#include <iostream>
#include <core/xmipp_program.h>
#include <core/xmipp_image.h>
#include <core/metadata.h>
#include <core/xmipp_hdf5.h>
#include <core/xmipp_fft.h>
#include <core/xmipp_fftw.h>
#include <math.h>
#include <limits>
#include <complex>
#include <data/fourier_filter.h>
#include <data/monogenic.h>
#include <data/filters.h>
#include <string>
#include "symmetrize.h"
#include "resolution_directional.h"

/**@defgroup Directional Sharpening
   @ingroup ReconsLibrary */
//@{
/** SSNR parameters. */

class ProgDirSharpening : public XmippProgram
{
public:
	 /** Filenames */
	FileName fnOut, fnVol, fnRes, fnMD, fnMask;

	/** sampling rate, minimum resolution, and maximum resolution */
	double sampling, maxRes, minRes, lambda, K, maxFreq, minFreq, desv_Vorig, R, significance, res_step;
	int Niter, Nthread;
	bool test;

public:

    void defineParams();
    void readParams();
    void produceSideInfo();
    void icosahedronVertex(Matrix2D<double> &angles);
    void icosahedronFaces(Matrix2D<int> &faces, Matrix2D<double> &vertex);

    double averageInMultidimArray(MultidimArray<double> &amplitude, MultidimArray<int> &mask);

    void getFaceVector(int face_number, Matrix2D<int> &faces,
    		Matrix2D<double> &vertex, double &x1, double &y1, double &z1);

    void directionalNoiseEstimation(double &x_dir, double &y_dir, double &z_dir,
    		MultidimArray<double> &amplitudeMS, MultidimArray<int> &mask, double &cone_angle,
    		int &particleRadius, double &NS, double &NN, double &sumS, double &sumS2, double &sumN2, double &sumN,
    		double &thresholdNoise);

//    void bandPassDirectionalFilterFunction(int face_number, Matrix2D<int> &faces,
//    		Matrix2D<double> &vertex, MultidimArray< std::complex<double> > &myfftV,
//    		MultidimArray<double> &Vorig, MultidimArray<double> &iu, FourierTransformer &transformer_inv,
//            double w, double wL, MultidimArray<double> &filteredVol, int count);
    void bandPassDirectionalFilterFunction(int face_number, MultidimArray< std::complex<double> > &myfftV,
    		MultidimArray<double> &Vorig, MultidimArray<double> &iu, FourierTransformer &transformer_inv,
            double w, double wL, MultidimArray<double> &filteredVol, int count);

    void directionalResolutionStep(int face_number, Matrix2D<int> &faces, Matrix2D<double> &vertex,
    		MultidimArray< std::complex<double> > &conefilter, MultidimArray<int> &mask, MultidimArray<double> &localResolutionMap,
    		double &cone_angle);

    void localDirectionalfiltering(Matrix2D<int> &faces,
    		Matrix2D<double> &vertex, MultidimArray< std::complex<double> > &myfftV,
            MultidimArray<double> &localfilteredVol, MultidimArray<double> &Vorig,
            double &minRes, double &maxRes, double &step);

    void defineIcosahedronCone(int face_number, Matrix2D<int> &faces, Matrix2D<double> &vertex,
    		MultidimArray< std::complex<double> > &myfftV, MultidimArray<double> &conefilter,
			double coneAngle);

    void localdeblurStep(MultidimArray<double> &vol, MultidimArray<int> &mask,
    		Matrix2D<double> &vertex, Matrix2D<int> &faces);

    void run();
public:
    //, VsoftMask;
    Image<int> mask;
    MultidimArray<double> iu, VRiesz, sharpenedMap; // Inverse of the frequency
	MultidimArray< std::complex<double> > fftV, fftVfilter; // Fourier transform of the input volume
	FourierTransformer transformer, transformer_inv;
    MultidimArray< std::complex<double> > fftVRiesz, fftVRiesz_aux;
   	Matrix2D<double> angles, resolutionMatrix, maskMatrix, trigProducts;
	Matrix1D<double> freq_fourier_x, freq_fourier_y, freq_fourier_z;
	int N_smoothing, Rparticle;
	long NVoxelsOriginalMask;
};
//@}
#endif
