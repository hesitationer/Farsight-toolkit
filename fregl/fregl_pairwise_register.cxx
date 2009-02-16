#include "fregl_pairwise_register.h"

#include "itkMeanSquaresImageToImageMetric.h"
#include "itkNormalizedCorrelationImageToImageMetric.h"
#include "itkImageRegistrationMethod.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"
#include "itkImageFileWriter.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkImageSliceIteratorWithIndex.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"

#include <rgrl/rgrl_cast.h>
#include <rgrl/rgrl_trans_affine.h>
#include <rgrl/rgrl_trans_similarity.h>
#include <rgrl/rgrl_trans_reader.h>

#include <vul/vul_timer.h>
#include <vul/vul_file.h>
#include <vnl/vnl_math.h>
#include <vul/vul_reg_exp.h>

#include <vcl_fstream.h>

fregl_pairwise_register::
fregl_pairwise_register( InputImageType::Pointer from_image, 
                         InputImageType::Pointer to_image,
                         float background )
  : from_image_( from_image ),
    to_image_( to_image ),
    background_( background ),
    exhaustive_( false ),
    stack_size_set_( false ),
    smoothing_( 0 )
{}

fregl_pairwise_register::
~fregl_pairwise_register()
{}

void
fregl_pairwise_register::
set_from_image( InputImageType::Pointer from_image )
{
  from_image_ = from_image;
  transform_ = NULL;
}

void
fregl_pairwise_register::
set_to_image( InputImageType::Pointer to_image )
{
  to_image_ = to_image;
  transform_ = NULL;
}

void 
fregl_pairwise_register::
set_exhausive_search(bool exhaustive)
{
  exhaustive_ =  exhaustive;
}

void 
fregl_pairwise_register::
set_stack_size(int size)
{
  // check if the size is smaller than the image size for both
  // from_image_ and to_image_
  InputImageType::SizeType size_from = from_image_->GetLargestPossibleRegion().GetSize();
  InputImageType::SizeType size_to = to_image_->GetLargestPossibleRegion().GetSize();
  if (size_from[2] <= size && size_to[2] <= size) {
    std::cout<<"Warning: The given size not effective"<<std::endl;
    stack_size_set_ = false;
  }
  else {
    std::cout<<"Stack size is set to "<<size<<std::endl;
    stack_size_set_ = true;
    stack_size_ = size;
  }
}

void 
fregl_pairwise_register::
unset_stack_size()
{
  stack_size_set_ = false;
}

fregl_pairwise_register::TransformType::Pointer 
fregl_pairwise_register::
transform()
{
  return transform_;
}

void
fregl_pairwise_register::
set_smoothing(double variance)
{
  smoothing_ = variance;
}

#if defined(VCL_WIN32) && !defined(__CYGWIN__)
//: replace instances of 'from' in 's' with 'to'
static unsigned replace(char from, char to, vcl_string &s)
{
  unsigned c = 0;
  for (unsigned i=0; i<s.size(); ++i)
    if (s[i] == from) {
      c++;
      s[i] = to;
    }
  return c;
}
#endif

