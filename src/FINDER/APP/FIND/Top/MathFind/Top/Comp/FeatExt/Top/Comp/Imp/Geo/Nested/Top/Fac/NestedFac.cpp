/*
 * NumCompletelyNestedBlobsFeatureExtractorFactory.cpp
 *
 *  Created on: Dec 4, 2016
 *      Author: jake
 */

#include <NestedFac.h>

#include <BlobFeatExtCat.h>
#include <NestedDesc.h>
#include <NestedFeatExt.h>
#include <FinderInfo.h>

NumCompletelyNestedBlobsFeatureExtractorFactory
::NumCompletelyNestedBlobsFeatureExtractorFactory(
    BlobFeatureExtractorCategory* const category) {
  this->description =
      new NumCompletelyNestedBlobsFeatureExtractorDescription(
          category);
}

NumCompletelyNestedBlobsFeatureExtractorFactory::
~NumCompletelyNestedBlobsFeatureExtractorFactory() {
  delete description;
}

NumCompletelyNestedBlobsFeatureExtractor*
NumCompletelyNestedBlobsFeatureExtractorFactory
::create(FinderInfo* const finderInfo) {
  return new NumCompletelyNestedBlobsFeatureExtractor(
      description,
      finderInfo);
}

BlobFeatureExtractorDescription* NumCompletelyNestedBlobsFeatureExtractorFactory::getDescription() {
  return description;
}
