//
// header-start
//////////////////////////////////////////////////////////////////////////////////
//
// \file      yuvimage.cpp
//
// \brief     This file belongs to the C++ tutorial project
//
// \author    Bernard
//
// \copyright Copyright 2019
//
//////////////////////////////////////////////////////////////////////////////////
// header-end
//
#include "yuvimage_student.h"
#include <QString>
#include <QDebug>
#include <cmath>
#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include "gse4_util.h"
#include <chrono>
#include <thread>
#include "imageparameters.h"
#define ALIGN32(X) X __attribute__((aligned(32)))

// Convert a single yuv point to rgb color
// based on BT.709 formula
// Y = 0.2126*R + 0.7152*G + 0.0722*B
//
static
QRgb convert_yuv_to_rgb(uint8_t y, uint8_t u, uint8_t v) {
    static GSE4::Clamp<double> clamp_to_rgb(0., 255.);
    double r = y + 0*u       + 1.28033*v;
    double g = y - 0.21482*u - 0.38059*v;
    double b = y + 2.12798*u +       0*v;
    return qRgb(clamp_to_rgb(r), clamp_to_rgb(g), clamp_to_rgb(b));
}

// Convert raw yuv data to rgb image
// process image pixel by pixel
// calling the conversion yuv to rgb each time
// The raw yuv data is in 4:2:0 format
// i.e a single chroma value (u or v) is applicable to 4 luma values.
// We haven't implemented the filtering for upscaling described in
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
//

int YuvImage::trans(int i) {
    int rang = (i/width_)/2;
    int colone = (i%width_)/2;
    return colone+rang*(width_/2);
}
//int test = 0;
//int testt[4]={0,0,0,0};
void YuvImage::yuv_to_rgb(size_t groupe_idx) {
    int i,i_begin,i_end;
    i_begin = groupe_idx*total_pixels/nb_threads;
    i_end = (groupe_idx+1)*total_pixels/nb_threads;
    std::cout<<"i_begin :"<<i_begin<<std::endl;
    std::cout<<"i_end :"<<i_end<<std::endl;
    if(ImageParameters::instance().get_simd()){
       SIMD_yuv_to_rgb(i_begin, i_end);
    } else {
        for(i=i_begin ; i < i_end ; ++i) {
            r_[i] = 1.1643836*y_raw_[i] +         0*u_raw_[trans(i)] + 1.8765140*v_raw_[trans(i)] - 268.2065015;
            g_[i] = 1.1643836*y_raw_[i] - 0.2132486*u_raw_[trans(i)] - 0.5578116*v_raw_[trans(i)] + 82.8546329;
            b_[i] = 1.1643836*y_raw_[i] + 2.1124018*u_raw_[trans(i)] -         0*v_raw_[trans(i)] - 289.0175656;
        }
    }
}

//Utiliser les instructions SIMD pour transformer les valeurs de yuv à rgb

