/* 
 * Copyright 2009 Rensselaer Polytechnic Institute
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

//seedsDetection_2D.cxx

#if defined(_MSC_VER)
#pragma warning ( disable : 4786 )
#endif

#ifdef __BORLANDC__
#define ITK_LEAN_AND_MEAN
#endif


#include <stdio.h>
#include <iostream>
#include <fstream>
#include<stdlib.h>
#include <iostream>
#include <algorithm>
#include <math.h>

#include "itkImage.h"
#include "itklaplacianrecursivegaussianimagefilternew.h"
#include "itkImageRegionIteratorWithIndex.h"
#include <itkDanielssonDistanceMapImageFilter.h> 
#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkCastImageFilter.h"

//added by Yousef on 8/26/2009
#include "itkExtractImageFilter.h"


////
#include "itkImageFileWriter.h"

using namespace std;


typedef    unsigned short     MyInputPixelType;
typedef itk::Image< MyInputPixelType,  3 >   MyInputImageType;
typedef itk::Image< MyInputPixelType,  2 >   MyInputImageType2D;

int detect_seeds(itk::SmartPointer<MyInputImageType>, int , int , int, const double, float*, int);
//int multiScaleLoG(itk::SmartPointer<MyInputImageType> im, int r, int c, int z,const double sigma_min, double sigma_max, float* IMG, int sampl_ratio, int unsigned short* dImg, int* minIMout, int UseDistMap);
int multiScaleLoG(itk::SmartPointer<MyInputImageType> im, int r, int c, int z, int rmin, int rmax, int cmin, int cmax, int zmin, int zmax,const double sigma_min, double sigma_max, float* IMG, int sampl_ratio, int unsigned short* dImg, int* minIMout, int UseDistMap);
float get_maximum_3D(float* A, int r1, int r2, int c1, int c2, int z1, int z2, int R, int C);
unsigned short get_maximum_3D(unsigned short* A, int r1, int r2, int c1, int c2, int z1, int z2,int R, int C);
void Detect_Local_MaximaPoints_3D(float* im_vals, int r, int c, int z, double scale_xy, double scale_z, unsigned short* out1, unsigned short* bImg);
int distMap(itk::SmartPointer<MyInputImageType> im, int r, int c, int z, unsigned short* IMG);
int distMap_SliceBySlice(itk::SmartPointer<MyInputImageType> im, int r, int c, int z, unsigned short* IMG);
MyInputImageType2D::Pointer extract2DImageSlice(itk::SmartPointer<MyInputImageType> im, int plane, int slice);
MyInputImageType::Pointer extract3DImageRegion(itk::SmartPointer<MyInputImageType> im, int sz_x, int sz_y, int sz_z, int start_x, int start_y, int start_z);
void estimateMinMaxScales(itk::SmartPointer<MyInputImageType> im, unsigned short* distIm, double* minScale, double* maxScale, int r, int c, int z);
int computeMedian(std::vector< std::vector<unsigned short> > scales, int cntr);
void estimateMinMaxScalesV2(itk::SmartPointer<MyInputImageType> im, unsigned short* distIm, double* minScale, double* maxScale, int r, int c, int z);
int computeWeightedMedian(std::vector< std::vector<float> > scales, int cntr);

int Seeds_Detection_3D( float* IM, float** IM_out, unsigned short** IM_bin, int r, int c, int z, double *sigma_min_in, double *sigma_max_in, double *scale_xy_in, double *scale_z_in, int sampl_ratio, unsigned short* bImg, int UseDistMap, int* minIMout, bool paramEstimation)
{	
	//get this inputs
	double sigma_min = sigma_min_in[0];
	double sigma_max = sigma_max_in[0];
	double scale_xy = scale_xy_in[0];
	double scale_z = scale_z_in[0];

	//Create an itk image
	MyInputImageType::Pointer im;
	im = MyInputImageType::New();
	MyInputImageType::PointType origin;
    origin[0] = 0.; 
    origin[1] = 0.;    
	origin[2] = 0.;    
    im->SetOrigin( origin );

    MyInputImageType::IndexType start;
    start[0] =   0;  // first index on X
    start[1] =   0;  // first index on Y    
	start[2] =   0;  // first index on Z    
    MyInputImageType::SizeType  size;
    size[0]  = c;  // size along X
    size[1]  = r;  // size along Y
	size[2]  = z;  // size along Z
  
    MyInputImageType::RegionType region;
    region.SetSize( size );
    region.SetIndex( start );
    
    double spacing[3];
	spacing[0] = 1; //spacing along x
	spacing[1] = 1; //spacing along y
	spacing[2] = sampl_ratio; //spacing along z

    im->SetRegions( region );
	im->SetSpacing(spacing);
    im->Allocate();
    im->FillBuffer(0);
	im->Update();	
	//copy the input image into the ITK image
	typedef itk::ImageRegionIteratorWithIndex< MyInputImageType > IteratorType;
	IteratorType iterator1(im,im->GetRequestedRegion());
	
	unsigned short* dImg = NULL;	
	int max_dist = 1;
	if(UseDistMap == 1)
	{
		for(int i=0; i<r*c*z; i++)
		{				
			if(bImg[i]>0)
				iterator1.Set(0.0);//IM[i]);
			else
				iterator1.Set(255.0);
			++iterator1;
		
		}
		//By Yousef on 09/08/2009
		//Just for testing purposes: Write out the distance map into an image file
		typedef itk::ImageFileWriter< MyInputImageType > WriterType;
		WriterType::Pointer writer = WriterType::New();
		writer->SetFileName("bin_test.tif");
		writer->SetInput( im );
		//writer->Update();
		//////////////////////////////////////////////////////////////////////////
		std::cout<<"Computing distance transform...";
		dImg = (unsigned short *) malloc(r*c*z*sizeof(unsigned short));
		if(!dImg)
		{
			std::cerr<<"Failed to allocate memory for the distance image"<<std::endl;
			return 0;
		}
		//max_dist = distMap(im, r, c, z,dImg);
		max_dist = distMap_SliceBySlice(im, r, c, z,dImg);
		std::cout<<"done"<<std::endl;
		iterator1.GoToBegin();		
	}
	
	float multp = 1.0;
	for(int i=0; i<r*c*z; i++)
	{
		if(UseDistMap == 1)
			multp = 1+((float) dImg[i]/(2*max_dist));
		if(bImg[i] > 0)
			iterator1.Set((unsigned short)IM[i]/multp);			
		else
			iterator1.Set(255);
		
		++iterator1;
	}

	//By Yousef (8/29/2009)
	//Estimate the segmentation parameters
	if(UseDistMap == 1 && paramEstimation)
	{
		std::cout<<"Estimating parameters..."<<std::endl;
		estimateMinMaxScalesV2(im, dImg, &sigma_min, &sigma_max, r, c, z);
		scale_xy = sigma_min;
		if(scale_xy<3)
			scale_xy = 3; //just avoid very small search boxes
		scale_z = ceil(scale_xy / sampl_ratio);
		std::cout<<"    Minimum scale = "<<sigma_min<<std::endl;
		std::cout<<"    Maximum scale = "<<sigma_max<<std::endl;
		std::cout<<"    Clustering Resolution = "<<scale_xy<<std::endl;
		//write out the parameters
		sigma_min_in[0] = sigma_min;
		sigma_max_in[0] = sigma_max;
		scale_xy_in[0] =  scale_xy;
		scale_z_in[0] = scale_z;
	}

	//By Yousef (8/28/2009)
	//In some situations the image is very larg and we cannot allocate memory for the LoG filter (20xthe size of the image in bytes)
	//In such cases, we can divide the image into small tiles, process them independently, and them combine the results
	int block_divisor;	
	//see if we have enought memory for the LoG step
	//approximately, we need (20~21)ximage size in bytes
	//try to allocate memory for an unsigned char* of the 23ximage size
	unsigned char *tmpp = (unsigned char*) malloc(23*r*c*z*sizeof(unsigned char));
	if(tmpp)
		block_divisor = 1;
	else
		block_divisor = 2;
	free(tmpp); //delete it
	tmpp = NULL;
	
	
	int min_x, min_y, max_x, max_y;
	for(int i=0; i<r; i+=r/block_divisor)
	{
		for(int j=0; j<c; j+=c/block_divisor)
		{						
			min_x = j; 
			max_x = 40+(int)j+c/block_divisor; //40 is the size of the overlapping between the two tiles along x
			min_y = i;
			max_y = 40+(int)i+r/block_divisor; //40 is the size of the overlapping between the two tiles along y
			if(max_x >= c)
				max_x = c-1;
			if(max_y >= r)
				max_y = r-1;

			//Create an itk image to hold the sub image (tile) being processing
			MyInputImageType::Pointer im_Small = extract3DImageRegion(im, max_x-min_x+1, max_y-min_y+1, z, min_x, min_y, 0);
						
			//By Yousef (8/27/2009): multi-scale LoG is done in one function now
			multiScaleLoG(im_Small, r, c, z, min_y, max_y, min_x, max_x, 0, z-1, sigma_min, sigma_max, IM, sampl_ratio, dImg, minIMout, UseDistMap);
			//
		}
	}
	
		
	free(dImg);
   
	IM_out[0] = new float[r*c*z];
	if(!IM_out[0])
	{
		std::cerr<<"could not allocate memory for the LoG response image"<<std::endl;
		return 0;
	}
	for(int i=0; i<r*c*z; i++)
	{
		IM_out[0][i] = IM[i];
	}
	//Detect the seed points (which are also the local maxima points)	
	std::cout<<"Detecting Seeds"<<std::endl;
	IM_bin[0] = new unsigned short[r*c*z];
  //std::cout << "about to call Detect_Local_MaximaPoints_3D" << std::endl;
	Detect_Local_MaximaPoints_3D(IM_out[0], r, c, z, scale_xy, scale_z, IM_bin[0], bImg);	
  std::cout << "done detecting seeds" << std::endl;
	
	return 1;
}


int detect_seeds(itk::SmartPointer<MyInputImageType> im, int r, int c, int z,const double sigma, float* IMG, int sampl_ratio)
{
  
  //  Types should be selected on the desired input and output pixel types.
  typedef    float     OutputPixelType;


  //  The input and output image types are instantiated using the pixel types.
  typedef itk::Image< OutputPixelType, 3 >   OutputImageType;


  //  The filter type is now instantiated using both the input image and the
  //  output image types.
  typedef itk::LaplacianRecursiveGaussianImageFilterNew<MyInputImageType, OutputImageType >  FilterType;
  FilterType::Pointer laplacian = FilterType::New();
  

  //  The option for normalizing across scale space can also be selected in this filter.
  laplacian->SetNormalizeAcrossScale( true );

  //  The input image can be obtained from the output of another
  //  filter. Here the image comming from the calling function is used as the source
  laplacian->SetInput( im);
  

  
  //  It is now time to select the $\sigma$ of the Gaussian used to smooth the
  //  data.  Note that $\sigma$ must be passed to both filters and that sigma
  //  is considered to be in millimeters. That is, at the moment of applying
  //  the smoothing process, the filter will take into account the spacing
  //  values defined in the image.
  //
  laplacian->SetSigma(sigma);
 
  //  Finally the pipeline is executed by invoking the \code{Update()} method.
  //
 try
    {
    laplacian->Update();
    }
  catch( itk::ExceptionObject & err ) 
    { 
    std::cout << "ExceptionObject caught !" << std::endl; 
    std::cout << err << std::endl; 
    return EXIT_FAILURE;
    } 
 
  //   Copy the resulting image into the input array
  long int i = 0;
  typedef itk::ImageRegionIteratorWithIndex< OutputImageType > IteratorType;
  IteratorType iterate(laplacian->GetOutput(),laplacian->GetOutput()->GetRequestedRegion());
  while ( i<r*c*z)
  {
    IMG[i] = /*sigma*sigma*/iterate.Get()/sqrt(sigma);
    ++i;
	++iterate;
  }

  return EXIT_SUCCESS;
}

