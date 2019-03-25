//
// header-start
//////////////////////////////////////////////////////////////////////////////////
//
// \file      yuvimage.h
//
// \brief     This file belongs to the C++ tutorial project
//
// \author    Bernard
//
// \copyright Copyright 2019
//
//////////////////////////////////////////////////////////////////////////////////
// header-log
//
#pragma once

#include <QtWidgets>
#include <cstring>
#include <string>
#include <exception>
#include <memory>

QT_BEGIN_NAMESPACE
class QImage;
QT_END_NAMESPACE


class YuvImage : public QImage {
 public:
  explicit YuvImage(const std::string &file_name);
  YuvImage(const YuvImage &image) = delete;
  void load_from_stream(std::ifstream &yuv_strm);
  void yuv_to_rgb(size_t groupe_idx);
  void SIMD_yuv_to_rgb(int begin, int end);
  int trans(int i);
  void time(std::string s);
  void testsame();

  // A nested class extending the exception class
  // we shall thow an exception if the size of the
  // file does not match some expected values
  // the usage is:  throw wrong_size();
  // which calls the constructor for this class
  // As you can see, no constructor exists so
  // wrong_size() calls the default constructor
  // which call the base class constructor std::exception();
  // which trigger the throw
  // the virtual function what() return a pointer to a const char *
  // the qualifers after the what() are
  // const    => this function does not change the class member value
  // noexcept => this function does not throw
  // override => this function is virtual and override the base class function

  class wrong_size : public std::exception {
  public:
    const char *what() const noexcept override {
      return "File has wrong size";
    }
  };

private:
  int width_;
  int height_;
  int total_pixels;
  uint8_t *y_raw_;
  uint8_t *u_raw_;
  uint8_t *v_raw_;

  size_t nb_threads;
  double *r_;
  double *g_;
  double *b_;

// student version: some private member removed

};