bool 
fregl_pairwise_register::
run(double& obj_value, const vcl_string & gdbicp_exe_path, bool scaling)
{
  // Read the 3D image and project it to a 2D image using maximum
  // projection for GDBICP. The idea is to run GDBICP on the 2D image
  // to obtain a transformation accurate in x-y plan, since it is
  // where the major motion is. The shift in the z-stack is taken care
  // of later by the coarse-to-fine refinement in 3D
  vul_timer timer;
  timer.mark();
  ImageType2D::Pointer from_image_2d, to_image_2d;
  vcl_cout << "Projecting the from_image ....\n";
  maximum_projection( from_image_, from_image_2d );
  vcl_cout << "Projecting the to_image ....\n";
  maximum_projection( to_image_, to_image_2d );

  vcl_cout << " Time spent in reading and projecting images: " << vcl_endl;
  timer.print( vcl_cout );
  vcl_cout << "\n";

  // output max projected images to files and read them back as vxl
  // images. This might not be a very neat approach, but it is much
  // easier this way, since rrl_gdbicp_info takes image filenames.

  vcl_string from_2dfilename = vcl_string("xxx_from_image_proj.tif");
  vcl_string to_2dfilename = vcl_string("xxx_to_image_proj.tif");

  typedef itk::ImageFileWriter< ImageType2D >  WriterType2D;

  try {
    WriterType2D::Pointer writer2D = WriterType2D::New();
    writer2D->SetFileName( from_2dfilename );
    writer2D->SetInput( from_image_2d );
    writer2D->Update();
    
    writer2D->SetFileName( to_2dfilename );
    writer2D->SetInput( to_image_2d );
    writer2D->Update();
  }
  catch(itk::ExceptionObject& e) {
    vcl_cout << e << vcl_endl;
    return 1;
  }

  try {
    vcl_string path = gdbicp_exe_path;

#if defined(VCL_WIN32) && !defined(__CYGWIN__)
    replace('/', '\\', path);
    vcl_string exe_command = path+vcl_string("gdbicp.exe ")+from_2dfilename+vcl_string(" ")+to_2dfilename+vcl_string(" -model 0 -no_render -complete");
#else
    vcl_string exe_command = path+vcl_string("gdbicp ")+from_2dfilename+vcl_string(" ")+to_2dfilename+vcl_string(" -model 0 -no_render -complete");
#endif
    // The xform file generated is
    // mosaic_xxx_from_image_proj_to_xxx_to_image_proj.xform
    vcl_cout<<"Run: "<<exe_command<<vcl_endl;
    int status = 2;
    status = vcl_system(exe_command.c_str());
    
    if ( status > 1 ) {
      vcl_cout << "Registration failed in 2D. Please try a different channel with more intensity variation." << vcl_endl;
      return false;
    }

    // Read in the file
    // mosaic_xxx_from_image_proj_to_xxx_to_image_proj.xform back to
    // memory
    vcl_ifstream reg_info("mosaic_xxx_from_image_proj_to_xxx_to_image_proj.xform");
    rgrl_transformation_sptr xform_2d = read_2d_xform( reg_info );

    if ( !valid_2d_xform(xform_2d, scaling) ) {
        vcl_cout << "Invalid 2D xform. Please try a different channel with more intensity variation." << vcl_endl;
      return false;
      }
    // Now work out the center of mass in z-direction in the overlap
    // volume estimated by the x-y shift.
    //
    double t_z = compute_z_shift (xform_2d, from_image_, to_image_, 
                                  from_image_2d, to_image_2d, background_);
    
    // Image type of float are needed for intensity-based
    // registration. Crop only the overlap volume and release the
    // original images.
    InternalImageType:: Pointer from_image_crop, to_image_crop;
    from_image_crop = crop_image(from_image_);
    to_image_crop = crop_image(to_image_);
    

    // Force early release of images that are not needed anymore.
    from_image_2d = 0;
    to_image_2d = 0;
    std::cout<<"End of cropping"<<std::endl;

    TransformType::ParametersType parameters(12);
    convert_rgrl_to_itk_xform( xform_2d, -t_z, parameters);
    
    // Now perform intensity-based registration on the cropped 3D
    // volume
    //

    std::cout<<"Running 3D intensity-based registration ..."<<std::endl;
    //typedef itk::MeanSquaresImageToImageMetric< InternalImageType, InternalImageType > MetricType;
    typedef itk::NormalizedCorrelationImageToImageMetric< InternalImageType, InternalImageType > MetricType;
    typedef itk::LinearInterpolateImageFunction< InternalImageType, double> InterpolatorType;
    typedef itk::ImageRegistrationMethod< InternalImageType, InternalImageType > RegistrationType;
    typedef itk::RegularStepGradientDescentOptimizer    OptimizerType;
    typedef OptimizerType::ScalesType OptimizerScalesType;

    // Exhaustive search is not work yet!!!
    if ( exhaustive_ ) {
      std::cout<<"Running exhaustive search"<<std::endl;

      InterpolatorType::Pointer interpolator = InterpolatorType::New();
      MetricType::Pointer metric = MetricType::New();
      TransformType::Pointer transform = TransformType::New();
      metric->SetInterpolator( interpolator );
     
      metric->SetFixedImage( to_image_crop );
      metric->SetMovingImage( from_image_crop );
      metric->SetFixedImageRegion( to_image_crop->GetRequestedRegion() );
      //metric->Initialize();

      double value, min_value;
      int index = 0;
      bool value_set = false;
      for( int dx = -10; dx <=10; dx++) {
        parameters[11] = -t_z + dx;
        transform->SetParameters( parameters );
        metric->SetTransform( transform );
        value = metric->GetValue( parameters );
        if (!value_set) {
          value_set = true;
          min_value = value;
          index = dx;
        }
        else if ( value < min_value) {
          min_value = value;
          index = dx;
        }
        vcl_cout<<"z_shift = "<<-t_z + dx<<" and value = "<<value<<vcl_endl;
      }
      parameters[11] = -t_z + index;
      if (!transform_) transform_ = TransformType::New();
      transform_->SetParameters( parameters );
    }
    else {
    
      InterpolatorType::Pointer interpolator = InterpolatorType::New();
      MetricType::Pointer metric = MetricType::New();
      TransformType::Pointer transform = TransformType::New();
      RegistrationType::Pointer registrator = RegistrationType::New();
      OptimizerType::Pointer optimizer = OptimizerType::New();

      registrator->SetTransform( transform );
      registrator->SetOptimizer( optimizer );
      registrator->SetInterpolator( interpolator );
      registrator->SetMetric( metric );
      
      registrator->SetFixedImage( to_image_crop );
      registrator->SetMovingImage( from_image_crop );
      registrator->SetFixedImageRegion(to_image_crop->GetRequestedRegion());
      registrator->SetInitialTransformParameters( parameters );
      
      optimizer->SetMaximumStepLength( 4.0 );
      optimizer->SetMinimumStepLength( 0.1);
      optimizer->SetNumberOfIterations( 100 );
      optimizer->MaximizeOff();

      // set the parameter scale to limit the freedom in affine parts
      int num_params = 12;    
      OptimizerScalesType optimizerScales(num_params);
      for (unsigned int i = 0; i<9; i++) {
        //affine components
        optimizerScales[i] = 1.0;
      }
      for (unsigned int i = 0; i<3; i++) {
        //translation components
        optimizerScales[i+9] = 1.0/1000000;
      }
      optimizer->SetScales(optimizerScales);
      
      // To add the observer to watch the progress of the registration
      CommandIteration::Pointer   observer  = CommandIteration::New();
      optimizer->AddObserver( itk::IterationEvent(), observer );
      
      // Now run the registration
      registrator->StartRegistration();

      // Set the final transform
      TransformType::ParametersType final_parameters;
      final_parameters = registrator->GetLastTransformParameters();
      if (!transform_) transform_ = TransformType::New();
      transform_->SetParameters( final_parameters );
      obj_value = optimizer->GetValue();
      /*
      if (obj_value > 1000) { 
        vcl_cout << "Registration failed in 3D. Please try a different channel with more intensity variation." << vcl_endl;
        return false;
      }
      */
    }
    
  }
  catch(itk::ExceptionObject& e) {
    vcl_cout << e << vcl_endl;
    return false;
  }

  return true;
  
}

