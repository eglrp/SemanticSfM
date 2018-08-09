
// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef I23DSFM_FEATURES_IMAGE_DESCRIBER_HPP
#define I23DSFM_FEATURES_IMAGE_DESCRIBER_HPP

#include "i23dSFM/numeric/numeric.h"
#include "i23dSFM/features/regions.hpp"
#include "i23dSFM/image/image_container.hpp"
#include <memory>
#include <cereal/cereal.hpp> // Serialization

namespace i23dSFM {
namespace features {

enum EDESCRIBER_PRESET
{
  NORMAL_PRESET,
  HIGH_PRESET,
  ULTRA_PRESET
};
/// A pure virtual class for image description computation
class Image_describer
{
public:
  Image_describer() {}
  virtual ~Image_describer() {}

  /**
  @brief Use a preset to control the number of detected regions
  @param preset The preset configuration
  @return True if configuration succeed.
  */
  virtual bool Set_configuration_preset(EDESCRIBER_PRESET preset) = 0;

  /**
  @brief Detect regions on the image and compute their attributes (description)
  @param image Image.
  @param regions The detected regions and attributes
  @param mask 8-bit gray image for keypoint filtering (optional).
     Non-zero values depict the region of interest.
  */
  virtual bool Describe( const image::Image<unsigned char> & image,
                                      std::unique_ptr<Regions> &regions,
                                      const image::Image<unsigned char> * mask = NULL) = 0;

  virtual bool Describe( const image::Image<unsigned char> & image,
                                      const image::Image<unsigned char> & semantic_image,
                                      std::unique_ptr<Regions> &regions,
                                      const image::Image<unsigned char> * mask = NULL) = 0;

  /// Allocate regions depending of the Image_describer
  virtual void Allocate(std::unique_ptr<Regions> &regions) const = 0;


  // Load semantic label of features
  // virtual bool LoadSemanticLabel(Regions * regions, const std::string& semantic_img_path)
  // {
  //   return regions->LoadSemanticLabel(semantic_img_path);
  // }


  //--
  // IO - one file for region features, one file for region descriptors
  //--

  virtual bool Load(Regions * regions,
    const std::string& sfileNameFeats,
    const std::string& sfileNameDescs) const
  {
    return regions->Load(sfileNameFeats, sfileNameDescs);
  }

  virtual bool Save(const Regions * regions,
    const std::string& sfileNameFeats,
    const std::string& sfileNameDescs) const
  {
    return regions->Save(sfileNameFeats, sfileNameDescs);
  };

  virtual bool LoadFeatures(Regions * regions,
    const std::string& sfileNameFeats) const
  {
    return regions->LoadFeatures(sfileNameFeats);
  }
};

} // namespace features
} // namespace i23dSFM

#endif // I23DSFM_FEATURES_IMAGE_DESCRIBER_HPP