int multiScaleLoG(itk::SmartPointer<MyInputImageType> im, int r, int c, int z, int rmin, int rmax, int cmin, int cmax, int zmin, int zmax,const double sigma_min, double sigma_max, float* IMG, int sampl_ratio, int unsigned short* dImg, int* minIMout, int UseDistMap)
{
  
  //  Types should be selected on the desired input and output pixel types.
  typedef    float     OutputPixelType;
  //  The input and output image types are instantiated using the pixel types.
  typedef itk::Image< OutputPixelType, 3 >   OutputImageType;


  for(int sigma=sigma_min; sigma<=sigma_max; sigma++)
  {
    std::cout<<"Processing scale "<<sigma<<"...";
	//  The filter type is now instantiated using both the input image and the
	//  output image types.
	typedef itk::LaplacianRecursiveGaussianImageFilterNew<MyInputImageType, OutputImageType >  FilterType;
	FilterType::Pointer laplacian = FilterType::New();
	//  The option for normalizing across scale space can also be selected in this filter.
	laplacian->SetNormalizeAcrossScale( true );

	//  The input image can be obtained from the output of another
	//  filter. Here the image comming from the calling function is used as the source
	laplacian->SetInput( im);
  
	//  It is now time to select the $\sigma$ of the Gaussian used to smooth the
	//  data.  Note that $\sigma$ must be passed to both filters and that sigma
	//  is considered to be in millimeters. That is, at the moment of applying
	//  the smoothing process, the filter will take into account the spacing
	//  values defined in the image.
	//
	laplacian->SetSigma(sigma);
 
	//  Finally the pipeline is executed by invoking the \code{Update()} method.
	//
	try
	{
		laplacian->Update();
    }
	catch( itk::ExceptionObject & err ) 
    { 
		std::cout << "ExceptionObject caught !" << std::endl; 
		std::cout << err << std::endl; 
		return EXIT_FAILURE;
    } 
 
	//   Copy the resulting image into the input array
	//long int i = 0;
	typedef itk::ImageRegionIteratorWithIndex< OutputImageType > IteratorType;
	IteratorType iterate(laplacian->GetOutput(),laplacian->GetOutput()->GetRequestedRegion());
	long int II;
	for(int k1=zmin; k1<=zmax; k1++)
	{
		for(int i1=rmin; i1<=rmax; i1++)	
		{			
			for(int j1=cmin; j1<=cmax; j1++)
			{
				II = (k1*r*c)+(i1*c)+j1;
				float lgrsp = iterate.Get();
				//lgrsp /= sqrt((double)sigma);
				if(sigma==sigma_min)
				{
					IMG[II] = lgrsp;			
				}
				else
				{					
					if(UseDistMap == 1)
					{
						if(sigma<=dImg[II]/100)
						{
							IMG[II] = (IMG[II]>=lgrsp)? IMG[II] : lgrsp;				
							if(IMG[II]<minIMout[0])
								minIMout[0] = IMG[II];
						}
					}
					else
					{
						IMG[II] = (IMG[II]>=lgrsp)? IMG[II] : lgrsp;				
						if(IMG[II]<minIMout[0])
							minIMout[0] = IMG[II];
					}
				}				
				++iterate;		
			}
		}
	}	

	std::cout<<"done"<<std::endl;
  }
	
  return EXIT_SUCCESS;
}