/************** Private Functions ************************************/

// Project a 3D image to 2D image using maximum projection
void 
fregl_pairwise_register::
maximum_projection( InputImageType::Pointer image_3d,
                    ImageType2D::Pointer& image_2d )
{
  typedef itk::ImageLinearIteratorWithIndex< ImageType2D > LinearIteratorType;
  typedef itk::ImageSliceIteratorWithIndex< InputImageType >  SliceIteratorType1;
  
  ImageType2D::RegionType region;
  ImageType2D::RegionType::SizeType size;
  ImageType2D::RegionType::IndexType index;
  ImageType2D::SpacingType spacing;
  ImageType2D::PointType origin;
  
  InputImageType::RegionType requestedRegion = image_3d->GetRequestedRegion();
  index[ 0 ] = 0;
  index[ 1 ] = 0;
  size[ 0 ] = requestedRegion.GetSize()[ 0 ];
  size[ 1 ] = requestedRegion.GetSize()[ 1 ];
  region.SetSize( size );
  region.SetIndex( index );

  image_2d = ImageType2D::New();
  image_2d->SetRegions( region );
  image_2d->Allocate();

  //Set the iterator
  SliceIteratorType1 output3DIt( image_3d, image_3d->GetRequestedRegion() );
  LinearIteratorType output2DIt( image_2d, image_2d->GetRequestedRegion() );

  unsigned int direction[2];
  direction[0] = 0;
  direction[1] = 1;

  output3DIt.SetFirstDirection( direction[1] );
  output3DIt.SetSecondDirection( direction[0] );
  output2DIt.SetDirection( 1 - direction[0] );

  // Initialized the 2D image
  output2DIt.GoToBegin();
  while ( ! output2DIt.IsAtEnd() ) {
    while ( ! output2DIt.IsAtEndOfLine() ) {
      output2DIt.Set( itk::NumericTraits<unsigned short>::NonpositiveMin() );
      ++output2DIt;
    }
    output2DIt.NextLine();
  }

  // Now do the max projection
  output3DIt.GoToBegin();
  output2DIt.GoToBegin();

  while( !output3DIt.IsAtEnd() ) {
    while ( !output3DIt.IsAtEndOfSlice() ) {
      while ( !output3DIt.IsAtEndOfLine() ) {
        output2DIt.Set( vnl_math_max( static_cast<unsigned char>(output3DIt.Get()), output2DIt.Get() ));
        ++output3DIt;
        ++output2DIt;
      }
      output2DIt.NextLine();
      output3DIt.NextLine();
    }
    output2DIt.GoToBegin();
    output3DIt.NextSlice();
  }
}

