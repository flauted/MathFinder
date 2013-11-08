/**************************************************************************
 * Project Isagoge: Enhancing the OCR Quality of Printed Scientific Documents
 * File name:   TrainerPredictor.h
 * Written by:  Jake Bruce, Copyright (C) 2013
 * History:     Created Oct 14, 2013 6:53:21 PM
 * ------------------------------------------------------------------------
 * Description: Main header for the detection component of the MEDS module.
 *              The detection component includes the feature extraction,
 *              training, and binary classification used in order to detect
 *              mathematical symbols on a page. The emphasis of this component
 *              is to get as many true positives as possible while avoiding
 *              false positives to the greatest extent possible. It is considered
 *              better to have a false negative than a false positive in
 *              general at this stage, since it is much easier to correct the
 *              latter during segmentation.
 *
 *              This module is designed to cover both the training and 
 *              prediction functionality needed in order to run experiments
 *              on different classification/feature extraction/training
 *              combinations. Compile-time polymorphism is used in this 
 *              design such that the common requirements of all such 
 *              combinations are abstracted away. This makes it relatively
 *              easy to try different combinations for experimentation
 *              purposes. 
 * ------------------------------------------------------------------------
 * This file is part of Project Isagoge.
 *
 * Project Isagoge is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Project Isagoge is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Project Isagoge.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/
#ifndef TRAINER_PREDICTOR_H
#define TRAINER_PREDICTOR_H

#include <Sample.h>
#include <GTParser.h>
using namespace GT_Parser;
#include <ITrainer.h>
#include <IBinaryClassifier.h>
#include <IFeatureExtractor.h>

template <typename TrainerType,
typename BinClassType,
typename FeatExtType>
class TrainerPredictor {
 public:
  // some shorthand to make things less messy
  typedef ITrainer<TrainerType, BinClassType, FeatExtType> I_Trainer;
  typedef IBinaryClassifier<BinClassType> IClassifier;
  typedef IFeatureExtractor<FeatExtType> IFeatExt;

  TrainerPredictor<TrainerType, BinClassType, FeatExtType>()
    : training_done(false) {}

  inline void initTrainingPaths(const string& groundtruth_path_,
      const string& training_set_path_, const string& ext_) {
    groundtruth_path = groundtruth_path_;
    training_set_path = training_set_path_;
    ext = ext_;
  }

  // Initialize the feature extractor for the full training set
  // depending on what features are being extracted, the extractor
  // may require performing some computation on the entire set or
  // a subset of the training data in order to get some metrics
  // useful in feature extraction (this of course does not apply to
  // the prediction stage, but information gained from this initialization
  // will help to make predictions after training)
  inline void initFeatExtFull(TessBaseAPI& api) {
    featext.initFeatExtFull(api, groundtruth_path, training_set_path, ext);
  }

  // When doing training it is necessary to first get all the samples
  // up front. This method does feature extraction on all the blobs in
  // the given blobinfogrid and returns the feature vector (sample)
  // corresponding to each blob. This returns a vector of BLSamples
  // (Binary Labeled Samples) which includes both the features and the
  // binary label (true for math, false for non-math)
  inline vector<BLSample*> getAllSamples(tesseract::BlobInfoGrid* grid,
      int image_index) {
    featext.initFeatExtSinglePage();
    featext.extractAllFeatures(grid);
    tesseract::BlobInfoGridSearch bigs(grid);
    tesseract::M_Utils mutils;
    bigs.StartFullSearch();
    tesseract::BLOBINFO* blob = NULL;
    vector<BLSample*> samples;
    int blobnum = 0;
    while((blob = bigs.NextFullSearch()) != NULL) {
      BLSample* lsample = new BLSample; // labeled sample
      lsample->sample = blob->features;
      lsample->entry = getBlobGTEntry(blob, image_index, grid->getImg());
      TBOX tbox = blob->bounding_box();
      lsample->blobbox = mutils.tessTBoxToImBox(&tbox, grid->getImg());
      if(lsample->entry == NULL)
        lsample->label = false;
      else
        lsample->label = true;
      samples.push_back(lsample);
      blobnum++;
    }
    return samples;
  }

  // If the given blob in the given image is contained within any of the groundtruth
  // entry rectangles, then return a pointer to the entry it's contained in. Otherwise
  // just return NULL.
  GroundTruthEntry* getBlobGTEntry(tesseract::BLOBINFO* blob, int image_index,
      Pix* img) {
    // open the groundtruth file
    ifstream gtfile;
    string gtfilename = groundtruth_path;
    gtfile.open(gtfilename.c_str(), ifstream::in);
    if((gtfile.rdstate() & ifstream::failbit) != 0) {
      cout << "ERROR: Could not open Groundtruth.dat in " \
           << groundtruth_path << endl;
      exit(EXIT_FAILURE);
    }
    int max = 55;
    char* curline = new char[max];
    bool found = false;
    GroundTruthEntry* entry = NULL;
    while(!gtfile.eof()) {
      gtfile.getline(curline, max);
      if(curline == NULL)
        continue;
      string curlinestr = (string)curline;
      assert(curlinestr.length() < max);
      entry = parseGTLine(curlinestr);
      if(entry == NULL)
        continue;
      if(entry->image_index == image_index) {
        // see if the entry's rectangle overlaps this blob
        tesseract::M_Utils m;
        Box* blob_bb = m.getBlobInfoBox(blob, img);
        int bb_intersects = 0; // this will be 1 if blob_bb intersects math
        boxIntersects(entry->rect, blob_bb, &bb_intersects);
        if(bb_intersects == 1) {
          // found a math blob!
          found = true;
        }
      }
      if(found)
        break;
      delete entry; // delete the entry when we're done with it
      entry = NULL;
    }
    delete [] curline;
    curline = NULL;
    gtfile.close();
    return entry;
  }

  inline void initTraining(const vector<vector<BLSample*> > samples_,
      const string& predictor_path_) {
    predictor_path = predictor_path_;
    samples = samples_;
    classifier.initClassifier();
    trainer.initTraining(classifier, featext);
  }
  inline void train_() {
    classifier = trainer.train_(samples);
  }

  // prediction is just done on one page and will be using some
  // binary classifier which has already been trained with the
  // features specified for this type of trainer_predictor
  inline void initPrediction(string predictor_path_) {
    if(!classifier.isTrained()) {
      cout << "ERROR: Attempted prediction using an untrained classifier!\n";
      exit(EXIT_FAILURE);
    }
    predictor_path = predictor_path_;
  }
  inline bool predict(vector<double> sample) {
    return classifier.predict(sample);
  }

 private:
  // the training samples and their corresponding labels
  // the samples are separated into separate lists for
  // each sample image (i.e. samples[0] is the list of
  // samples for the first image, samples[1] for the
  // second, etc.)
  vector<vector<BLSample*> > samples;

  bool training_done;
  string groundtruth_path; // path to the groundtruth file used to determine sample labels
                           // for training
  string training_set_path; // path to the set of training images being used
  string ext; // image extension
  string predictor_path; // the path where the trained classifier will be
                         // or is stored for later use in prediction
  IClassifier classifier;
  IFeatExt featext;
  I_Trainer trainer;
};

#endif