float get_maximum_3D(float* A, int r1, int r2, int c1, int c2, int z1, int z2,int R, int C)
{
	
   	float mx = A[(z1*R*C)+(r1*C)+c1];
    for(int i=r1; i<=r2; i++)
    {
        for(int j=c1; j<=c2; j++)
        {
			for(int k=z1; k<=z2; k++)
			{				
				if(A[(k*R*C)+(i*C)+j]>mx)
					mx = A[(k*R*C)+(i*C)+j];
			}
        }
    }
    return mx;
}

unsigned short get_maximum_3D(unsigned short* A, int r1, int r2, int c1, int c2, int z1, int z2,int R, int C)
{
	
   	unsigned short mx = A[(z1*R*C)+(r1*C)+c1];
    for(int i=r1; i<=r2; i++)
    {
        for(int j=c1; j<=c2; j++)
        {
			for(int k=z1; k<=z2; k++)
			{				
				if(A[(k*R*C)+(i*C)+j]>mx)
					mx = A[(k*R*C)+(i*C)+j];
			}
        }
    }
    return mx;
}


void Detect_Local_MaximaPoints_3D(float* im_vals, int r, int c, int z, double scale_xy, double scale_z, unsigned short* out1, unsigned short* bImg)
{  
    int min_r, min_c, max_r, max_c, min_z, max_z;    
       
    //start by getting local maxima points
    //if a point is a local maximam give it a local maximum ID
        
	//int IND = 0;
	int II = 0;
  int itr = 0;
  //std::cout << "In Detect_Local_MaximaPoints_3D, about to plunge in the loop" << std::endl;
    for(int i=0; i<r; i++)
    {
        for(int j=0; j<c; j++)
        {				
			for(int k=0; k<z; k++)
			{									
				min_r = (int) max(0.0,i-scale_xy);
				min_c = (int) max(0.0,j-scale_xy);
				min_z = (int) max(0.0,k-scale_z);
				max_r = (int)min((double)r-1,i+scale_xy);
				max_c = (int)min((double)c-1,j+scale_xy);                         
				max_z = (int)min((double)z-1,k+scale_z);                         
        if(itr % 1000 == 0)
          {
          //std::cout << ".";
          std::cout.flush();
          }
				float mx = get_maximum_3D(im_vals, min_r, max_r, min_c, max_c, min_z, max_z,r,c);
				II = (k*r*c)+(i*c)+j;
				if(im_vals[II] == mx)    
				{
					//IND = IND+1;
					//if(bImg[II] > 0)
						out1[II]=255;                 
					//else
					//	out1[II] = -1;
				}
				else
					out1[(k*r*c)+(i*c)+j]=0;
      itr++;
			}			
        }
    }  
  //std::cout << std::endl << "made it out of the loop" << std::endl;
}