// Compute the z shift, and set the region of interest to the overlap
double
fregl_pairwise_register::
compute_z_shift( rgrl_transformation_sptr fw_xform, 
                 InputImageType::Pointer from_image_3d,
                 InputImageType::Pointer to_image_3d,
                 ImageType2D::Pointer from_image_2d, 
                 ImageType2D::Pointer to_image_2d,
                 float bg)
{

  typedef itk::ImageRegionConstIterator< ImageType2D > ConstIterator2DType;

  vnl_vector_fixed<double,2> loc, loc_mapped;
  ImageType2D::RegionType::IndexType index, index_mapped;
  ImageType2D::RegionType from_region, to_region;
  InputImageType::RegionType::IndexType index3d;
  InputImageType::RegionType region3d;
  vnl_vector_fixed<double,3> from_weighted_sum(0.0);

  from_region = from_image_2d->GetRequestedRegion(); 
  to_region = to_image_2d->GetRequestedRegion(); 
  
  // first_moment for from_image
  {
    vnl_vector_fixed<double,2> x0(0.0),x1(0.0);
    bool size_set = false;
    double count = 0;
    ConstIterator2DType output2DIt( from_image_2d, from_region);
    region3d = from_image_3d->GetRequestedRegion();

    for ( output2DIt.GoToBegin(); !output2DIt.IsAtEnd(); ++output2DIt ) {
      index = output2DIt.GetIndex();
      loc[0] = index[0];
      loc[1] = index[1];
      loc_mapped = fw_xform->map_location( loc );
      index_mapped[0] = loc_mapped[0];
      index_mapped[1] = loc_mapped[1];
      if ( to_region.IsInside(index_mapped) ) {
        // decide the bounding box
        if (!size_set) {
          x0 = loc;
          x1 = loc;
          size_set = true;
        }
        if (x0[0] > loc[0]) x0[0] = loc[0];
        if (x0[1] > loc[1]) x0[1] = loc[1];
        if (x1[0] < loc[0]) x1[0] = loc[0];
        if (x1[1] < loc[1]) x1[1] = loc[1];
        
        // Now scan through the z-stack of the same x-y loc.
        index3d[0] = index[0];
        index3d[1] = index[1];
        index3d[2] = 0;
        while ( region3d.IsInside(index3d) ) {
          InputImageType::PixelType pixelValue = from_image_3d->GetPixel( index3d );
          if ( pixelValue > bg ){
            from_weighted_sum[0] += index3d[0]*pixelValue;
            from_weighted_sum[1] += index3d[1]*pixelValue;
            from_weighted_sum[2] += index3d[2]*pixelValue;
            count+= static_cast<double>(pixelValue);
          }
          index3d[2]++;
        }
      }
    }
    from_weighted_sum /= count;
    vcl_cout<<" First_moment for from_image = "<<from_weighted_sum<<vcl_endl;

    // Set the RequestedRegion of the from_image
    InputImageType::IndexType start;
    start[0] = x0[0];
    start[1] = x0[1];
    if (!stack_size_set_) {
      start[2] = 0; //start from the first z-slice
    }
    else {
      int center = index3d[2]/2;
      int half_size = stack_size_/2;
      if (center - half_size - 10 > 0) 
        start[2] = center - half_size - 10; // make the window slightly larger
      else start[2] = 0;
    }
    vcl_cout<<"Start index = "<<start[0]<<", "<<start[1]<<", " <<start[2]<<vcl_endl;

    InputImageType::SizeType size;
    size[0] = x1[0]-x0[0] + 1;
    size[1] = x1[1]-x0[1] + 1;
    size[2] = index3d[2];
    if (!stack_size_set_ || start[2] == 0) {
      size[2] = index3d[2];
    }
    else {
      size[2] = stack_size_+20;
    }
    vcl_cout<<"Size = "<<size[0]<<", "<<size[1]<<", "<<size[2]<<vcl_endl;
    
    InputImageType::RegionType region;
    region.SetSize( size );
    region.SetIndex( start );
    from_image_3d->SetRequestedRegion( region );
  }

  // first_moment for to_image
  rgrl_transformation_sptr bw_xform = fw_xform->inverse_transform();
  vnl_vector_fixed<double,3> to_weighted_sum(0.0);
  {
    vnl_vector_fixed<double,2> x0(0.0),x1(0.0);
    bool size_set = false;
    double count = 0;
    ConstIterator2DType output2DIt( to_image_2d, to_region);
    region3d = to_image_3d->GetRequestedRegion();

    for ( output2DIt.GoToBegin(); !output2DIt.IsAtEnd(); ++output2DIt ) {
      index = output2DIt.GetIndex();
      loc[0] = index[0];
      loc[1] = index[1];
      loc_mapped = bw_xform->map_location( loc );
      index_mapped[0] = loc_mapped[0];
      index_mapped[1] = loc_mapped[1];
      if ( from_region.IsInside(index_mapped) ) {
        // decide the bounding box
        if (!size_set) {
          x0 = loc;
          x1 = loc;
          size_set = true;
        }
        if (x0[0] > loc[0]) x0[0] = loc[0];
        if (x0[1] > loc[1]) x0[1] = loc[1];
        if (x1[0] < loc[0]) x1[0] = loc[0];
        if (x1[1] < loc[1]) x1[1] = loc[1];
        
        // Now scan through the z-stack of the same x-y loc.
        index3d[0] = index[0];
        index3d[1] = index[1];
        index3d[2] = 0;
        while ( region3d.IsInside(index3d) ) {
          InputImageType::PixelType pixelValue = to_image_3d->GetPixel( index3d );
          if (pixelValue > bg) {
            to_weighted_sum[0] += index3d[0]*pixelValue;
            to_weighted_sum[1] += index3d[1]*pixelValue;
            to_weighted_sum[2] += index3d[2]*pixelValue;
            count+=static_cast<double>(pixelValue);
          }
          index3d[2]++;
        }
      }
    }
    to_weighted_sum /= count;
    vcl_cout<<" First_moment for to_image = "<<to_weighted_sum<<vcl_endl;

    // Set the RequestedRegion of the to_image
    InputImageType::IndexType start;
    start[0] = x0[0];
    start[1] = x0[1];
    if (!stack_size_set_) {
      start[2] = 0; //start from the first z-slice
    }
    else {
      int center = index3d[2]/2;
      int half_size = stack_size_/2;
      start[2] = center - half_size;
    }
    vcl_cout<<"Start index = "<<start[0]<<", "<<start[1]<<", " <<start[2]<<vcl_endl;

    InputImageType::SizeType size;
    size[0] = x1[0]-x0[0] + 1;
    size[1] = x1[1]-x0[1] + 1;
    if (!stack_size_set_) {
      size[2] = index3d[2];
    }
    else {
      size[2] = stack_size_;
    }
    vcl_cout<<"Size = "<<size[0]<<", "<<size[1]<<", "<<size[2]<<vcl_endl;
    
    InputImageType::RegionType region;
    region.SetSize( size );
    region.SetIndex( start );
    to_image_3d->SetRequestedRegion( region );
  }

  return to_weighted_sum[2] - from_weighted_sum[2];
}

