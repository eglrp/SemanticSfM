
// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "i23dSFM/matching_image_collection/Cascade_Hashing_Matcher_Regions_AllInMemory.hpp"
#include "i23dSFM/matching/matcher_cascade_hashing.hpp"
#include "i23dSFM/matching/indMatchDecoratorXY.hpp"
#include "i23dSFM/matching/matching_filters.hpp"

#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "third_party/progress/progress.hpp"


// #define USE_SEMANTIC_LABEL

namespace i23dSFM {
namespace matching_image_collection {

using namespace i23dSFM::matching;
using namespace i23dSFM::features;

Cascade_Hashing_Matcher_Regions_AllInMemory
::Cascade_Hashing_Matcher_Regions_AllInMemory
(
  float distRatio
):Matcher(), f_dist_ratio_(distRatio)
{
}

namespace impl
{
template <typename ScalarT>
void Match
(
  const sfm::SfM_Data & sfm_data,
  const sfm::Regions_Provider & regions_provider,
  const Pair_Set & pairs,
  float fDistRatio,
  PairWiseMatches & map_PutativesMatches // the pairwise photometric corresponding points
)
{
  C_Progress_display my_progress_bar( pairs.size() );

  // Collect used view indexes
  std::set<IndexT> used_index;
  // Sort pairs according the first index to minimize later memory swapping
  typedef std::map<IndexT, std::vector<IndexT> > Map_vectorT;
  Map_vectorT map_Pairs;
  for (Pair_Set::const_iterator iter = pairs.begin(); iter != pairs.end(); ++iter)
  {
    map_Pairs[iter->first].push_back(iter->second);
    used_index.insert(iter->first);
    used_index.insert(iter->second);
  }

  typedef Eigen::Matrix<ScalarT, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> BaseMat;

  // Init the cascade hasher
  CascadeHasher cascade_hasher;
  if (!used_index.empty())
  {
    const IndexT I = *used_index.begin();
    const features::Regions &regionsI = *regions_provider.regions_per_view.at(I).get();
    const size_t dimension = regionsI.DescriptorLength();
    cascade_hasher.Init(dimension);
  }

  std::map<IndexT, HashedDescriptions> hashed_base_;

  // Compute the zero mean descriptor that will be used for hashing (one for all the image regions)
  Eigen::VectorXf zero_mean_descriptor;
  {
    Eigen::MatrixXf matForZeroMean;
    for (int i =0; i < used_index.size(); ++i)
    {
      std::set<IndexT>::const_iterator iter = used_index.begin();
      std::advance(iter, i);
      const IndexT I = *iter;
      const features::Regions &regionsI = *regions_provider.regions_per_view.at(I).get();
      const ScalarT * tabI =
        reinterpret_cast<const ScalarT*>(regionsI.DescriptorRawData());
      const size_t dimension = regionsI.DescriptorLength();
      if (i==0)
      {
        matForZeroMean.resize(used_index.size(), dimension);
        matForZeroMean.fill(0.0f);
      }
      if (regionsI.RegionCount() > 0)
      {
        Eigen::Map<BaseMat> mat_I( (ScalarT*)tabI, regionsI.RegionCount(), dimension);
        matForZeroMean.row(i) = CascadeHasher::GetZeroMeanDescriptor(mat_I);
      }
    }
    zero_mean_descriptor = CascadeHasher::GetZeroMeanDescriptor(matForZeroMean);
  }

  // Index the input regions
  #ifdef I23DSFM_USE_OPENMP
    #pragma omp parallel for schedule(dynamic)
  #endif
  for (int i =0; i < used_index.size(); ++i)
  {
    std::set<IndexT>::const_iterator iter = used_index.begin();
    std::advance(iter, i);
    const IndexT I = *iter;
    const features::Regions &regionsI = *regions_provider.regions_per_view.at(I).get();
    const ScalarT * tabI =
      reinterpret_cast<const ScalarT*>(regionsI.DescriptorRawData());
    const size_t dimension = regionsI.DescriptorLength();

    Eigen::Map<BaseMat> mat_I( (ScalarT*)tabI, regionsI.RegionCount(), dimension);
    const HashedDescriptions hashed_description = cascade_hasher.CreateHashedDescriptions(mat_I,
      zero_mean_descriptor);
    #ifdef I23DSFM_USE_OPENMP
        #pragma omp critical
    #endif
    {
      hashed_base_[I] = std::move(hashed_description);
    }
  }

  // Perform matching between all the pairs
  for (Map_vectorT::const_iterator iter = map_Pairs.begin(); iter != map_Pairs.end(); ++iter)
  {
    const IndexT I = iter->first;
    const std::vector<IndexT> & indexToCompare = iter->second;

    const features::Regions &regionsI = *regions_provider.regions_per_view.at(I).get();
    if (regionsI.RegionCount() == 0)
    {
      my_progress_bar += indexToCompare.size();
      continue;
    }

    const std::vector<features::PointFeature> pointFeaturesI = regionsI.GetRegionsPositions();
    const ScalarT * tabI = reinterpret_cast<const ScalarT*>(regionsI.DescriptorRawData());
    const size_t dimension = regionsI.DescriptorLength();
    Eigen::Map<BaseMat> mat_I( (ScalarT*)tabI, regionsI.RegionCount(), dimension);

    // #ifdef USE_SEMANTIC_LABEL
    // // Group descriptor by semantic label of corresponding features
    // vector<ScalarT> tabI0, tabI1, tabI2;
    // for(int i = 0; i < pointFeaturesI.size(); i++)
    // {
    //   if(pointFeaturesI[i].semanticLabel() == 0) tabI0.push_back(tabI[i]);
    //   else if(pointFeaturesI[i].semanticLabel() == 1) tabI1.push_back(tabI[i]);
    //   else tabI2.push_back(tabI[i]);
    // }

    // // cout << "mapping to left image: " << endl;

    // Eigen::Map<BaseMat> mat_I0(&tabI0[0], tabI0.size(), dimension);
    // Eigen::Map<BaseMat> mat_I1(&tabI1[0], tabI1.size(), dimension);
    // Eigen::Map<BaseMat> mat_I2(&tabI2[0], tabI2.size(), dimension);
    
    // // cout << "ending of map to left image" << endl;
    // #endif

    #ifdef I23DSFM_USE_OPENMP
        #pragma omp parallel for schedule(dynamic)
    #endif
    for (int j = 0; j < (int)indexToCompare.size(); ++j)
    {
      const size_t J = indexToCompare[j];
      const features::Regions &regionsJ = *regions_provider.regions_per_view.at(J).get();

      if (regions_provider.regions_per_view.count(J) == 0
          || regionsI.Type_id() != regionsJ.Type_id())
      {
        #ifdef I23DSFM_USE_OPENMP
                #pragma omp critical
        #endif
        ++my_progress_bar;
        continue;
      }

      // Matrix representation of the query input data;
      const std::vector<features::PointFeature> pointFeaturesJ = regionsJ.GetRegionsPositions();      
      const ScalarT * tabJ = reinterpret_cast<const ScalarT*>(regionsJ.DescriptorRawData());    
      Eigen::Map<BaseMat> mat_J( (ScalarT*)tabJ, regionsJ.RegionCount(), dimension);

      //  #ifdef USE_SEMANTIC_LABEL
      // // Group descriptor by semantic label of corresponding features
      // vector<ScalarT> tabJ0, tabJ1, tabJ2;
      // for(int i = 0; i < pointFeaturesJ.size(); i++)
      // {
      //   if(pointFeaturesJ[i].semanticLabel() == 0) tabJ0.push_back(tabJ[i]);
      //   else if(pointFeaturesJ[i].semanticLabel() == 1) tabJ1.push_back(tabJ[i]);
      //   else tabJ2.push_back(tabJ[i]);
      // }

      // // cout << "mapping to right image" << endl;

      // Eigen::Map<BaseMat> mat_J0(&tabJ0[0], tabJ0.size(), dimension);
      // Eigen::Map<BaseMat> mat_J1(&tabJ1[0], tabJ1.size(), dimension);
      // Eigen::Map<BaseMat> mat_J2(&tabJ2[0], tabJ2.size(), dimension);

      // // cout << "end of map to right image" << endl;
      // #endif


      typedef typename Accumulator<ScalarT>::Type ResultType;      
      matching::IndMatches vec_putative_matches;

// #ifdef USE_SEMANTIC_LABEL
//       IndMatches pvec_indices0, pvec_indices1, pvec_indices2;
//       std::vector<ResultType> pvec_distances0, pvec_distances1, pvec_distances2;
//       pvec_distances0.reserve(tabJ0.size() * 2);
//       pvec_distances1.reserve(tabJ1.size() * 2);
//       pvec_distances2.reserve(tabJ2.size() * 2);
//       pvec_indices0.reserve(tabJ0.size() * 2);
//       pvec_indices1.reserve(tabJ1.size() * 2);
//       pvec_indices2.reserve(tabJ2.size() * 2);
      
//       cout << "begin semantic cascade hashing matching" << endl;
//       // Match the query descriptors to the database
//       cascade_hasher.Match_HashedDescriptions<BaseMat, ResultType>(
//         hashed_base_[J], mat_J0, hashed_base_[I], mat_I0, &pvec_indices0, &pvec_distances0);
//       cascade_hasher.Match_HashedDescriptions<BaseMat, ResultType>(
//         hashed_base_[J], mat_J1, hashed_base_[I], mat_I1, &pvec_indices1, &pvec_distances1);
//       cascade_hasher.Match_HashedDescriptions<BaseMat, ResultType>(
//         hashed_base_[J], mat_J2, hashed_base_[I], mat_I2, &pvec_indices2, &pvec_distances2);
//       cout << "end of semantic cascade hashing matching" << endl;

//       std::vector<int> vec_nn_ratio_idx0, vec_nn_ratio_idx1, vec_nn_ratio_idx2;
//       // Filter the matches using a distance ratio test:
//       //   The probability that a match is correct is determined by taking
//       //   the ratio of distance from the closest neighbor to the distance
//       //   of the second closest.
//       matching::NNdistanceRatio(pvec_distances0.begin(), pvec_distances0.end(), 2, vec_nn_ratio_idx0, Square(fDistRatio));
//       matching::NNdistanceRatio(pvec_distances1.begin(), pvec_distances1.end(), 2, vec_nn_ratio_idx1, Square(fDistRatio));
//       matching::NNdistanceRatio(pvec_distances2.begin(), pvec_distances2.end(), 2, vec_nn_ratio_idx2, Square(fDistRatio));      

//       vec_putative_matches.reserve(vec_nn_ratio_idx0.size() + vec_nn_ratio_idx1.size() + vec_nn_ratio_idx2.size());
//       for (size_t k=0; k < vec_nn_ratio_idx0.size(); ++k)
//       {
//         const size_t index = vec_nn_ratio_idx0[k];
//         vec_putative_matches.emplace_back(pvec_indices0[index*2]._j, pvec_indices0[index*2]._i);
//       }
//       for (size_t k=0; k < vec_nn_ratio_idx1.size(); ++k)
//       {
//         const size_t index = vec_nn_ratio_idx1[k];
//         vec_putative_matches.emplace_back(pvec_indices1[index*2]._j, pvec_indices1[index*2]._i);
//       }      
//       for (size_t k=0; k < vec_nn_ratio_idx2.size(); ++k)
//       {
//         const size_t index = vec_nn_ratio_idx2[k];
//         vec_putative_matches.emplace_back(pvec_indices2[index*2]._j, pvec_indices2[index*2]._i);
//       }
// #else
      IndMatches pvec_indices;
      std::vector<ResultType> pvec_distances;
      pvec_distances.reserve(regionsJ.RegionCount() * 2);
      pvec_indices.reserve(regionsJ.RegionCount() * 2);

      cascade_hasher.Match_HashedDescriptions<BaseMat, ResultType>(
        hashed_base_[J], mat_J,
        hashed_base_[I], mat_I,
        &pvec_indices, &pvec_distances);

      std::vector<int> vec_nn_ratio_idx;
      // Filter the matches using a distance ratio test:
      //   The probability that a match is correct is determined by taking
      //   the ratio of distance from the closest neighbor to the distance
      //   of the second closest.
      matching::NNdistanceRatio(
        pvec_distances.begin(), // distance start
        pvec_distances.end(),   // distance end
        2, // Number of neighbor in iterator sequence (minimum required 2)
        vec_nn_ratio_idx, // output (indices that respect the distance Ratio)
        Square(fDistRatio));

      // matching::IndMatches vec_putative_matches;
      vec_putative_matches.reserve(vec_nn_ratio_idx.size());
      for (size_t k=0; k < vec_nn_ratio_idx.size(); ++k)
      {
        const size_t index = vec_nn_ratio_idx[k];
        vec_putative_matches.emplace_back(pvec_indices[index*2]._j, pvec_indices[index*2]._i);
      }
// #endif

      // Remove duplicates
      matching::IndMatch::getDeduplicated(vec_putative_matches);

      // Remove matches that have the same (X,Y) coordinates
      matching::IndMatchDecorator<float> matchDeduplicator(vec_putative_matches, pointFeaturesI, pointFeaturesJ);
      matchDeduplicator.getDeduplicated(vec_putative_matches);

#ifdef I23DSFM_USE_OPENMP
#pragma omp critical
#endif
      {
        ++my_progress_bar;
        if (!vec_putative_matches.empty())
        {
          map_PutativesMatches.insert( make_pair( make_pair(I,J), std::move(vec_putative_matches) ));
        }
      }
    }
  }
}
} // namespace impl


void Cascade_Hashing_Matcher_Regions_AllInMemory::Match
(
  const sfm::SfM_Data & sfm_data,
  const std::shared_ptr<sfm::Regions_Provider> & regions_provider,
  const Pair_Set & pairs,
  PairWiseMatches & map_PutativesMatches // the pairwise photometric corresponding points
)const
{
#ifdef I23DSFM_USE_OPENMP
  std::cout << "Using the OPENMP thread interface" << std::endl;
#endif

  if (regions_provider->regions_per_view.empty())
    return;

  const features::Regions &regions = *regions_provider->regions_per_view.begin()->second.get();

  if (regions.IsBinary())
    return;

  if(regions.Type_id() == typeid(unsigned char).name())
  {
    impl::Match<unsigned char>(
      sfm_data,
      *regions_provider.get(),
      pairs,
      f_dist_ratio_,
      map_PutativesMatches);
  }
  else
  if(regions.Type_id() == typeid(float).name())
  {
    impl::Match<float>(
      sfm_data,
      *regions_provider.get(),
      pairs,
      f_dist_ratio_,
      map_PutativesMatches);
  }
  else
  {
    std::cerr << "Matcher not implemented for this region type" << std::endl;
  }
}

} // namespace i23dSFM
} // namespace matching_image_collection