//added by Yousef on 8/29/2009
//Estimate the min and max scales based on the local maxima points of the distance map
void estimateMinMaxScales(itk::SmartPointer<MyInputImageType> im, unsigned short* distIm, double* minScale, double* maxScale, int r, int c, int z)
{
	int min_r, min_c, max_r, max_c, min_z, max_z;    
	int II = 0;
	minScale[0] = 1000.0;
	maxScale[0] = 0.0;
	int cent_slice = (int) z/2;
	std::vector< std::vector<unsigned short> > scales;
//	double mean = 0.0;
//	double stdv = 0.0;
	int cnt = 0;
	ofstream p;
//	int max_dist = 0;
	//p.open("checkme.txt");
	for(int i=1; i<r-1; i++)
    {
        for(int j=1; j<c-1; j++)
        {				
			//for(int k=1; k<z-1; k+=2)
			for(int k=cent_slice; k<=cent_slice; k++)
			{									
				min_r = (int) max(0.0,(double)i-2);
				min_c = (int) max(0.0,(double)j-2);
				min_z = (int) max(0.0,(double)k);
				max_r = (int)min((double)r-1,(double)i+2);
				max_c = (int)min((double)c-1,(double)j+2);                         
				max_z = (int)min((double)z-1,(double)k);                         
				unsigned short mx = get_maximum_3D(distIm, min_r, max_r, min_c, max_c, min_z, max_z,r,c);
				
				if(mx <= 100)
					continue; //background or edge point
				II = (k*r*c)+(i*c)+j;
				if(distIm[II] == mx)    
				{		
					//since we have scaled by 100 earlier, scale back to the original value
					//also, we want to dived by squre root of 2 or approximately 1.4
					mx = mx/140;							
					//add the selected scale to the list of scales
					std::vector <unsigned short> lst;
					lst.push_back(mx);
					lst.push_back(i);
					lst.push_back(j);
					lst.push_back(k);
					p<<j<<" "<<i<<" "<<k<<" "<<mx<<std::endl;
					scales.push_back(lst);
					//mean +=mx;
					cnt++;										
				}				
			}			
        }
    } 
	//p.close();
	
	//get the median of the scales(distances)
	int medianS = computeMedian(scales, cnt);
	//ofstream p2;
	//p2.open("checkme2.txt");
	//p2<<"med = "<<medianS<<std::endl;

	//then compute the Median absolute deviation
	std::vector< std::vector<unsigned short> > madList;
	for(int i=0; i<cnt; i++)
	{
		std::vector<unsigned short> tmp;
		tmp.push_back(abs(scales[i][0]-medianS));
		madList.push_back(tmp);
	}
	int MAD = computeMedian(madList, cnt);
	minScale[0] = medianS-MAD;
	maxScale[0] = medianS+MAD;		
		
	//p2<<"mad = "<<MAD<<std::endl;
	//p2<<"med-mad = "<<minScale[0]<<std::endl;
	//p2<<"med+mad = "<<maxScale[0]<<std::endl;

	ofstream p3;
	p3.open("checkme3.txt");	
	//For each local maximum point,try to find the best LoG scale
	//To do that, suppose the distance at a given local maximum point is d, 
	//then compute the its LoG responses at scales from d/2 to d
	//Then, select the scale the gave us the maximum LoG response
	//Create an itk image to hold the sub image (tile) being processing
	int mnScl = 10000;
	int mxScl = 0;
	int cnt2 = 0;
	for(int ind=0; ind<cnt; ind++)
	{
		int mx = scales[ind][0];
		int i = scales[ind][1];
		int j = scales[ind][2];
		int k = scales[ind][3];
		//if(mx<minScale[0] || mx>maxScale[0])
		//	continue;

		int smin = (int) std::ceil(mx/2.0);
		if(smin == mx)
			continue;
		if(smin == 1)
			smin++;
		cnt2++;
		min_r = (int) max(0.0,(double)i-mx);
		min_c = (int) max(0.0,(double)j-mx);
		min_z = (int) max(0.0,(double)k-mx);
		max_r = (int)min((double)r-1,(double)i+mx);
		max_c = (int)min((double)c-1,(double)j+mx);                         
		max_z = (int)min((double)z-1,(double)k+mx);                         
					
		int sub_r = i-min_r;
		int sub_c = j-min_c;
		int sub_z = k-min_z;
		int sz_r = (max_r-min_r+1);
		int sz_c = (max_c-min_c+1);
		int sz_z = (max_z-min_z+1);
		int ind_i = sub_z*sz_r*sz_c+sub_r*sz_c+sub_c;
																	
		MyInputImageType::Pointer im_Small = extract3DImageRegion(im, sz_c, sz_r, sz_z, min_c, min_r, min_z);															
		float* IMG = new float[sz_c*sz_r*sz_z];
		float max_resp = -100000.0;	
		int best_scale = 0.0;
		double sigma;			
		for(int kk=smin; kk<=mx; kk++)
		{						
			sigma = kk;
			detect_seeds(im_Small, sz_r, sz_c, sz_z, sigma, IMG, 0);
			//Method 1:
			//Get the scale at which the LoG response at our point of interest is maximum
			if(IMG[ind_i]>=max_resp)
			{
				max_resp = IMG[ind_i];								
				best_scale = kk;				
			}
			//Method 2:
			//We need the scale at which the maximum response in the small image region surrouding our point of interest 
			//is maximum over scales
			/*float mx2 = get_maximum_3D(IMG, 0, sz_r-1, 0, sz_c-1, 0, sz_z-1,sz_r,sz_c);
			if(mx2>=max_resp)
			{
				max_resp = mx2;								
				best_scale = kk;				
			}*/
		}
		p3<<j<<" "<<i<<" "<<k<<" "<<mx<<" "<<best_scale<<" "<<max_resp<<std::endl;
		mx = best_scale;
		
		if(mx<mnScl)
			mnScl = mx;
		if(mx>mxScl)
			mxScl = mx;

		delete [] IMG;
	}
	//I assume at least 4 scales must be used (will be relaxed later)
	if(mxScl<mnScl+3)
		mxScl = mnScl+3;
	minScale[0] = mnScl;
	maxScale[0] = mxScl;	
	//p2<<"min_scale="<<mnScl<<std::endl;
	//p2<<"max_scale="<<mxScl<<std::endl;
	//p2.close();
	p3.close();
}