// Crop the image to the region of interest. This step is to reduce
// memory consumption, since it is too expensive to have image type of
// float. If smoothing_ is non-zero, DiscreteGaussianImageFilter is
// applied.
fregl_pairwise_register::InternalImageType::Pointer
fregl_pairwise_register::
crop_image( InputImageType::Pointer image )
{
  typedef itk::ImageRegionConstIterator< InputImageType > ConstIteratorType;
  typedef itk::ImageRegionIterator< InternalImageType> IteratorType;

  // Crop the from image 
  InternalImageType::Pointer image_crop = InternalImageType::New();
  {
    InternalImageType::RegionType crop_region;
    InternalImageType::IndexType start;
    crop_region = image->GetRequestedRegion();
    start[0] = 0;
    start[1] = 0;
    start[2] = 0;
    crop_region.SetIndex( start );
    image_crop->SetRegions( crop_region );
    std::cout<<"Cropped region"<<std::endl;
    crop_region.Print(std::cout);

    const InputImageType::SpacingType& spacing = image->GetSpacing();
    const InputImageType::PointType& inputOrigin = image->GetOrigin();
    const InputImageType::IndexType& inputStart = image->GetRequestedRegion().GetIndex();
    double outputOrigin[ 3 ];
    for(unsigned int i=0; i< 3; i++)
      {
        outputOrigin[i] = inputOrigin[i] + spacing[i] * inputStart[i];
      }
    image_crop->SetSpacing( spacing );
    image_crop->SetOrigin( outputOrigin );
    /*
    vcl_cout<<"outputOrigin from ="<<outputOrigin[0]<<", "<<outputOrigin[1]
            <<", "<<outputOrigin[2]<<vcl_endl;
    */
    image_crop->Allocate();
    
    // now copy the sub-image
    const InputImageType::RegionType& inputRegion = image->GetRequestedRegion();
    ConstIteratorType inputIt( image, inputRegion );
    IteratorType outputIt( image_crop, crop_region);
    for ( inputIt.GoToBegin(), outputIt.GoToBegin(); !inputIt.IsAtEnd();
          ++inputIt, ++outputIt)
      {
        outputIt.Set( inputIt.Get() );
      }
  }

  if (!smoothing_)
    return image_crop;

  // perform smoothing

  typedef itk::DiscreteGaussianImageFilter< InternalImageType,InternalImageType > SmoothingFilterType;
  typedef itk::RescaleIntensityImageFilter< InternalImageType, InternalImageType> RescalerType;

  SmoothingFilterType::Pointer smoother = SmoothingFilterType::New();
  RescalerType::Pointer rescaler = RescalerType::New();

  smoother->SetInput( image_crop );
  smoother->SetVariance(smoothing_);
  smoother->SetMaximumKernelWidth(15);
  rescaler->SetInput( smoother->GetOutput() );
  rescaler->SetOutputMinimum( 0 );
  rescaler->SetOutputMaximum( 250 );
  try {
    rescaler->Update();
  }
  catch(itk::ExceptionObject& e) {
    vcl_cout << e << vcl_endl;
  }
  
  return rescaler->GetOutput();
}

