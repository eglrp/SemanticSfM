
// Copyright (c) 2014 i23dSFM authors.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef I23DSFM_COLORHARMONIZATION_FULLFRAME_H
#define I23DSFM_COLORHARMONIZATION_FULLFRAME_H

#include "i23dSFM/color_harmonization/selection_interface.hpp"

namespace i23dSFM {
namespace color_harmonization {

class commonDataByPair_FullFrame  : public commonDataByPair
{
public:
  commonDataByPair_FullFrame( const std::string & sLeftImage,
                              const std::string & sRightImage ):
        commonDataByPair( sLeftImage, sRightImage )
  {}

  virtual ~commonDataByPair_FullFrame() {}

  /**
   * Put masks to white, all image is considered as valid pixel selection
   *
   * \param[out] maskLeft Mask of the left image (initialized to corresponding image size).
   * \param[out] maskRight  Mask of the right image (initialized to corresponding image size).
   *
   * \return True.
   */
  virtual bool computeMask( image::Image< unsigned char > & maskLeft, image::Image< unsigned char > & maskRight )
  {
    maskLeft.fill( image::WHITE );
    maskRight.fill( image::WHITE );
    return true;
  }

private:

};

}  // namespace color_harmonization
}  // namespace i23dSFM

#endif  // I23DSFM_COLORHARMONIZATION_FULLFRAME_H