//added by Yousef on 9/17/2009
//Estimate the min and max scales based on the local maxima points of the distance map
void estimateMinMaxScalesV2(itk::SmartPointer<MyInputImageType> im, unsigned short* distIm, double* minScale, double* maxScale, int r, int c, int z)
{
	int min_r, min_c, max_r, max_c, min_z, max_z;    
	int II = 0;
	minScale[0] = 1000.0;
	maxScale[0] = 0.0;
	int cent_slice = (int) z/2;
	std::vector< std::vector<unsigned short> > scales;
	//double mean = 0.0;
	//double stdv = 0.0;
	int cnt = 0;
	ofstream p;
	//int max_dist = 0;
	//p.open("checkme.txt");
	for(int i=1; i<r-1; i++)
    {
        for(int j=1; j<c-1; j++)
        {				
			//for(int k=1; k<z-1; k+=2)
			for(int k=cent_slice; k<=cent_slice; k++)
			{									
				min_r = (int) max(0.0,(double)i-2);
				min_c = (int) max(0.0,(double)j-2);
				min_z = (int) max(0.0,(double)k);
				max_r = (int)min((double)r-1,(double)i+2);
				max_c = (int)min((double)c-1,(double)j+2);                         
				max_z = (int)min((double)z-1,(double)k);                         
				unsigned short mx = get_maximum_3D(distIm, min_r, max_r, min_c, max_c, min_z, max_z,r,c);
				
				if(mx <= 100)
					continue; //background or edge point
				II = (k*r*c)+(i*c)+j;
				if(distIm[II] == mx)    
				{		
					//since we have scaled by 100 earlier, scale back to the original value
					//also, we want to dived by squre root of 2 or approximately 1.4
					mx = mx/140;							
					//add the selected scale to the list of scales
					std::vector <unsigned short> lst;
					lst.push_back(mx);
					lst.push_back(i);
					lst.push_back(j);
					lst.push_back(k);
					p<<j<<" "<<i<<" "<<k<<" "<<mx<<std::endl;
					scales.push_back(lst);
					//mean +=mx;
					cnt++;										
				}				
			}			
        }
    } 
	//p.close();
	
	//get the median of the scales(distances)
	int medianS = computeMedian(scales, cnt);
	//ofstream p2;
	//p2.open("checkme2.txt");
	//p2<<"med = "<<medianS<<std::endl;				

	//ofstream p3;
	//p3.open("checkme3.txt");	
	//For each local maximum point,try to find the best LoG scale
	//To do that, suppose the distance at a given local maximum point is d, 
	//then compute the its LoG responses at scales from d/2 to d
	//Then, select the scale the gave us the maximum LoG response
	int mnScl = 10000;
	int mxScl = 0;
	int cnt2 = 0;
	std::vector<std::vector<float> > smallScales;
	std::vector<std::vector<float> > largeScales;
	int numSmall = 0;
	int numLarge = 0;

	for(int ind=0; ind<cnt; ind++)
	{
		int mx = scales[ind][0];
		int i = scales[ind][1];
		int j = scales[ind][2];
		int k = scales[ind][3];		

		int smin = (int) std::ceil(mx/2.0);
		if(smin == mx)
			continue;
		if(smin == 1)
			smin++;
		cnt2++;
		min_r = (int) max(0.0,(double)i-mx);
		min_c = (int) max(0.0,(double)j-mx);
		min_z = (int) max(0.0,(double)k-mx);
		max_r = (int)min((double)r-1,(double)i+mx);
		max_c = (int)min((double)c-1,(double)j+mx);                         
		max_z = (int)min((double)z-1,(double)k+mx);                         
					
		int sub_r = i-min_r;
		int sub_c = j-min_c;
		int sub_z = k-min_z;
		int sz_r = (max_r-min_r+1);
		int sz_c = (max_c-min_c+1);
		int sz_z = (max_z-min_z+1);
		int ind_i = sub_z*sz_r*sz_c+sub_r*sz_c+sub_c;
																	
		MyInputImageType::Pointer im_Small = extract3DImageRegion(im, sz_c, sz_r, sz_z, min_c, min_r, min_z);															
		float* IMG = new float[sz_c*sz_r*sz_z];
		float max_resp = -100000.0;	
		int best_scale = 0.0;
		double sigma;	
	
		for(int kk=smin; kk<=mx; kk++)
		{						
			sigma = kk;
			detect_seeds(im_Small, sz_r, sz_c, sz_z, sigma, IMG, 0);
			//Method 1:
			//Get the scale at which the LoG response at our point of interest is maximum
			if(IMG[ind_i]>=max_resp)
			{
				max_resp = IMG[ind_i];								
				best_scale = kk;				
			}
			//Method 2:
			//We need the scale at which the maximum response in the small image region surrouding our point of interest 
			//is maximum over scales
			/*float mx2 = get_maximum_3D(IMG, 0, sz_r-1, 0, sz_c-1, 0, sz_z-1,sz_r,sz_c);
			if(mx2>=max_resp)
			{
				max_resp = mx2;								
				best_scale = kk;				
			}*/
		}
		std::vector<float> pp;
		pp.push_back(best_scale);
		pp.push_back(max_resp);

		if(mx<=medianS)
		{	
			numSmall++;		
			smallScales.push_back(pp);
		}
		else
		{
			numLarge++;		
			largeScales.push_back(pp);
		}

		//p3<<j<<" "<<i<<" "<<k<<" "<<mx<<" "<<best_scale<<" "<<max_resp<<std::endl;
		/*mx = best_scale;	
		if(mx<mnScl)
			mnScl = mx;
		if(mx>mxScl)
			mxScl = mx;*/

		delete [] IMG;
	}
	//p3.close();
	//set the min and max scales to the LoG-weighted medians of the small and large scale sets
	mnScl =  computeWeightedMedian(smallScales, numSmall);
	mxScl =  computeWeightedMedian(largeScales, numLarge);

	scales.clear();
	smallScales.clear();
	largeScales.clear();

	//I assume at least 4 scales must be used (will be relaxed later)
	if(mxScl<mnScl+3)
		mxScl = mnScl+3;
	minScale[0] = mnScl;
	maxScale[0] = mxScl;	
	//p2<<"min_scale="<<mnScl<<std::endl;
	//p2<<"max_scale="<<mxScl<<std::endl;
	//p2.close();	
}