void YuvImage::SIMD_yuv_to_rgb(int begin, int end){
    int idx = begin;
    int i = 0;
    //r_ = new double [width_*height_];
    //g_ = new double [width_*height_];
    //b_ = new double [width_*height_];

    //initialiser les tableau de coefficient de rgb et d'addition
    int32_t ALIGN32(coef_red[]) = { 298, 0, 480, -68660+128 };
    int32_t ALIGN32(coef_green[]) = { 298, -55, -143, 21211};
    int32_t ALIGN32(coef_blue[]) = { 298, 541, 0, -73988 };
    int32_t ALIGN32(coef_add[]) = { 128, 128, 128, 128 };

    //initialiser les registres positionnés de coefficient de rgb et d'addition
    __m128i r128_coef_red = _mm_load_si128(reinterpret_cast<__m128i const*>(coef_red));
    __m128i r128_coef_green = _mm_load_si128(reinterpret_cast<__m128i const*>(coef_green));
    __m128i r128_coef_blue = _mm_load_si128(reinterpret_cast<__m128i const*>(coef_blue));
    __m128i r128_coef_add = _mm_load_si128(reinterpret_cast<__m128i const*>(coef_add));

    //Calculer rgb
    for(idx ; idx < end; ++idx) {
         i=YuvImage::trans(idx);

         int32_t ALIGN32(coef_yuv[]) = { y_raw_[idx], u_raw_[i], v_raw_[i], 1 };
         __m128i r128_coef_yuv = _mm_load_si128(reinterpret_cast<__m128i const*>(coef_yuv));

         //faire l'opération de multiplication, addition et décalage à droite.
         __m128i red1 = _mm_mullo_epi32 (r128_coef_yuv, r128_coef_red);
         __m128i red2 = _mm_add_epi32 (red1, r128_coef_add);
         __m128i red3 = _mm_srai_epi32 (red2, 8);

         //faire la somme de quatre parties dans le registre 128 bits pour obtenir la valeur de r
         r_[idx] = _mm_extract_epi32(red3, 0) + _mm_extract_epi32(red3, 1)
                   + _mm_extract_epi32(red3, 2) + _mm_extract_epi32(red3, 3);

         //calculer g
         __m128i green1 = _mm_mullo_epi32 (r128_coef_yuv, r128_coef_green);
         __m128i green2 = _mm_add_epi32 (green1, r128_coef_add);
         __m128i green3 = _mm_srai_epi32 (green2, 8);
         g_[idx] = _mm_extract_epi32(green3, 0) + _mm_extract_epi32(green3, 1)
                   + _mm_extract_epi32(green3, 2) + _mm_extract_epi32(green3, 3);

         //calculer b
         __m128i blue1 = _mm_mullo_epi32 (r128_coef_yuv, r128_coef_blue);
         __m128i blue2 = _mm_add_epi32 (blue1, r128_coef_add);
         __m128i blue3 = _mm_srai_epi32 (blue2, 8);
         b_[idx] = _mm_extract_epi32(blue3, 0) + _mm_extract_epi32(blue3, 1)
                   + _mm_extract_epi32(blue3, 2) + _mm_extract_epi32(blue3, 3);
    }

}


//void YuvImage::yuv_to_rgb(size_t groupe_idx) {
//    int i;
//    r_ = new double [width_*height_];
//    g_ = new double [width_*height_];
//    b_ = new double [width_*height_];

//   for(i=0 ; i < width_*height_ ; ++i) {
//        r_[i] = 1.1643836*y_raw_[i] +         0*u_raw_[trans(i)] + 1.8765140*v_raw_[trans(i)] - 268.2065015;
//        g_[i] = 1.1643836*y_raw_[i] - 0.2132486*u_raw_[trans(i)] - 0.5578116*v_raw_[trans(i)] + 82.8546329;
//        b_[i] = 1.1643836*y_raw_[i] + 2.1124018*u_raw_[trans(i)] -         0*v_raw_[trans(i)] - 289.0175656;
//    }
//}
// Assist function for YUV to RGB image creation
// allocate the memory buffers
// and read raw yuv from the ifstream into the buffers
//
void YuvImage::load_from_stream(std::ifstream &yuv_strm) {
    auto ysize = total_pixels;
    auto uv_size = ysize >> 2;

    y_raw_ = new uint8_t [ysize];
    u_raw_ = new uint8_t [uv_size];
    v_raw_ = new uint8_t [uv_size];

    yuv_strm.read(reinterpret_cast<char*>(y_raw_), ysize);
    yuv_strm.read(reinterpret_cast<char*>(u_raw_), uv_size);
    yuv_strm.read(reinterpret_cast<char*>(v_raw_), uv_size);

}

// Mesure du temps
void YuvImage::time(std::string s) {
    size_t i=0;
//    auto time_start = std::chrono::high_resolution_clock::now();
//    //12 fois
//  //  for (int j=0; i<12; i++) {
//        YuvImage::yuv_to_rgb(i);
//  //  }
//    auto time_end = std::chrono::high_resolution_clock::now();

//    auto elapsed_time_us = std::chrono::duration_cast<std::chrono::microseconds> (time_end - time_start);
//    long long int elapsed_time_us_eff = elapsed_time_us.count();
//    std::string s = std::to_string(elapsed_time_us_eff);

    //std::cout << "INFO: image calculed in " << s << std::endl;
    // Séparer le temps par virgule
    uint id = 0;
    const auto end_ = s.end();
    const auto size_ = s.size(); // pour éviter le changement de pointeur
    for (id = 3; id < size_; id += 3) {
        s.insert (end_ - id, ',');
    }

    //Imprimer sur le terminal
    std::cout << "INFO: image calculed in " << s << " us" << std::endl;
}