// Check the validity of the 2D xform to be sure the affine components
// is more or less identity.
bool 
fregl_pairwise_register::
valid_2d_xform( rgrl_transformation_sptr xform2d, bool scaling )
{
  vnl_matrix<double> A;

  if (xform2d->is_type(rgrl_trans_similarity::type_id())){
    rgrl_trans_similarity* sim2d = rgrl_cast<rgrl_trans_similarity*>(xform2d);
    vcl_cout<<"2D transformation :"<<"A = "<<sim2d->A()
            <<", t = "<<sim2d->t()<<vcl_endl;
    A = sim2d->A();
  }
  else {// type = affine
    rgrl_trans_affine* affine2d = rgrl_cast<rgrl_trans_affine*>(xform2d);
    vcl_cout<<"2D transformation :"<<"A = "<<affine2d->A()
            <<", t = "<<affine2d->t()<<vcl_endl;
    A = affine2d->A();
  }

  // Condition 1: Both the diagonal elements are > 0
  if ( A(0,0)<0 || A(1,1)<0) {
    vcl_cout<<"2D xform failed the condition: Both the diagonal elements are > 0"<<vcl_endl;
    return false;
  } 
  // Condition 2: No substaintial changes in the area. This is done by
  // checking the determinant of the affine matrix. If area conserved
  // when det|A|=1.
  float det = A(0,0)*A(1,1) - A(0,1)*A(1,0);
  if ( !scaling && vnl_math_abs(1-det) > 0.2 ) {
    vcl_cout<<"2D xform failed the condition: No substaintial changes in the area."<<vcl_endl;
    return false;
  }
  // Condition 3: No substaintial deformation
  //if ( vnl_math_abs(1-A(0,0)*A(1,1)) > 0.2 ||  vnl_math_abs(A(0,1)*A(1,0))>0.001) return false;
  if ( !scaling && vnl_math_abs(1-A(0,0)*A(1,1)) > 0.2 ||  vnl_math_abs(A(0,1)*A(1,0))>0.01){
    vcl_cout<<"2D xform failed the condition: No substaintial deformation"<<vcl_endl;
    return false;
  }
  return true;
}