int distMap(itk::SmartPointer<MyInputImageType> im, int r, int c, int z, unsigned short* IMG)
{
  
  //  Types should be selected on the desired input and output pixel types.  
  typedef unsigned short             InputPixelType2;
  typedef float          OutputPixelType2;

  //  The input and output image types are instantiated using the pixel types.
  typedef itk::Image< InputPixelType2,  3 >   InputImageType2;
  typedef itk::Image< OutputPixelType2, 3 >   OutputImageType2;


  //  The filter type is now instantiated using both the input image and the
  //  output image types.
  //typedef itk::ApproximateSignedDistanceMapImageFilter<InputImageType, OutputImageType > DTFilter ;    
  //typedef itk::DanielssonDistanceMapImageFilter<InputImageType, OutputImageType > DTFilter ;  
  typedef itk::SignedMaurerDistanceMapImageFilter<InputImageType2, OutputImageType2>  DTFilter;
  DTFilter::Pointer dt_obj= DTFilter::New() ;
  //dt_obj->UseImageSpacingOn();

  typedef itk::CastImageFilter< MyInputImageType, InputImageType2> myCasterType;
  myCasterType::Pointer potCaster = myCasterType::New();
  potCaster->SetInput( im );
  potCaster->Update();

  dt_obj->SetInput(potCaster->GetOutput()) ;
  dt_obj->SetSquaredDistance( false );
  dt_obj->SetUseImageSpacing( true );
  dt_obj->SetInsideIsPositive( false );

  //dt_obj->SetInsideValue(0.0);
  //dt_obj->SetOutsideValue(255.0);
  try{
	 dt_obj->Update() ;
  }
  catch( itk::ExceptionObject & err ){
	std::cerr << "Error calculating distance transform: " << err << endl ;
    return -1;
  }
 
  //   Copy the resulting image into the input array
  long int i = 0;
  typedef itk::ImageRegionIteratorWithIndex< OutputImageType2 > IteratorType;
  IteratorType iterate(dt_obj->GetOutput(),dt_obj->GetOutput()->GetRequestedRegion());
        
  int max_dist = 0;
  while ( i<r*c*z)
  {	  
	  double ds = iterate.Get();
	  if(ds<=0)
		  IMG[i] = 0;
	  else
		IMG[i] = (unsigned short) ds;

	  if(IMG[i]>max_dist)
		  max_dist = IMG[i];

      ++i;
 	  ++iterate;
  }	
 
  return max_dist;
}


