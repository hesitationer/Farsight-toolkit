/*=========================================================================
Copyright 2009 Rensselaer Polytechnic Institute
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 
=========================================================================*/
/**************************************************************************  
 *  Volume dataset processing:
 *  Author: Xiaosong Yuan, RPI
 *  Modified on Sep. 29, 2005  
 *  Adapted Jan. 2010 by Isaac Abbott 
 *        
 *************************************************************************/
#ifndef __mdlVolumeProcess_h
#define __mdlVolumeProcess_h

#include "mdlTypes.h"

#include <string>
#include <vector>
#include <iostream>
#include <math.h>
#include <algorithm>
#include <utility>

#include "GCBinarization/cell_binarization.h"

//#include "itkImageFileWriter.h"
#include "itkOtsuThresholdImageFilter.h" 
#include "itkRescaleIntensityImageFilter.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkSigmoidImageFilter.h"

namespace mdl
{

class VolumeProcess
{
public:
	VolumeProcess();
	//Setup:
	void SetDebug(bool inp = true){ debug = inp; };
	void SetInput(ImageType::Pointer inImage);
	//Methods:
	bool RescaleIntensities(int min, int max);
	bool NonlinearMappingSigmoidFilter(double alpha,  double beta,  double min, double max);
	bool RunCAD(unsigned int numberOfIterations = 5 ,double timeStep = 0.0425,double conductance =3); //Curvature Anisotropic Diffusion
	bool RunOtsuDenoising();
	bool DialateImage(int iterations);
	bool MaskSmallConnComp(int minObjSize);
	bool MaskUsingGraphCuts();
	bool RunAnisotropicDiffusion(int timesDiffuse=1, bool iso = false);	//Run anisotropic diffusion written by Xiaosong
	bool RunManualThreshold(int threshold);
	bool RunDistanceTransform(void);
	bool distTransform(unsigned char *f, int L, int M, int N); 
	//Get Result:
	ImageType::Pointer GetOutput();

private:
	static const unsigned char m_NumberOfHistogramBins = 128;

	//Parameters
	bool debug;				//If debug is true, process in steps and print stuff
	
	//Images
	ImageType::Pointer m_inputImage;
	ImageType::Pointer m_outputImage;

	//Functions:
	double getItkOtsuThreshold(ImageType::Pointer img);
	double getXiaoLiangOtsuThreshold(ImageType::Pointer img);
	int MIN(int x,int y) {return (((x) < (y))?(x):(y));}
	int MAX(int x,int y) {return (((x) > (y))?(x):(y));}
};

}  // end namespace mdl

#endif
