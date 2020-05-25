// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Jyrki Alakuijala (jyrki.alakuijala@gmail.com)

#ifndef JXL_BUTTERAUGLI_BUTTERAUGLI_H_
#define JXL_BUTTERAUGLI_BUTTERAUGLI_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmath>
#include <memory>
#include <vector>

#include "jxl/base/compiler_specific.h"
#include "jxl/image.h"
#include "jxl/image_ops.h"

#define BUTTERAUGLI_ENABLE_CHECKS 0
#define BUTTERAUGLI_RESTRICT JXL_RESTRICT

// This is the main interface to butteraugli image similarity
// analysis function.

namespace jxl {

// ButteraugliInterface defines the public interface for butteraugli.
//
// It calculates the difference between rgb0 and rgb1.
//
// rgb0 and rgb1 contain the images. rgb0[c][px] and rgb1[c][px] contains
// the red image for c == 0, green for c == 1, blue for c == 2. Location index
// px is calculated as y * xsize + x.
//
// Value of pixels of images rgb0 and rgb1 need to be represented as raw
// intensity. Most image formats store gamma corrected intensity in pixel
// values. This gamma correction has to be removed, by applying the following
// function:
// butteraugli_val = 255.0 * pow(png_val / 255.0, gamma);
// A typical value of gamma is 2.2. It is usually stored in the image header.
// Take care not to confuse that value with its inverse. The gamma value should
// be always greater than one.
// Butteraugli does not work as intended if the caller does not perform
// gamma correction.
//
// hf_asymmetry is a multiplier for penalizing new HF artifacts more than
// blurring away features (1.0 -> neutral).
//
// diffmap will contain an image of the size xsize * ysize, containing
// localized differences for values px (indexed with the px the same as rgb0
// and rgb1). diffvalue will give a global score of similarity.
//
// A diffvalue smaller than kButteraugliGood indicates that images can be
// observed as the same image.
// diffvalue larger than kButteraugliBad indicates that a difference between
// the images can be observed.
// A diffvalue between kButteraugliGood and kButteraugliBad indicates that
// a subtle difference can be observed between the images.
//
// Returns true on success.

bool ButteraugliInterface(const Image3F &rgb0, const Image3F &rgb1,
                          float hf_asymmetry, ImageF &diffmap,
                          double &diffvalue);

// Converts the butteraugli score into fuzzy class values that are continuous
// at the class boundary. The class boundary location is based on human
// raters, but the slope is arbitrary. Particularly, it does not reflect
// the expectation value of probabilities of the human raters. It is just
// expected that a smoother class boundary will allow for higher-level
// optimization algorithms to work faster.
//
// Returns 2.0 for a perfect match, and 1.0 for 'ok', 0.0 for bad. Because the
// scoring is fuzzy, a butteraugli score of 0.96 would return a class of
// around 1.9.
double ButteraugliFuzzyClass(double score);

// Input values should be in range 0 (bad) to 2 (good). Use
// kButteraugliNormalization as normalization.
double ButteraugliFuzzyInverse(double seek);

// Implementation details, don't use anything below or your code will
// break in the future.

#ifdef _MSC_VER
#define BUTTERAUGLI_INLINE __forceinline
#else
#define BUTTERAUGLI_INLINE inline
#endif

#ifdef __clang__
// Early versions of Clang did not support __builtin_assume_aligned.
#define BUTTERAUGLI_HAS_ASSUME_ALIGNED __has_builtin(__builtin_assume_aligned)
#elif defined(__GNUC__)
#define BUTTERAUGLI_HAS_ASSUME_ALIGNED 1
#else
#define BUTTERAUGLI_HAS_ASSUME_ALIGNED 0
#endif

// Returns a void* pointer which the compiler then assumes is N-byte aligned.
// Example: float* JXL_RESTRICT aligned = (float*)JXL_ASSUME_ALIGNED(in, 32);
//
// The assignment semantics are required by GCC/Clang. ICC provides an in-place
// __assume_aligned, whereas MSVC's __assume appears unsuitable.
#if BUTTERAUGLI_HAS_ASSUME_ALIGNED
#define BUTTERAUGLI_ASSUME_ALIGNED(ptr, align) \
  __builtin_assume_aligned((ptr), (align))
#else
#define BUTTERAUGLI_ASSUME_ALIGNED(ptr, align) (ptr)
#endif  // BUTTERAUGLI_HAS_ASSUME_ALIGNED

struct MaskImage {
  MaskImage() {}
  MaskImage(int xs, int ys) : mask_x(xs, ys), mask_yb(xs, ys) {}
  ImageF mask_x;
  ImageF mask_yb;
};

struct PsychoImage {
  ImageF uhf[2];  // XY
  ImageF hf[2];   // XY
  Image3F mf;     // XYB
  Image3F lf;     // XYB
};

class ButteraugliComparator {
 public:
  ButteraugliComparator(const Image3F &rgb0, double hf_asymmetry);
  virtual ~ButteraugliComparator();

  // Computes the butteraugli map between the original image given in the
  // constructor and the distorted image give here.
  void Diffmap(const Image3F &rgb1, ImageF &result) const;

  // Same as above, but OpsinDynamicsImage() was already applied.
  void DiffmapOpsinDynamicsImage(const Image3F &xyb1, ImageF &result) const;

  // Same as above, but the frequency decomposition was already applied.
  void DiffmapPsychoImage(const PsychoImage &ps1, ImageF &diffmap) const;

  void Mask(MaskImage *BUTTERAUGLI_RESTRICT mask,
            MaskImage *BUTTERAUGLI_RESTRICT mask_dc) const;

 private:
  const size_t xsize_;
  const size_t ysize_;
  float hf_asymmetry_;
  PsychoImage pi0_;
  ButteraugliComparator *sub_;
};

bool ButteraugliDiffmap(const Image3F &rgb0, const Image3F &rgb1,
                        double hf_asymmetry, ImageF &diffmap);

double ButteraugliScoreFromDiffmap(const ImageF &diffmap);

// Generate rgb-representation of the distance between two images.
Image3B CreateHeatMapImage(const ImageF &distmap, double good_threshold,
                           double bad_threshold);

}  // namespace jxl

#endif  // JXL_BUTTERAUGLI_BUTTERAUGLI_H_