// Convert the transform from the rgrl format to itk format. The
// result can be either affine or similarity
void 
fregl_pairwise_register::
convert_rgrl_to_itk_xform( rgrl_transformation_sptr xform2d,
                           double z_displacement,
                           TransformType::ParametersType& parameters)
{
  vnl_matrix<double> A_inv;
  vnl_vector<double> t_inv;

  if (xform2d->is_type(rgrl_trans_similarity::type_id())){
    rgrl_trans_similarity* sim2d_tmp = rgrl_cast<rgrl_trans_similarity*>(xform2d);
    vcl_cout<<"2D transformation :"<<"A = "<<sim2d_tmp->A()
            <<", t = "<<sim2d_tmp->t()<<vcl_endl;
    rgrl_transformation_sptr sim2d_inv = xform2d->inverse_transform();
    rgrl_trans_similarity* sim2d_inverse = rgrl_cast<rgrl_trans_similarity*>(sim2d_inv);
    vcl_cout<<"Inversed 2D transformation :"<<"A = "<<sim2d_inverse->A()
            <<", t = "<<sim2d_inverse->t()<<vcl_endl;
    A_inv = sim2d_inverse->A();
    t_inv = sim2d_inverse->t();
  }
  else {// type = affine
    rgrl_trans_affine* affine2d_tmp = rgrl_cast<rgrl_trans_affine*>(xform2d);
    vcl_cout<<"2D transformation :"<<"A = "<<affine2d_tmp->A()
            <<", t = "<<affine2d_tmp->t()<<vcl_endl;
    rgrl_transformation_sptr affine2d_inv = xform2d->inverse_transform();
    rgrl_trans_affine* affine2d_inverse = rgrl_cast<rgrl_trans_affine*>(affine2d_inv);
    vcl_cout<<"Inversed 2D transformation :"<<"A = "<<affine2d_inverse->A()
            <<", t = "<<affine2d_inverse->t()<<vcl_endl;
    A_inv = affine2d_inverse->A();
    t_inv = affine2d_inverse->t();
  }

  vnl_matrix<double> theta_inv(3,3);
  theta_inv.set_identity();
  theta_inv(0,0) = A_inv(0,0);
  theta_inv(0,1) = A_inv(0,1);
  theta_inv(1,0) = A_inv(1,0);
  theta_inv(1,1) = A_inv(1,1);
  theta_inv(0,2) = t_inv(0);
  theta_inv(1,2) = t_inv(1);

  vnl_matrix<double> param_matrix = theta_inv;
  vcl_cout<<"Input matrix = \n"<<param_matrix<<vcl_endl;

  parameters[0] = param_matrix(0,0);
  parameters[1] = param_matrix(0,1);
  parameters[2] = 0;
  parameters[3] = param_matrix(1,0);
  parameters[4] = param_matrix(1,1);
  parameters[5] = 0;
  parameters[6] = 0;
  parameters[7] = 0;
  parameters[8] = 1;
  parameters[9] = param_matrix(0,2);
  parameters[10] = param_matrix(1,2);
  parameters[11] = z_displacement;
  
}

