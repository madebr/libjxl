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

#include <string>

#include "gtest/gtest.h"
#include "jxl/aux_out.h"
#include "jxl/base/data_parallel.h"
#include "jxl/base/padded_bytes.h"
#include "jxl/base/thread_pool_internal.h"
#include "jxl/codec_in_out.h"
#include "jxl/dec_file.h"
#include "jxl/dec_params.h"
#include "jxl/enc_butteraugli_comparator.h"
#include "jxl/enc_cache.h"
#include "jxl/enc_file.h"
#include "jxl/enc_params.h"
#include "jxl/extras/codec.h"
#include "jxl/image.h"
#include "jxl/image_test_utils.h"
#include "jxl/test_utils.h"
#include "jxl/testdata_path.h"

namespace jxl {
namespace {

struct SpeedTierTestParams {
  explicit SpeedTierTestParams(const SpeedTier speed_tier,
                               const bool shrink8 = false)
      : speed_tier(speed_tier), shrink8(shrink8) {}
  SpeedTier speed_tier;
  bool shrink8;
};

std::ostream& operator<<(std::ostream& os, SpeedTierTestParams params) {
  auto previous_flags = os.flags();
  os << std::boolalpha;
  os << "SpeedTierTestParams{" << SpeedTierName(params.speed_tier)
     << ", /*shrink8=*/" << params.shrink8 << "}";
  os.flags(previous_flags);
  return os;
}

class SpeedTierTest : public testing::TestWithParam<SpeedTierTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    SpeedTierTestInstantiation, SpeedTierTest,
    testing::Values(SpeedTierTestParams{SpeedTier::kCheetah,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kCheetah,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kFalcon,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kFalcon,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kHare,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kHare,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kWombat,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kWombat,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kSquirrel,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kSquirrel,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kKitten,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kKitten,
                                        /*shrink8=*/false},
                    // Only downscaled image for Tortoise mode.
                    SpeedTierTestParams{SpeedTier::kTortoise,
                                        /*shrink8=*/true}));

TEST_P(SpeedTierTest, Roundtrip) {
  const std::string pathname =
      GetTestDataPath("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ThreadPoolInternal pool(8);
  ASSERT_TRUE(SetFromFile(pathname, &io, &pool));

  const SpeedTierTestParams& params = GetParam();

  if (params.shrink8) {
    io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);
  }

  CompressParams cparams;
  cparams.speed_tier = params.speed_tier;
  DecompressParams dparams;

  CodecInOut io2;
  test::Roundtrip(&io, cparams, dparams, nullptr, &io2);

  EXPECT_LE(ButteraugliDistance(io, io2, cparams.hf_asymmetry,
                                /*distmap=*/nullptr, /*pool=*/nullptr),
            2);
}
}  // namespace
}  // namespace jxl