
// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef I23DSFM_SFM_GLOBAL_ENGINE_PIPELINES_GLOBAL_ROTATION_AVERAGING_HPP
#define I23DSFM_SFM_GLOBAL_ENGINE_PIPELINES_GLOBAL_ROTATION_AVERAGING_HPP

namespace i23dSFM{
namespace sfm{

enum ERotationAveragingMethod
{
  ROTATION_AVERAGING_L1 = 1,
  ROTATION_AVERAGING_L2 = 2
};

enum ERelativeRotationInferenceMethod
{
  TRIPLET_ROTATION_INFERENCE_NONE = 0,
  TRIPLET_ROTATION_INFERENCE_COMPOSITION_ERROR = 1
};

} // namespace sfm
} // namespace i23dSFM

#include "i23dSFM/sfm/sfm.hpp"
#include "i23dSFM/graph/graph.hpp"
#include "i23dSFM/multiview/rotation_averaging_common.hpp"

namespace i23dSFM{
namespace sfm{

class GlobalSfM_Rotation_AveragingSolver
{
private:
  mutable Pair_Set used_pairs; // pair that are considered as valid by the rotation averaging solver

public:
  bool Run(
    ERotationAveragingMethod eRotationAveragingMethod,
    ERelativeRotationInferenceMethod eRelativeRotationInferenceMethod,
    const rotation_averaging::RelativeRotations & relativeRot_In,
    Hash_Map<IndexT, Mat3> & map_globalR
  ) const;

  /// Reject edges of the view graph that do not produce triplets with tiny
  ///  angular error once rotation composition have been computed.
  void TripletRotationRejection(
    const double max_angular_error,
    std::vector< graph::Triplet > & vec_triplets,
    rotation_averaging::RelativeRotations & relativeRotations) const;

  /// Return the pairs validated by the GlobalRotation routine (inference can remove some)
  Pair_Set GetUsedPairs() const;
};

} // namespace sfm
} // namespace i23dSFM

#endif // I23DSFM_SFM_GLOBAL_ENGINE_PIPELINES_GLOBAL_ROTATION_AVERAGING_HPP