rgrl_transformation_sptr 
fregl_pairwise_register::
read_2d_xform( vcl_istream& reg_info )
{
  vcl_string line, val, name, contents;
  const vcl_string space = " \r\t\n";
  vul_reg_exp pair("(.*)=(.*)$");
  vcl_string::size_type pos;
  
  while (reg_info && !reg_info.eof()) {
    // reset
    line.clear();
    vcl_getline (reg_info,line);
    line = string_trim (line,space);
 
    // empty line
    if (line.size ()==0)
        continue;

    if( !pair.find( line ) ) {
      vcl_cerr << "Error: Cannot parse this line: " << line 
               << "       Operation aborted. " << vcl_endl;
      return false;
    }
    
    name = pair.match(1);
    name = string_trim (name, space);
    val = pair.match(2);
    val = string_trim (val, space);

    if( name == vcl_string("xform") ) { //start of the xform
      bool found_closing = false;
      contents.clear();

      /*
      // in case some stuff after '<'
      if( val.size() > 1 ) {
        contents.append( val.substr( 1, val.size() -1 ) );
        contents.append( "\n" );
      }
      */

      while( reg_info && !reg_info.eof() ) {
        // reset
        line.clear();
        vcl_getline( reg_info, line );
        
        if( (pos=line.find_first_of(">")) != vcl_string::npos ) {
          // add anything before closing curly bracket
          if( pos!= 0 )
            contents.append( line.substr( 0, pos-1 ) );
          // done
          found_closing = true;
          break;
        } else {
          contents.append( line );
          contents.append( "\n" );
        }
      }
      
      if( !found_closing ) {
        vcl_cerr << "Error in reading 2D xform: Cannot find the closing '>'" 
                 << "       Operation aborted. " << vcl_endl;
        return NULL;
      }
      break;
    }
  }

  // Convert contents to rgrl transformation
  vcl_istringstream istrstr( contents );
  return rgrl_trans_reader::read( istrstr );
}

vcl_string 
fregl_pairwise_register::
string_trim (const vcl_string& source, const vcl_string& pat)
{
  vcl_string::size_type po;
  vcl_string result (source);
  
  // if an empty line
  if (result.size()==0)
    return result;

  // Trimming the beginning
  po = result.find_first_not_of (pat);
  if (po ==  vcl_string::npos)
    result = "";
  else
    result = result.substr (po, result.size()-po);

  // if an empty line
  if (result.size()==0)
    return result;

  // Trimming the end
  po = result.find_last_not_of (pat);
  assert (po != vcl_string::npos);

  result = result.substr (0, po+1);
  
  return result;
}