void YuvImage::testsame(){
    yuv_to_rgb(0);
    double *test_r = new double[total_pixels];
    double *test_g = new double[total_pixels];
    double *test_b = new double[total_pixels];
    for(int i=0;i<total_pixels;++i){
        test_r[i] = r_[i];
        test_g[i] = g_[i];
        test_b[i] = b_[i];
    }


}
//! \brief
//! Constructor for the YuvImage
//! Note that the default QImage
//! will not be used except if a throw.
//! We shall use QImage::swap at the end
//
static const int default_width = 400;
static const int default_height = 400;

YuvImage::YuvImage(const std::string &file_name) :
  QImage(default_width, default_height, QImage::Format_RGB32),
  width_{default_width},
  height_{default_height} {

  static GSE4::Clamp<double> clamp_to_rgb(0., 255.);

  // we find the width and height of the file
  // based on the size of the file

  // student version
  // all try catch block removed
  std::ifstream yuv_strm(file_name, std::ios::in | std::ios::binary);

  // position stream pointer at the end to read the file size...
  yuv_strm.seekg(0, std::ios::end);
  auto filesize =  yuv_strm.tellg();

  // .. and don't forget to put it back to the beg so that we can read
  yuv_strm.seekg(0, std::ios::beg);

  // a few lines of code needed here
  // to compute  width and height from the size of the file

  auto ysize = (filesize * 2) / 3;
  if (((ysize * 3) / 2) != filesize && (((ysize * 3) / 2) % 6) == 0) {
    // more code is needed here ?????????????????????
    throw wrong_size();
    qDebug("Wrong size");
  }

    switch(ysize) {
     case(28000*4762): width_ = 28000; height_ = 4762; break;
     case(10000*4762): width_ = 10000; height_ = 4762; break;
     case(3840*2160) : width_ =  3840; height_ = 2160; break;
     case(1920*1080) : width_ =  1920; height_ = 1080; break;
     case(1024*768)  : width_ =  1024; height_ =  768; break;
     case(832*480)   : width_ =   832; height_ =  480; break;
     case(352*288)   : width_ =   352; height_ =  288; break;
     default: throw wrong_size();
   }
    total_pixels=width_*height_;
    r_ = new double [total_pixels];
    g_ = new double [total_pixels];
    b_ = new double [total_pixels];

    auto time_start = std::chrono::high_resolution_clock::now();

    nb_threads = ImageParameters::instance().get_nb_threads();
    std::vector<std::thread> threads;
    YuvImage::load_from_stream(yuv_strm);

    size_t i=0;
    //Launch a foup of threads

    for (i = 1; i < nb_threads; ++i) {
        threads.emplace_back([=]() {yuv_to_rgb(i);});
        std::cout<<"i = " << i <<std::endl;
    }
    std::cout<<nb_threads<<std::endl;
    yuv_to_rgb(0);
    for (auto &thread_elem : threads) {
      thread_elem.join();
    }




    auto time_end = std::chrono::high_resolution_clock::now();
    auto elapsed_time_us = std::chrono::duration_cast<std::chrono::microseconds> (time_end - time_start);
    long long int elapsed_time_us_eff = elapsed_time_us.count();
    std::string s = std::to_string(elapsed_time_us_eff);
    YuvImage::time(s);
  // create local image of given size
  // and swap with current image of the class
  QImage main_image(width_, height_, QImage::Format_RGB32);
  swap(main_image);

  for (int yp = 0; yp < height_; ++yp) {
    for (int xp = 0; xp < width_; ++xp) {
      setPixel(xp, yp, qRgb(clamp_to_rgb(r_[xp+yp*width_]), clamp_to_rgb(g_[xp+yp*width_]), clamp_to_rgb(b_[xp+yp*width_])));
      }
  }

//  std::cout<<"testt[]= :"<<testt[0]<<std::endl;
//  std::cout<<"testt[]= :"<<testt[1]<<std::endl;
//  std::cout<<"testt[]= :"<<testt[2]<<std::endl;
//  std::cout<<"testt[]= :"<<testt[3]<<std::endl;
  // les destructeurs

  delete [] y_raw_;
  delete [] u_raw_;
  delete [] v_raw_;

  delete [] r_;
  delete [] g_;
  delete [] b_;

}