int distMap_SliceBySlice(itk::SmartPointer<MyInputImageType> im, int r, int c, int z, unsigned short* IMG)
{
  
  //  Types should be selected on the desired input and output pixel types.  
  typedef unsigned short             InputPixelType2;
  typedef float          OutputPixelType2;

  //  The input and output image types are instantiated using the pixel types.  
  typedef itk::Image< OutputPixelType2, 2 >   OutputImageType2;
  long int k = 0;
  int max_dist = 0;
  for(int i=0; i<z; i++)
  {
	  MyInputImageType2D::Pointer image2D = extract2DImageSlice(im, 2, i);
	  typedef itk::SignedMaurerDistanceMapImageFilter<MyInputImageType2D, OutputImageType2>  DTFilter;
	  DTFilter::Pointer dt_obj= DTFilter::New() ;
	  dt_obj->SetInput(image2D) ;
	  dt_obj->SetSquaredDistance( false );      
	  dt_obj->SetInsideIsPositive( false );
	  try{
		dt_obj->Update() ;
      }
      catch( itk::ExceptionObject & err ){
		std::cerr << "Error calculating distance transform: " << err << endl ;
		return -1;
	  }
	  
	  //   Copy the resulting image into the input array  
      typedef itk::ImageRegionIteratorWithIndex< OutputImageType2 > IteratorType;
      IteratorType iterate(dt_obj->GetOutput(),dt_obj->GetOutput()->GetRequestedRegion());
	  int j=0;	  	 
      while ( j<r*c)
      {	  
		  double ds = iterate.Get();
		  ds = ds*100; //enhance the contrast
		  if(ds<=0)
		  {
			 IMG[k] = 0;
			 //iterate.Set(0.0);//try to write back 
		  }
		  else
			 IMG[k] = (unsigned short) ds;
		  if(IMG[k]>max_dist)
			  max_dist = IMG[k];
		  ++k;
		  ++j;
 		  ++iterate;
	  }	  

	  //By Yousef: try to write out the output at the central slice
	  int cent_slice = (int) z/2;
	  if(i==cent_slice)
	  {
		  typedef    unsigned char     MyInputPixelTypeNew;
		  typedef itk::Image< MyInputPixelTypeNew,  2 >   MyInputImageType2DNew;
		  typedef itk::CastImageFilter< OutputImageType2, MyInputImageType2DNew> myCasterType;
		  myCasterType::Pointer potCaster = myCasterType::New();
		  potCaster->SetInput( dt_obj->GetOutput() );
		  typedef itk::ImageFileWriter< MyInputImageType2DNew > WriterType;
		  WriterType::Pointer writer = WriterType::New();
		  writer->SetFileName("dist_test.tif");
		  writer->SetInput( potCaster->GetOutput() );
		  //writer->Update();		  
	  }
  } 
 
  return max_dist;
}


