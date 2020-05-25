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

#include <stdint.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "jxl/base/data_parallel.h"
#include "jxl/base/file_io.h"
#include "jxl/base/padded_bytes.h"
#include "jxl/base/status.h"
#include "jxl/base/thread_pool_internal.h"
#include "jxl/butteraugli/butteraugli.h"
#include "jxl/codec_in_out.h"
#include "jxl/color_encoding.h"
#include "jxl/color_management.h"
#include "jxl/enc_butteraugli_comparator.h"
#include "jxl/extras/codec.h"
#include "jxl/extras/codec_png.h"
#include "jxl/image.h"
#include "jxl/image_bundle.h"
#include "jxl/image_ops.h"
#include "tools/butteraugli_pnorm.h"

namespace jxl {
namespace {

Status WritePNG(const Image3B& image, const std::string& filename) {
  ThreadPoolInternal pool(4);
  std::vector<uint8_t> rgb(image.xsize() * image.ysize() * 3);
  CodecInOut io;
  io.metadata.bits_per_sample = 8;
  io.metadata.floating_point_sample = false;
  io.metadata.color_encoding = ColorEncoding::SRGB();
  io.SetFromImage(StaticCastImage3<float>(image), io.metadata.color_encoding);
  PaddedBytes compressed;
  JXL_CHECK(EncodeImagePNG(&io, io.Main().c_current(), 8, &pool, &compressed));
  return WriteFile(compressed, filename);
}

Status RunButteraugli(const char* pathname1, const char* pathname2,
                      const std::string& distmap_filename,
                      const std::string& colorspace_hint) {
  CodecInOut io1;
  if (!colorspace_hint.empty()) {
    io1.dec_hints.Add("color_space", colorspace_hint);
  }
  ThreadPoolInternal pool(4);
  if (!SetFromFile(pathname1, &io1, &pool)) {
    fprintf(stderr, "Failed to read image from %s\n", pathname1);
    return false;
  }

  CodecInOut io2;
  if (!colorspace_hint.empty()) {
    io2.dec_hints.Add("color_space", colorspace_hint);
  }
  if (!SetFromFile(pathname2, &io2, &pool)) {
    fprintf(stderr, "Failed to read image from %s\n", pathname2);
    return false;
  }

  if (io1.xsize() != io2.xsize()) {
    fprintf(stderr, "Width mismatch: %zu %zu\n", io1.xsize(), io2.xsize());
    return false;
  }
  if (io1.ysize() != io2.ysize()) {
    fprintf(stderr, "Height mismatch: %zu %zu\n", io1.ysize(), io2.ysize());
    return false;
  }

  ImageF distmap;
  const float kHfAsymmetry = 0.8;
  const float distance = ButteraugliDistance(io1.Main(), io2.Main(),
                                             kHfAsymmetry, &distmap, &pool);
  printf("%.10f\n", distance);

  double p = 3.0;
  double pnorm = ChooseComputeDistanceP()(distmap, p);
  printf("%g-norm: %f\n", p, pnorm);

  if (!distmap_filename.empty()) {
    float good = ButteraugliFuzzyInverse(1.5);
    float bad = ButteraugliFuzzyInverse(0.5);
    Image3B heatmap = CreateHeatMapImage(distmap, good, bad);
    JXL_CHECK(WritePNG(heatmap, distmap_filename));
  }
  return true;
}

}  // namespace
}  // namespace jxl

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr,
            "Usage: %s <reference> <distorted> [--distmap <distmap>] "
            "[--colorspace <colorspace_hint>]\n"
            "NOTE: images get converted to linear sRGB for butteraugli. Images"
            " without attached profiles (such as ppm or pfm) are interpreted"
            " as nonlinear sRGB. The hint format is RGB_D65_SRG_Rel_Lin for"
            " linear sRGB\n",
            argv[0]);
    return 1;
  }
  std::string distmap;
  std::string colorspace;
  for (int i = 2; i < argc; i++) {
    if (std::string(argv[i]) == "--distmap" && i + 1 < argc)
      distmap = argv[++i];
    if (std::string(argv[i]) == "--colorspace" && i + 1 < argc)
      colorspace = argv[++i];
  }

  return jxl::RunButteraugli(argv[1], argv[2], distmap, colorspace) ? 0 : 1;
}