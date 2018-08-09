// Copyright (c) 2012 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef I23DSFM_RESECTION_L1_KERNEL_H_
#define I23DSFM_RESECTION_L1_KERNEL_H_

#include <vector>
#include "i23dSFM/multiview/projection.hpp"
#include "i23dSFM/multiview/two_view_kernel.hpp"
#include "i23dSFM/numeric/numeric.h"

namespace i23dSFM {
namespace lInfinityCV {
namespace kernel {

using namespace std;

/**
 * Six-point resection
 * P Matrix estimation (Pose estimation)
 * Rely on L1 Resection algorithm.
 * Work from 6 to N points.
 */
struct l1SixPointResectionSolver {
  enum { MINIMUM_SAMPLES = 6 };
  enum { MAX_MODELS = 1 };
  // Solve the problem of camera pose.
  // First 3d point will be translated in order to have X0 = (0,0,0,1)
  static void Solve(const Mat &pt2D, const Mat &pt3D, vector<Mat34> *P);

  // Compute the residual of the projection distance(pt2D, Project(P,pt3D))
  static double Error(const Mat34 & P, const Vec2 & pt2D, const Vec3 & pt3D)
  {
    Vec2 x = Project(P, pt3D);
    return (x-pt2D).norm();
  }
};

//-- Usable solver for the l1 6pt Resection Estimation
typedef two_view::kernel::Kernel<l1SixPointResectionSolver,
  l1SixPointResectionSolver, Mat34>  l1PoseResectionKernel;

}  // namespace kernel
}  // namespace lInfinityCV
}  // namespace i23dSFM

#endif  // I23DSFM_RESECTION_L1_KERNEL_H_
