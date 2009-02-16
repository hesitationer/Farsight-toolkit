//: Executable program to fuse two 3D image stacks to one. The input
//can be gray or rgb color. The output is always a gray image. The
//maximum intensity at each pixel is taken.
//
#include "itkImage.h"
#include "itkImageRegionConstIterator.h"
#include "itkImageRegionIterator.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkRGBAPixel.h"
#include "itkTIFFImageIO.h"

#include "vnl/vnl_math.h"

#include <Common/fsc_channel_accessor.h>

typedef itk::Image< unsigned char, 3 > ImageType;
typedef itk::ImageRegionConstIterator< ImageType > RegionConstIterator;
typedef itk::ImageRegionIterator< ImageType > RegionIterator;

ImageType::Pointer
read_image( std::string const & file_name, int channel )
{
  std::cout<<"Reading the image "<<file_name<<std::endl;

  ImageType::Pointer image;

  // Get pixel information
  itk::TIFFImageIO::Pointer io = itk::TIFFImageIO::New();
  io->SetFileName(file_name);
  io->ReadImageInformation();
  int pixel_type = (int)io->GetPixelType();
  std::cout<<"Pixel Type = "<<pixel_type<<std::endl; //1 - grayscale, 2-RGB, 3-RGBA, etc.,

  if (pixel_type == 3) { //RGBA pixel type
    typedef fsc_channel_accessor<itk::RGBAPixel<unsigned char>,3 > ChannelAccType;
    ChannelAccType channel_accessor(file_name);
    image = channel_accessor.get_channel(ChannelAccType::channel_type(channel));
  }
  else if (pixel_type == 2) { //RGA pixel type
    typedef fsc_channel_accessor<itk::RGBPixel<unsigned char>,3 > ChannelAccType;
    ChannelAccType channel_accessor(file_name);
    image = channel_accessor.get_channel(ChannelAccType::channel_type(channel));
  }
  else {// Gray image
    typedef itk::ImageFileReader< ImageType > ReaderType;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName( file_name );
    try {
      reader->Update();
    }
    catch(itk::ExceptionObject& e) {
      vcl_cout << e << vcl_endl;
    }
    image =  reader->GetOutput();
  }
  return image;
}


int main(int argc, char* argv[])
{
  if (argc<4) {
    std::cerr << "Usage: " << argv[0] << " Inputimage1 InputImage2 OutputGrayImage rgb_channel  ";
    return EXIT_FAILURE;
  }

  std::string filename1 = argv[1];
  std::string filename2 = argv[2];
  const char *outfilename = argv[3];
  int channel = 0;

  if (argc == 5) channel = atoi(argv[4]);

  ImageType::Pointer image1, image2, image_out;
  image1 = read_image( filename1, channel );
  image2 = read_image( filename2, channel );

  // Perform the fusing here
  //
  std::cout<<"Fusing the images"<<std::endl;
  image_out = ImageType::New();
  image_out->SetRegions(image1->GetRequestedRegion());
  image_out->Allocate();

  //Set the iterator
  RegionConstIterator inputIt1( image1, image1->GetRequestedRegion() );
  RegionConstIterator inputIt2( image2, image2->GetRequestedRegion() );
  RegionIterator outputIt( image_out, image_out->GetRequestedRegion() );

  for ( inputIt1.GoToBegin(), inputIt2.GoToBegin(), outputIt.GoToBegin(); 
        !inputIt1.IsAtEnd();  ++inputIt1, ++inputIt2, ++outputIt)
      {
        outputIt.Set( vnl_math_max(inputIt1.Get(), inputIt2.Get()) );
      }

  typedef itk::ImageFileWriter< ImageType >  WriterType3D;

  WriterType3D::Pointer writer3D = WriterType3D::New();
  writer3D->SetFileName( outfilename );
  writer3D->SetInput( image_out );
  writer3D->Update();

  return 0;
}