MyInputImageType2D::Pointer extract2DImageSlice(itk::SmartPointer<MyInputImageType> im, int plane, int slice) 
{
    typedef itk::ExtractImageFilter< MyInputImageType, MyInputImageType2D > FilterType2D;
	FilterType2D::Pointer filter = FilterType2D::New();
    
    MyInputImageType::RegionType inputRegion = im->GetLargestPossibleRegion();
    
    MyInputImageType::SizeType size = inputRegion.GetSize();
    size[plane] = 0;
    
    MyInputImageType::IndexType start = inputRegion.GetIndex();
    const unsigned int sliceNumber = slice;
    start[plane] = sliceNumber;
    
    MyInputImageType::RegionType desiredRegion;
    desiredRegion.SetSize(  size  );
    desiredRegion.SetIndex( start );
    
    filter->SetExtractionRegion( desiredRegion );
    
    filter->SetInput( im );

    MyInputImageType2D::Pointer img = filter->GetOutput();
    img->Update();

    return img;
}

MyInputImageType::Pointer extract3DImageRegion(itk::SmartPointer<MyInputImageType> im, int sz_x, int sz_y, int sz_z, int start_x, int start_y, int start_z)
{
    typedef itk::ExtractImageFilter< MyInputImageType, MyInputImageType > FilterType;
	FilterType::Pointer filter = FilterType::New();   
    
    MyInputImageType::SizeType size;
	size[0] = sz_x;
	size[1] = sz_y;
	size[2] = sz_z;
    
    MyInputImageType::IndexType start;
    start[0] = start_x;
	start[1] = start_y;
	start[2] = start_z;
    
    MyInputImageType::RegionType desiredRegion;
    desiredRegion.SetSize(  size  );
    desiredRegion.SetIndex( start );
	
    
    filter->SetExtractionRegion( desiredRegion );
    
    filter->SetInput( im );

    MyInputImageType::Pointer img = filter->GetOutput();
    img->Update();

    return img;
}

int computeMedian(std::vector< std::vector<unsigned short> > scales, int cntr)
{
	if(cntr == 1)
		return scales[0][0];

	unsigned short* srtList = new unsigned short[cntr];
	for(int i=0; i<cntr-1; i++)
		srtList[i] = scales[i][0];

	//sort the distances
	for(int i=0; i<cntr-1; i++)
	{
		for(int j=i+1; j<cntr; j++)
		{
			if(srtList[j]<srtList[i])
			{
				unsigned short tmp = srtList[i];
				srtList[i] = srtList[j];
				srtList[j] = tmp;
			}
		}
	}

	//if the number of points is odd then the median is the mid point
	//else, it is in between the two mid points
	int res = cntr % 2;
	int mdn;
	if(res!=0)
	{
		mdn = (cntr+1)/2;
		mdn = (int) srtList[mdn-1];
	}
	else
	{
		int mdn1 = cntr/2;
		mdn = (int) (srtList[mdn1]+srtList[mdn1+1])/2;
	}
		
	return mdn;
}

int computeWeightedMedian(std::vector< std::vector<float> > scales, int cntr)
{
	if(cntr == 1)
		return scales[0][0];

	float* srtList = new float[cntr];
	float* wgtList = new float[cntr];
	

	float sumWeights = 0;
	for(int i=0; i<cntr; i++)
	{
		srtList[i] = scales[i][0];
		wgtList[i] = scales[i][1];		
		sumWeights+= wgtList[i];
	}

	//normalize the list of weights
	for(int i=0; i<cntr; i++)
		wgtList[i] /= sumWeights;

	//sort the distances
	for(int i=0; i<cntr-1; i++)
	{
		for(int j=i+1; j<cntr; j++)
		{
			if(srtList[j]<srtList[i])
			{
				float tmp = srtList[i];
				srtList[i] = srtList[j];
				srtList[j] = tmp;
				tmp = wgtList[i];
				wgtList[i] = wgtList[j];
				wgtList[j] = tmp;

			}
		}
	}

	//Find the point at which the cummulative sum exceeds .5
	float cumSum = 0;
	int mdn = -1;
	for(int i=0; i<cntr; i++)
	{
		cumSum+=wgtList[i];
		if(cumSum > .5)
		{
			mdn = srtList[i];
			break;
		}

	}	
		
	delete [] srtList;
	delete [] wgtList;
	return mdn;
}

