// Copyright (c) 2012 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef MIMATTE_LINEAR_PROGRAMMING_INTERFACE_MOSEK_H_
#define MIMATTE_LINEAR_PROGRAMMING_INTERFACE_MOSEK_H_

#ifdef I23DSFM_HAVE_MOSEK

#include "i23dSFM/numeric/numeric.h"
#include "i23dSFM/linearProgramming/linearProgrammingInterface.hpp"
extern "C"{
#include "mosek.h"
}
#include <vector>

namespace i23dSFM   {
namespace linearProgramming  {

/// MOSEK wrapper for the LP_Solver
class MOSEK_SolveWrapper : public LP_Solver
{
public :
  MOSEK_SolveWrapper(int nbParams);

  ~MOSEK_SolveWrapper();

  //--
  // Inherited functions :
  //--

  bool setup(const LP_Constraints & constraints);
  bool setup(const LP_Constraints_Sparse & constraints);

  bool solve();

  bool getSolution(std::vector<double> & estimatedParams);

private :
  //MSKenv_t     env;
  MSKtask_t    task; // Solver object.
};


} // namespace linearProgramming
} // namespace i23dSFM


#endif // MIMATTE_LINEAR_PROGRAMMING_INTERFACE_MOSEK_H_

#endif // I23DSFM_HAVE_MOSEK
