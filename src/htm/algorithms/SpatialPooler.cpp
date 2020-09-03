/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013, Numenta, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 * ---------------------------------------------------------------------- */

/** @file
 * Implementation of SpatialPooler
 */

#include <string>
#include <algorithm>
#include <iterator> //begin()
#include <cmath> //fmod

#include <htm/algorithms/SpatialPooler.hpp>
#include <htm/utils/Topology.hpp>
#include <htm/utils/VectorHelpers.hpp>

using namespace std;
using namespace htm;

class CoordinateConverterND {

public:
  CoordinateConverterND(const vector<UInt> &dimensions) {
    NTA_ASSERT(!dimensions.empty());

    dimensions_ = dimensions;
    UInt b = 1u;
    for (Size i = dimensions.size(); i > 0u; i--) {
      bounds_.insert(bounds_.begin(), b);
      b *= dimensions[i-1];
    }
  }

  void toCoord(UInt index, vector<UInt> &coord) const {
    coord.clear();
    for (Size i = 0u; i < bounds_.size(); i++) {
      coord.push_back((index / bounds_[i]) % dimensions_[i]);
    }
  };

  UInt toIndex(vector<UInt> &coord) const {
    UInt index = 0;
    for (Size i = 0; i < coord.size(); i++) {
      index += coord[i] * bounds_[i];
    }
    return index;
  };

private:
  vector<UInt> dimensions_;
  vector<UInt> bounds_;
};

SpatialPooler::SpatialPooler() {
  // The current version number.
  version_ = 2;
}

SpatialPooler::SpatialPooler(
    const vector<UInt> inputDimensions, 
    const vector<UInt> columnDimensions,
    UInt potentialRadius, 
    Real potentialPct, 
    bool globalInhibition,
    Real localAreaDensity, 
    UInt numActiveColumnsPerInhArea,
    UInt stimulusThreshold, 
    Real synPermInactiveDec, 
    Real synPermActiveInc,
    Real synPermConnected, 
    Real minPctOverlapDutyCycles, 
    UInt dutyCyclePeriod,
    Real boostStrength, 
    UInt seed, 
    UInt spVerbosity, 
    bool wrapAround) : SpatialPooler::SpatialPooler()
{
  initialize(inputDimensions,
             columnDimensions,
             potentialRadius,
             potentialPct,
             globalInhibition,
             localAreaDensity,
             numActiveColumnsPerInhArea,
             stimulusThreshold,
             synPermInactiveDec,
             synPermActiveInc,
             synPermConnected,
             minPctOverlapDutyCycles,
             dutyCyclePeriod,
             boostStrength,
             seed,
             spVerbosity,
             wrapAround);
}

vector<UInt> SpatialPooler::getColumnDimensions() const {
  return columnDimensions_;
}

vector<UInt> SpatialPooler::getInputDimensions() const {
  return inputDimensions_;
}

UInt SpatialPooler::getNumColumns() const { return numColumns_; }

UInt SpatialPooler::getNumInputs() const { return numInputs_; }

UInt SpatialPooler::getPotentialRadius() const { return potentialRadius_; }

void SpatialPooler::setPotentialRadius(UInt potentialRadius) {
  NTA_CHECK(potentialRadius < numInputs_) << "SP setPotentialRadius: " << potentialRadius << " must be < " << numInputs_;
  potentialRadius_ = potentialRadius;
}

Real SpatialPooler::getPotentialPct() const { return potentialPct_; }

void SpatialPooler::setPotentialPct(Real potentialPct) {
  NTA_CHECK(potentialPct > 0.0f && potentialPct <= 1.0f) << "SP setPotentialPct(): out of bounds (0, 1]";
  potentialPct_ = potentialPct;
}

bool SpatialPooler::getGlobalInhibition() const { return globalInhibition_; }

void SpatialPooler::setGlobalInhibition(bool globalInhibition) {
  globalInhibition_ = globalInhibition;
}

Int SpatialPooler::getNumActiveColumnsPerInhArea() const {
  return numActiveColumnsPerInhArea_;
}

void SpatialPooler::setNumActiveColumnsPerInhArea(UInt numActiveColumnsPerInhArea) {
  NTA_CHECK( numActiveColumnsPerInhArea > 0 && numActiveColumnsPerInhArea <= numColumns_); //TODO this boundary could be smarter
  numActiveColumnsPerInhArea_ = numActiveColumnsPerInhArea;
  localAreaDensity_ = 0.0f;  //MUTEX with localAreaDensity
}

Real SpatialPooler::getLocalAreaDensity() const { return localAreaDensity_; }

void SpatialPooler::setLocalAreaDensity(const Real localAreaDensity) {
  NTA_CHECK(localAreaDensity > 0.0f && localAreaDensity <= 1.0f);
  NTA_CHECK(static_cast<UInt>(localAreaDensity * getNumColumns()) > 0) << "Too small density or sp.getNumColumns() -> would have zero active output columns.";
  localAreaDensity_ = localAreaDensity;
  numActiveColumnsPerInhArea_ = 0; //MUTEX with numActiveColumnsPerInhArea
}

UInt SpatialPooler::getStimulusThreshold() const { return stimulusThreshold_; }

void SpatialPooler::setStimulusThreshold(UInt stimulusThreshold) {
  stimulusThreshold_ = stimulusThreshold;
}

UInt SpatialPooler::getInhibitionRadius() const { return inhibitionRadius_; }

void SpatialPooler::setInhibitionRadius(UInt inhibitionRadius) {
  inhibitionRadius_ = inhibitionRadius;
}

UInt SpatialPooler::getDutyCyclePeriod() const { return dutyCyclePeriod_; }

void SpatialPooler::setDutyCyclePeriod(UInt dutyCyclePeriod) {
  dutyCyclePeriod_ = dutyCyclePeriod;
}

Real SpatialPooler::getBoostStrength() const { return boostStrength_; }

void SpatialPooler::setBoostStrength(Real boostStrength) {
  NTA_CHECK(boostStrength >= 0.0f);
  boostStrength_ = boostStrength;
}

UInt SpatialPooler::getIterationNum() const { return iterationNum_; }

void SpatialPooler::setIterationNum(UInt iterationNum) {
  iterationNum_ = iterationNum;
}

UInt SpatialPooler::getIterationLearnNum() const { return iterationLearnNum_; }

void SpatialPooler::setIterationLearnNum(UInt iterationLearnNum) {
  iterationLearnNum_ = iterationLearnNum;
}

UInt SpatialPooler::getSpVerbosity() const { return spVerbosity_; }

void SpatialPooler::setSpVerbosity(UInt spVerbosity) {
  spVerbosity_ = spVerbosity;
}

bool SpatialPooler::getWrapAround() const { return wrapAround_; }

void SpatialPooler::setWrapAround(bool wrapAround) { wrapAround_ = wrapAround; }

UInt SpatialPooler::getUpdatePeriod() const { return updatePeriod_; }

void SpatialPooler::setUpdatePeriod(UInt updatePeriod) {
  updatePeriod_ = updatePeriod;
}

Real SpatialPooler::getSynPermActiveInc() const { return synPermActiveInc_; }

void SpatialPooler::setSynPermActiveInc(Real synPermActiveInc) {
  NTA_CHECK( synPermActiveInc > minPermanence );
  NTA_CHECK( synPermActiveInc <= maxPermanence );
  synPermActiveInc_ = synPermActiveInc;
}

Real SpatialPooler::getSynPermInactiveDec() const {
  return synPermInactiveDec_;
}

void SpatialPooler::setSynPermInactiveDec(Real synPermInactiveDec) {
  NTA_CHECK( synPermInactiveDec >= minPermanence );
  NTA_CHECK( synPermInactiveDec <= maxPermanence );
  synPermInactiveDec_ = synPermInactiveDec;
}

Real SpatialPooler::getSynPermBelowStimulusInc() const {
  return synPermBelowStimulusInc_;
}

void SpatialPooler::setSynPermBelowStimulusInc(Real synPermBelowStimulusInc) {
  NTA_CHECK( synPermBelowStimulusInc > minPermanence );
  NTA_CHECK( synPermBelowStimulusInc <= maxPermanence );
  synPermBelowStimulusInc_ = synPermBelowStimulusInc;
}

Real SpatialPooler::getSynPermConnected() const { return synPermConnected_; }

Real SpatialPooler::getSynPermMax() const { return maxPermanence; }

Real SpatialPooler::getMinPctOverlapDutyCycles() const {
  return minPctOverlapDutyCycles_;
}

void SpatialPooler::setMinPctOverlapDutyCycles(Real minPctOverlapDutyCycles) {
  NTA_CHECK(minPctOverlapDutyCycles > 0.0f && minPctOverlapDutyCycles <= 1.0f);
  minPctOverlapDutyCycles_ = minPctOverlapDutyCycles;
}

void SpatialPooler::getBoostFactors(Real boostFactors[]) const { //TODO make vector
  copy(boostFactors_.begin(), boostFactors_.end(), boostFactors);
}

void SpatialPooler::setBoostFactors(Real boostFactors[]) {
  boostFactors_.assign(&boostFactors[0], &boostFactors[numColumns_]);
}

void SpatialPooler::getOverlapDutyCycles(Real overlapDutyCycles[]) const {
  copy(overlapDutyCycles_.begin(), overlapDutyCycles_.end(), overlapDutyCycles);
}

void SpatialPooler::setOverlapDutyCycles(const Real overlapDutyCycles[]) {
  overlapDutyCycles_.assign(&overlapDutyCycles[0],
                            &overlapDutyCycles[numColumns_]);
}

void SpatialPooler::getActiveDutyCycles(Real activeDutyCycles[]) const {
  copy(activeDutyCycles_.begin(), activeDutyCycles_.end(), activeDutyCycles);
}

void SpatialPooler::setActiveDutyCycles(const Real activeDutyCycles[]) {
  activeDutyCycles_.assign(&activeDutyCycles[0],
                           &activeDutyCycles[numColumns_]);
}

void SpatialPooler::getMinOverlapDutyCycles(Real minOverlapDutyCycles[]) const {
  copy(minOverlapDutyCycles_.begin(), minOverlapDutyCycles_.end(),
       minOverlapDutyCycles);
}

void SpatialPooler::setMinOverlapDutyCycles(const Real minOverlapDutyCycles[]) {
  minOverlapDutyCycles_.assign(&minOverlapDutyCycles[0],
                               &minOverlapDutyCycles[numColumns_]);
}

void SpatialPooler::getPotential(UInt column, UInt potential[]) const {
  NTA_ASSERT(column < numColumns_);
  std::fill( potential, potential + numInputs_, 0 );
  const auto &synapses = connections_.synapsesForSegment( column );
  for(const auto syn : synapses) {
    const auto &synData = connections_.dataForSynapse( syn );
    potential[synData.presynapticCell] = 1;
  }
}

void SpatialPooler::setPotential(UInt column, const UInt potential[]) {
  NTA_ASSERT(column < numColumns_);

  // Remove all existing synapses.
  const auto &synapses = connections_.synapsesForSegment( column );
  while( synapses.size() > 0 )
    connections_.destroySynapse( synapses[0] );

  // Replace with new synapse.
  vector<UInt> potentialDenseVec( potential, potential + numInputs_ );
  const auto &perm = initPermanence_( potentialDenseVec, initConnectedPct_ );
  for(UInt i = 0; i < numInputs_; i++) {
    if( potential[i] )
      connections_.createSynapse( column, i, perm[i] );
  }
}

vector<Real> SpatialPooler::getPermanence(const UInt column, 
				          const Permanence threshold) const {
  NTA_ASSERT(column < numColumns_);
  const auto &synapses = connections_.synapsesForSegment( column );
  vector<Real> permanences(numInputs_, 0.0f);
  for( const auto syn : synapses ) {
    const auto &synData = connections_.dataForSynapse( syn );
    if( synData.permanence >= threshold) { // there must be >= for default case 0.0 where we want all permanences
      permanences[ synData.presynapticCell ] = synData.permanence;
    }
  }
  return permanences;
}


void SpatialPooler::setPermanence(UInt column, const Real permanences[]) {
  NTA_ASSERT(column < numColumns_);

#ifndef NDEBUG // If DEBUG mode ...
  // Keep track of which permanences have been successfully applied to the
  // connections, by zeroing each out after processing.  After all synapses
  // processed check that all permanences are zeroed.
  vector<Real> check_data(permanences, permanences + numInputs_);
#endif

  const auto synapses = connections_.synapsesForSegment( column );
  for(const auto &syn : synapses) {
    const auto &synData = connections_.dataForSynapse( syn );
    const auto &presyn  = synData.presynapticCell;
    connections_.updateSynapsePermanence( syn, permanences[presyn] );

#ifndef NDEBUG
    check_data[presyn] = minPermanence;
#endif
  }

#ifndef NDEBUG
  for(UInt i = 0; i < numInputs_; i++) {
    NTA_ASSERT(check_data[i] == minPermanence)
          << "Can't setPermanence for synapse which is not in potential pool!";
  }
#endif
}


void SpatialPooler::getConnectedCounts(UInt connectedCounts[]) const {
  for(size_t seg = 0; seg < numColumns_; seg++) { //in SP each column = 1 cell with 1 segment only.
    const auto &segment = connections_.dataForSegment( (htm::Segment)seg );
    connectedCounts[ seg ] = segment.numConnected; //TODO numConnected only used here, rm from SegmentData and compute for each segment.synapses?
  }
}


const vector<Real> &SpatialPooler::getBoostedOverlaps() const {
  return boostedOverlaps_;
}

void SpatialPooler::initialize(
    const vector<UInt>& inputDimensions, 
    const vector<UInt>& columnDimensions,
    UInt potentialRadius, 
    Real potentialPct, 
    bool globalInhibition,
    Real localAreaDensity,
    UInt numActiveColumnsPerInhArea,
    UInt stimulusThreshold, 
    Real synPermInactiveDec, 
    Real synPermActiveInc,
    Real synPermConnected, 
    Real minPctOverlapDutyCycles, 
    UInt dutyCyclePeriod,
    Real boostStrength, 
    UInt seed, 
    UInt spVerbosity, 
    bool wrapAround) {

  numInputs_ = 1u;
  inputDimensions_.clear();
  for (const auto &inputDimension : inputDimensions) {
    NTA_CHECK(inputDimension > 0) << "Input dimensions must be positive integers!";
    numInputs_ *= inputDimension;
    inputDimensions_.push_back(inputDimension);
  }
  numColumns_ = 1u;
  columnDimensions_.clear();
  for (const auto &columnDimension : columnDimensions) {
    NTA_CHECK(columnDimension > 0) << "Column dimensions must be positive integers!";
    numColumns_ *= columnDimension;
    columnDimensions_.push_back(columnDimension);
  }
  NTA_CHECK(numColumns_ > 0);
  NTA_CHECK(numInputs_ > 0);

  // 1D input produces 1D output; 2D => 2D, etc. //TODO allow nD -> mD conversion
  NTA_CHECK(inputDimensions_.size() == columnDimensions_.size()); 

  NTA_CHECK( (numActiveColumnsPerInhArea > 0 && localAreaDensity == 0) || (localAreaDensity > 0 && numActiveColumnsPerInhArea == 0) ) 
  << "SP: Mutex. Only one can be set to >0: localAreaDensity, numActiveColumnsPerInhArea";
  if(numActiveColumnsPerInhArea > 0) {
    setNumActiveColumnsPerInhArea(numActiveColumnsPerInhArea);
  } else {
    setLocalAreaDensity(localAreaDensity); 
  }

  rng_ = Random(seed);

  potentialRadius_ = min(numInputs_ , potentialRadius);
  //!setPotentialRadius(potentialRadius);
  setPotentialPct(potentialPct);;
  globalInhibition_ = globalInhibition;
  stimulusThreshold_ = stimulusThreshold;
  synPermInactiveDec_ = synPermInactiveDec;
  synPermActiveInc_ = synPermActiveInc;
  synPermBelowStimulusInc_ = synPermConnected / 10.0f;
  synPermConnected_ = synPermConnected;
  minPctOverlapDutyCycles_ = minPctOverlapDutyCycles;
  dutyCyclePeriod_ = dutyCyclePeriod;
  boostStrength_ = boostStrength;
  spVerbosity_ = spVerbosity;
  wrapAround_ = wrapAround;
  updatePeriod_ = 50u;
  initConnectedPct_ = 0.5f; //FIXME make SP's param, and much lower 0.01 https://discourse.numenta.org/t/spatial-pooler-implementation-for-mnist-dataset/2317/25?u=breznak 
  iterationNum_ = 0u;
  iterationLearnNum_ = 0u;

  overlapDutyCycles_.assign(numColumns_, 0); //TODO make all these sparse or rm to reduce footprint
  activeDutyCycles_.assign(numColumns_, 0);
  minOverlapDutyCycles_.assign(numColumns_, 0.0);
  boostFactors_.assign(numColumns_, 1.0); //1 is neutral value for boosting
  boostedOverlaps_.resize(numColumns_);

  inhibitionRadius_ = 0;

  connections_.initialize(numColumns_, synPermConnected_);
  for (Size i = 0; i < numColumns_; ++i) {
    connections_.createSegment( static_cast<CellIdx>(i) , 1 /* max segments per cell is fixed for SP to 1 */);

    // Note: initMapPotential_ & initPermanence_ return dense arrays.
    vector<UInt> potential = initMapPotential_((UInt)i, wrapAround_);
    vector<Real> perm = initPermanence_(potential, initConnectedPct_);
    for(size_t presyn = 0; presyn < numInputs_; presyn++) {
      if( potential[presyn] )
        connections_.createSynapse( static_cast<Segment>(i), static_cast<htm::CellIdx>(presyn), perm[presyn] );
    }

    connections_.raisePermanencesToThreshold( (Segment)i, stimulusThreshold_ );
  }

  updateInhibitionRadius_();

  if (spVerbosity_ > 0) {
    printParameters();
    std::cout << "CPP SP seed                 = " << seed << std::endl;
  }
}


const vector<SynapseIdx> SpatialPooler::compute(const SDR &input, const bool learn, SDR &active) {
  input.reshape(  inputDimensions_ );
  active.reshape( columnDimensions_ );
  updateBookeepingVars_(learn);

  const auto& overlaps = connections_.computeActivity(input.getSparse(), learn);

  boostOverlaps_(overlaps, boostedOverlaps_);

  auto &activeVector = active.getSparse();
  inhibitColumns_(boostedOverlaps_, activeVector);
  // Notify the active SDR that its internal data vector has changed.  Always
  // call SDR's setter methods even if when modifying the SDR's own data
  // inplace.
  sort( activeVector.begin(), activeVector.end() );
  active.setSparse( activeVector );

  if (learn) {
    adaptSynapses_(input, active);
    updateDutyCycles_(overlaps, active);
    bumpUpWeakColumns_();
    updateBoostFactors_();
    if (isUpdateRound_()) {
      updateInhibitionRadius_();
      updateMinDutyCycles_();
    }
  }

  return overlaps;
}


void SpatialPooler::boostOverlaps_(const vector<SynapseIdx> &overlaps, //TODO use Eigen sparse vector here
                                   vector<Real> &boosted) const {
  if(boostStrength_ < htm::Epsilon) { //boost ~ 0.0, we can skip these computations, just copy the data
    boosted.assign(overlaps.begin(), overlaps.end());
    return;
  }
  for (size_t i = 0; i < numColumns_; i++) {
    boosted[i] = overlaps[i] * boostFactors_[i];
  }
}


UInt SpatialPooler::initMapColumn_(UInt column) const {
  NTA_ASSERT(column < numColumns_);
  vector<UInt> columnCoords;
  const CoordinateConverterND columnConv(columnDimensions_);
  columnConv.toCoord(column, columnCoords);

  vector<UInt> inputCoords;
  inputCoords.reserve(columnCoords.size());
  for (Size i = 0; i < columnCoords.size(); i++) {
    const Real inputCoord = ((Real)columnCoords[i] + 0.5f) *
                            (inputDimensions_[i] / (Real)columnDimensions_[i]);
    inputCoords.push_back((UInt32)floor(inputCoord));
  }

  const CoordinateConverterND inputConv(inputDimensions_);
  return inputConv.toIndex(inputCoords);
}


vector<UInt> SpatialPooler::initMapPotential_(UInt column, bool wrapAround) {
  NTA_ASSERT(column < numColumns_);
  const UInt centerInput = initMapColumn_(column);

  vector<UInt> columnInputs;
  if (wrapAround) {
    for (const auto input : WrappingNeighborhood(centerInput, potentialRadius_, inputDimensions_)) {
      columnInputs.push_back(input);
    }
  } else {
    for (const auto input : Neighborhood(centerInput, potentialRadius_, inputDimensions_)) {
      columnInputs.push_back(input);
    }
  }

  const UInt numPotential = (UInt)round(columnInputs.size() * potentialPct_);
  const auto selectedInputs = rng_.sample<UInt>(columnInputs, numPotential);
  const vector<UInt> potential = VectorHelpers::sparseToBinary<UInt>(selectedInputs, numInputs_);
  return potential;
}


Real SpatialPooler::initPermConnected_() {
  return rng_.realRange(synPermConnected_, maxPermanence);
}


Real SpatialPooler::initPermNonConnected_() {
  return rng_.realRange(minPermanence, synPermConnected_);
}


vector<Real> SpatialPooler::initPermanence_(const vector<UInt> &potential, //TODO make potential sparse
                                            Real connectedPct) {
  vector<Real> perm(numInputs_, 0);
  for (size_t i = 0; i < numInputs_; i++) {
    if (potential[i] < 1) {
      continue;
    }

    if (rng_.getReal64() <= connectedPct) {
      perm[i] = initPermConnected_();
    } else {
      perm[i] = initPermNonConnected_();
    }
  }

  return perm;
}


void SpatialPooler::updateInhibitionRadius_() {
  if (globalInhibition_) {
    inhibitionRadius_ =
        *max_element(columnDimensions_.cbegin(), columnDimensions_.cend());
    return;
  }

  Real connectedSpan = 0.0f;
  for (UInt i = 0; i < numColumns_; i++) {
    connectedSpan += avgConnectedSpanForColumnND_(i);
  }
  connectedSpan /= numColumns_;
  const Real columnsPerInput = avgColumnsPerInput_();
  const Real diameter = connectedSpan * columnsPerInput;
  Real radius = (diameter - 1) / 2.0f;
  radius = max((Real)1.0, radius);
  inhibitionRadius_ = UInt(round(radius));
}


void SpatialPooler::updateMinDutyCycles_() {
  if (globalInhibition_ ||
      inhibitionRadius_ >=
          *max_element(columnDimensions_.begin(), columnDimensions_.end())) {
    updateMinDutyCyclesGlobal_();
  } else {
    updateMinDutyCyclesLocal_();
  }
}


void SpatialPooler::updateMinDutyCyclesGlobal_() {
  const Real maxOverlapDutyCycles =
      *max_element(overlapDutyCycles_.begin(), overlapDutyCycles_.end());

  fill(minOverlapDutyCycles_.begin(), minOverlapDutyCycles_.end(),
       minPctOverlapDutyCycles_ * maxOverlapDutyCycles);
}


void SpatialPooler::updateMinDutyCyclesLocal_() {
  for (UInt i = 0; i < numColumns_; i++) {
    Real maxActiveDuty = 0.0f;
    Real maxOverlapDuty = 0.0f;
    if (wrapAround_) {
     for(auto column : WrappingNeighborhood(i, inhibitionRadius_, columnDimensions_)) {
      maxActiveDuty = max(maxActiveDuty, activeDutyCycles_[column]);
      maxOverlapDuty = max(maxOverlapDuty, overlapDutyCycles_[column]);
     }
    } else {
      for (auto column : Neighborhood(i, inhibitionRadius_, columnDimensions_)) {
      maxActiveDuty = max(maxActiveDuty, activeDutyCycles_[column]);
      maxOverlapDuty = max(maxOverlapDuty, overlapDutyCycles_[column]);
      }
    }

    minOverlapDutyCycles_[i] = maxOverlapDuty * minPctOverlapDutyCycles_;
  }
}


void SpatialPooler::updateDutyCycles_(const vector<SynapseIdx> &overlaps,
                                      SDR &active) {

  // Turn the overlaps array into an SDR. Convert directly to flat-sparse to
  // avoid copies and  type convertions.
  SDR newOverlap({ numColumns_ });
  auto &overlapsSparseVec = newOverlap.getSparse();
  for (UInt i = 0; i < numColumns_; i++) {
    if( overlaps[i] != 0 )
      overlapsSparseVec.push_back( i );
  }
  newOverlap.setSparse( overlapsSparseVec );

  const UInt period = std::min(dutyCyclePeriod_, iterationNum_);

  updateDutyCyclesHelper_(overlapDutyCycles_, newOverlap, period);
  updateDutyCyclesHelper_(activeDutyCycles_, active, period);
}


Real SpatialPooler::avgColumnsPerInput_() const {
  const size_t numDim = max(columnDimensions_.size(), inputDimensions_.size());
  Real columnsPerInput = 0.0f;
  for (size_t i = 0; i < numDim; i++) {
    const Real col = (Real)((i < columnDimensions_.size()) ? columnDimensions_[i] : 1);
    const Real input = (Real)((i < inputDimensions_.size()) ? inputDimensions_[i] : 1);
    columnsPerInput += col / input;
  }
  return columnsPerInput / numDim;
}


Real SpatialPooler::avgConnectedSpanForColumnND_(const UInt column) const {
  NTA_ASSERT(column < numColumns_);

  //get connected synapses
  const auto& connectedDense = getPermanence( column, synPermConnected_ + htm::Epsilon );

  const auto numDimensions = inputDimensions_.size();
  vector<UInt> maxCoord(numDimensions, 0);
  vector<UInt> minCoord(numDimensions, *max_element(inputDimensions_.begin(),
                                                    inputDimensions_.end()));
  const CoordinateConverterND conv(inputDimensions_);
  bool all_zero = true;
  for(UInt i = 0; i < numInputs_; i++) {
    if( connectedDense[i] < synPermConnected_ ) // 0.0 for empty == not-conected values
      continue;
    all_zero = false;
    vector<UInt> columnCoord;
    conv.toCoord(i, columnCoord);
    for (size_t j = 0; j < columnCoord.size(); j++) {
      maxCoord[j] = max(maxCoord[j], columnCoord[j]); //FIXME this computation may be flawed
      minCoord[j] = min(minCoord[j], columnCoord[j]);
    }
  }
  if( all_zero ) return 0.0f;

  UInt totalSpan = 0;
  for (size_t j = 0; j < inputDimensions_.size(); j++) {
    totalSpan += maxCoord[j] - minCoord[j] + 1;
  }

  return (Real)totalSpan / inputDimensions_.size();
}


void SpatialPooler::adaptSynapses_(const SDR &input,
                                   const SDR &active) {
  for(const auto &column : active.getSparse()) {
    connections_.adaptSegment(column, input, synPermActiveInc_, synPermInactiveDec_);
    connections_.raisePermanencesToThreshold( column, stimulusThreshold_ );
  }
}


void SpatialPooler::bumpUpWeakColumns_() {
  for (size_t i = 0; i < numColumns_; i++) {
    if (overlapDutyCycles_[i] >= minOverlapDutyCycles_[i]) {
      continue;
    }
    connections_.bumpSegment( static_cast<Segment>(i), synPermBelowStimulusInc_ );
  }
}


void SpatialPooler::updateDutyCyclesHelper_(vector<Real> &dutyCycles,
                                            const SDR &newValues,
                                            const UInt period) {
  NTA_ASSERT(period > 0);
  NTA_ASSERT(dutyCycles.size() == newValues.size) << "duty dims: " << dutyCycles.size() << " SDR dims: " << newValues.size;

  // Duty cycles are exponential moving averages, typically written like:
  //   alpha = 1 / period
  //   DC( time ) = DC( time - 1 ) * (1 - alpha) + value( time ) * alpha
  // However since the values are sparse this equation is split into two loops,
  // and the second loop iterates over only the non-zero values.

  const Real decay = (period - 1) / static_cast<Real>(period);
  for (Size i = 0; i < dutyCycles.size(); i++)
    dutyCycles[i] *= decay;

  const Real increment = 1.0f / period;  // All non-zero values are 1.
  for(const auto idx : newValues.getSparse())
    dutyCycles[idx] += increment;
}


void SpatialPooler::updateBoostFactors_() {
  if (globalInhibition_) {
    updateBoostFactorsGlobal_();
  } else {
    updateBoostFactorsLocal_();
  }
}


void applyBoosting_(const size_t i,
		    const Real targetDensity, 
		    const vector<Real>& actualDensity,
		    const Real boost,
	            vector<Real>& output) {
  if(boost < htm::Epsilon) return; //skip for disabled boosting
  output[i] = exp((targetDensity - actualDensity[i]) * boost); //TODO doc this code
}


void SpatialPooler::updateBoostFactorsGlobal_() {
  Real targetDensity;
  if (numActiveColumnsPerInhArea_ > 0) {
    UInt inhibitionArea = 1u;
    for(const auto dim : columnDimensions_) {
      inhibitionArea *= min(dim, 2 * inhibitionRadius_ + 1);
    } 
    NTA_ASSERT(inhibitionArea > 0 && inhibitionArea <= numColumns_);
    targetDensity = ((Real)numActiveColumnsPerInhArea_) / inhibitionArea;
    targetDensity = min(targetDensity, (Real)MAX_LOCALAREADENSITY);
  } else {
    targetDensity = localAreaDensity_;
  }
  
  for (size_t i = 0; i < numColumns_; ++i) { 
    applyBoosting_(i, targetDensity, activeDutyCycles_, boostStrength_, boostFactors_);
  }
}


void SpatialPooler::updateBoostFactorsLocal_() {
  for (UInt i = 0; i < numColumns_; ++i) {
    UInt numNeighbors = 0u;
    Real localActivityDensity = 0.0f;

    if (wrapAround_) {
      for(const auto neighbor: WrappingNeighborhood(i, inhibitionRadius_, columnDimensions_)) {
        localActivityDensity += activeDutyCycles_[neighbor];
        numNeighbors += 1;
      }
    } else {
      for(const auto neighbor: Neighborhood(i, inhibitionRadius_, columnDimensions_)) {
        localActivityDensity += activeDutyCycles_[neighbor];
        numNeighbors += 1;
      }
    }

    const Real targetDensity = localActivityDensity / numNeighbors;
    applyBoosting_(i, targetDensity, activeDutyCycles_, boostStrength_, boostFactors_);
  }
}


void SpatialPooler::updateBookeepingVars_(bool learn) {
  iterationNum_++;
  if (learn) {
    iterationLearnNum_++;
  }
}


void SpatialPooler::inhibitColumns_(const vector<Real> &overlaps,
                                    vector<CellIdx> &activeColumns) const {
  Real density = localAreaDensity_;
  if (numActiveColumnsPerInhArea_ > 0) {
    UInt inhibitionArea =
      (UInt)(pow((Real)(2 * inhibitionRadius_ + 1), (Real)columnDimensions_.size()));
    inhibitionArea = min(inhibitionArea, numColumns_);
    density = ((Real)numActiveColumnsPerInhArea_) / inhibitionArea;
    density = min(density, (Real)MAX_LOCALAREADENSITY);
  }

  if (globalInhibition_ ||
      inhibitionRadius_ >
          *max_element(columnDimensions_.begin(), columnDimensions_.end())) {
    inhibitColumnsGlobal_(overlaps, density, activeColumns);
  } else {
    inhibitColumnsLocal_(overlaps, density, activeColumns);
  }
}


void SpatialPooler::inhibitColumnsGlobal_(const vector<Real> &overlaps,
                                          Real density,
                                          vector<UInt> &activeColumns) const {
  NTA_ASSERT(!overlaps.empty());
  NTA_ASSERT(density > 0.0f && density <= 1.0f);

  activeColumns.clear();
  const UInt numDesired = (UInt)(density * numColumns_);
  NTA_CHECK(numDesired > 0) << "Not enough columns (" << numColumns_ << ") "
                            << "for desired density (" << density << ").";
  // Sort the columns by the amount of overlap.  First make a list of all of the
  // column indexes.
  activeColumns.reserve(numColumns_);
  for(UInt i = 0; i < numColumns_; i++)
    activeColumns.push_back(i);
  // Compare the column indexes by their overlap.
  auto compare = [&overlaps](const UInt &a, const UInt &b) -> bool
    {return (overlaps[a] == overlaps[b]) ? a > b : overlaps[a] > overlaps[b];};  //for determinism if overlaps match (tieBreaker does not solve that),
  //otherwise we'd return just `return overlaps[a] > overlaps[b]`. 

  // Do a partial sort to divide the winners from the losers.  This sort is
  // faster than a regular sort because it stops after it partitions the
  // elements about the Nth element, with all elements on their correct side of
  // the Nth element.
  std::nth_element(
    activeColumns.begin(),
    activeColumns.begin() + numDesired,
    activeColumns.end(),
    compare);
  // Remove the columns which lost the competition.
  activeColumns.resize(numDesired);
  // Finish sorting the winner columns by their overlap.
  std::sort(activeColumns.begin(), activeColumns.end(), compare);
  // Remove sub-threshold winners
  while( !activeColumns.empty() &&
         overlaps[activeColumns.back()] < stimulusThreshold_)
      activeColumns.pop_back();
}


void SpatialPooler::inhibitColumnsLocal_(const vector<Real> &overlaps,
                                         Real density,
                                         vector<UInt> &activeColumns) const {
  activeColumns.clear();

  // Tie-breaking: when overlaps are equal, columns that have already been
  // selected are treated as "bigger".
  vector<bool> activeColumnsDense(numColumns_, false);

  for (UInt column = 0; column < numColumns_; column++) {
    if (overlaps[column] < stimulusThreshold_) {
      continue;
    }

    UInt numNeighbors = 0;
    UInt numBigger = 0;


      if (wrapAround_) {
         numNeighbors = 0;  // In wrapAround, number of neighbors to be considered is solely a function of the inhibition radius, 
	 // ... the number of dimensions, and of the size of each of those dimenion
         UInt predN = 1;
	 const UInt diam = 2*inhibitionRadius_ + 1; //the inh radius can change, that's why we recompute here
         for (const auto dim : columnDimensions_) {
           predN *= std::min(diam, dim);
         }
         predN -= 1;
         numNeighbors = predN;
         const UInt numActive_wrap = static_cast<UInt>(0.5f + (density * (numNeighbors + 1)));

        for(const auto neighbor: WrappingNeighborhood(column, inhibitionRadius_,columnDimensions_)) { //TODO if we don't change inh radius (changes only every isUpdateRound()),
		// then these values can be cached -> faster local inh
          if (neighbor == column) {
            continue;
          }

          const Real difference = overlaps[neighbor] - overlaps[column];
          if (difference > 0 || (difference == 0 && activeColumnsDense[neighbor])) {
            numBigger++;
	    if (numBigger >= numActive_wrap) { break; }
          }
	}
      } else {
        for(const auto neighbor: Neighborhood(column, inhibitionRadius_, columnDimensions_)) {
          if (neighbor == column) {
            continue;
          }
          numNeighbors++;

          const Real difference = overlaps[neighbor] - overlaps[column];
          if (difference > 0 || (difference == 0 && activeColumnsDense[neighbor])) {
            numBigger++;
          }
	}
      }

      const UInt numActive = (UInt)(0.5f + (density * (numNeighbors + 1)));
      if (numBigger < numActive) {
        activeColumns.push_back(column);
        activeColumnsDense[column] = true;
      }
  }
}


bool SpatialPooler::isUpdateRound_() const {
  return (iterationNum_ % updatePeriod_) == 0;
}

namespace htm {
std::ostream& operator<< (std::ostream& stream, const SpatialPooler& self)
{
  stream << "Spatial Pooler " << self.connections;
  return stream;
}
}


//----------------------------------------------------------------------
// Debugging helpers
//----------------------------------------------------------------------

// Print the main SP creation parameters
void SpatialPooler::printParameters(std::ostream& out) const {
  out << "------------CPP SpatialPooler Parameters ------------------\n";
  out << "iterationNum                = " << getIterationNum() << std::endl
      << "iterationLearnNum           = " << getIterationLearnNum() << std::endl
      << "numInputs                   = " << getNumInputs() << std::endl
      << "numColumns                  = " << getNumColumns() << std::endl
      << "numActiveColumnsPerInhArea  = " << getNumActiveColumnsPerInhArea()
      << std::endl
      << "potentialPct                = " << getPotentialPct() << std::endl
      << "globalInhibition            = " << getGlobalInhibition() << std::endl
      << "localAreaDensity            = " << getLocalAreaDensity() << std::endl
      << "stimulusThreshold           = " << getStimulusThreshold() << std::endl
      << "synPermActiveInc            = " << getSynPermActiveInc() << std::endl
      << "synPermInactiveDec          = " << getSynPermInactiveDec()
      << std::endl
      << "synPermConnected            = " << getSynPermConnected() << std::endl
      << "minPctOverlapDutyCycles     = " << getMinPctOverlapDutyCycles()
      << std::endl
      << "dutyCyclePeriod             = " << getDutyCyclePeriod() << std::endl
      << "boostStrength               = " << getBoostStrength() << std::endl
      << "spVerbosity                 = " << getSpVerbosity() << std::endl
      << "wrapAround                  = " << getWrapAround() << std::endl
      << "version                     = " << version() << std::endl;
}

void SpatialPooler::printState(const vector<UInt> &state, std::ostream& out) const {
  out << "[  ";
  for (UInt i = 0; i != state.size(); ++i) {
    if (i > 0 && i % 10 == 0) {
      out << "\n   ";
    }
    out << state[i] << " ";
  }
  out << "]\n";
}

void SpatialPooler::printState(const vector<Real> &state, std::ostream& out) const {
  out << "[  ";
  for (UInt i = 0; i != state.size(); ++i) {
    if (i > 0 && i % 10 == 0) {
      out << "\n   ";
    }
    out << state[i];
  }
  out << "]\n";
}


bool SpatialPooler::operator==(const SpatialPooler& o) const{
  //Note on implementation: NTA_CHECKs are used, so we know 
  //what condition failed. 
  try {

  // Check SP's derect member variables first:.
  NTA_CHECK (numInputs_ == o.numInputs_) << "SP equals: numInputs:" << numInputs_ << " vs. " << o.numInputs_ ;
  NTA_CHECK (numColumns_ == o.numColumns_) << "SP equals: numColumns: " << numColumns_ << " vs. " << o.numColumns_;
  NTA_CHECK (potentialRadius_ == o.potentialRadius_) << "SP equals: potentialRadius: " << potentialRadius_ << " vs. " << o.potentialRadius_;
  NTA_CHECK (potentialPct_ == o.potentialPct_) << "SP equals: potentialPct: " << potentialPct_ << " vs. " << o.potentialPct_;
  NTA_CHECK (initConnectedPct_ == o.initConnectedPct_) << "SP equals: initConnectedPct: " << initConnectedPct_ << " vs. " << o.initConnectedPct_;
  NTA_CHECK (globalInhibition_ == o.globalInhibition_) << "SP equals: globalInhibition: " << globalInhibition_ << " vs. " << o.globalInhibition_;
  NTA_CHECK (numActiveColumnsPerInhArea_ == o.numActiveColumnsPerInhArea_) << "SP equals: numActiveColumnsPerInhArea: " 
	  << numActiveColumnsPerInhArea_ << " vs. " << o.numActiveColumnsPerInhArea_;
  NTA_CHECK (localAreaDensity_ == o.localAreaDensity_) << "SP equals: localAreaDensity: " << localAreaDensity_ << " vs. " << o.localAreaDensity_;
  NTA_CHECK (stimulusThreshold_ == o.stimulusThreshold_) << "SP equals: stimulusThreshold: " << stimulusThreshold_ << " vs. " << o.stimulusThreshold_;
  NTA_CHECK (inhibitionRadius_ == o.inhibitionRadius_) << "SP equals: inhibitionRadius: " << inhibitionRadius_ << " vs. " << o.inhibitionRadius_;
  NTA_CHECK (dutyCyclePeriod_ == o.dutyCyclePeriod_) << "SP equals: dutyCyclePeriod: " << dutyCyclePeriod_ << " vs. " << o.dutyCyclePeriod_;
  NTA_CHECK (boostStrength_ == o.boostStrength_) << "SP equals: boostStrength: " << boostStrength_ << " vs. " << o.boostStrength_;
  NTA_CHECK (iterationNum_ == o.iterationNum_) << "SP equals: iterationNum: " << iterationNum_ << " vs. " << o.iterationNum_;
  NTA_CHECK (iterationLearnNum_ == o.iterationLearnNum_) << "SP equals: iterationLearnNum: " << iterationLearnNum_ << " vs. " << o.iterationLearnNum_;
  NTA_CHECK (spVerbosity_ == o.spVerbosity_) << "SP equals: spVerbosity: " << spVerbosity_ << " vs. " << o.spVerbosity_;
  NTA_CHECK (updatePeriod_ == o.updatePeriod_) << "SP equals: updatePeriod: " << updatePeriod_ << " vs. " << o.updatePeriod_;
  NTA_CHECK (synPermInactiveDec_ == o.synPermInactiveDec_) << "SP equals: synPermInactiveDec: " << synPermInactiveDec_ << " vs. " << o.synPermInactiveDec_;
  NTA_CHECK (synPermActiveInc_ == o.synPermActiveInc_) << "SP equals: synPermActiveInc: " << synPermActiveInc_ << " vs. " << o.synPermActiveInc_;
  NTA_CHECK (synPermBelowStimulusInc_ == o.synPermBelowStimulusInc_) << "SP equals: synPermBelowStimulusInc: " << synPermBelowStimulusInc_ << " vs. " << o.synPermBelowStimulusInc_;
  NTA_CHECK (synPermConnected_ == o.synPermConnected_) << "SP equals: synPermConnected: " << synPermConnected_ << " vs. " << o.synPermConnected_;
  NTA_CHECK (minPctOverlapDutyCycles_ == o.minPctOverlapDutyCycles_) 
	  << "SP equals: minPctOverlapDutyCycles: " << minPctOverlapDutyCycles_ << " vs. " << minPctOverlapDutyCycles_;
  NTA_CHECK (wrapAround_ == o.wrapAround_) << "SP equals: wrapAround: " << wrapAround_ << " vs. " << o.wrapAround_;

  //Random
  NTA_CHECK (rng_ == o.rng_) << "SP equals: rng differs";

  // compare connections
  NTA_CHECK (connections_ == o.connections_) << "SP equals: connections: " << connections_ << " vs. " << o.connections_;


  // compare vectors.
  NTA_CHECK (inputDimensions_      == o.inputDimensions_) << "SP equals: inputDimensions differ";
  NTA_CHECK (columnDimensions_     == o.columnDimensions_) << "SP equals: columnDimensions differ";
  NTA_CHECK (boostFactors_         == o.boostFactors_) << "SP equals: boostFactors";
  NTA_CHECK (overlapDutyCycles_    == o.overlapDutyCycles_) << "SP equals: overlapDutyCycles"; //FIXME seems bug is here?!
  NTA_CHECK (activeDutyCycles_     == o.activeDutyCycles_) << "SP equals: activeDutyCucles"; //FIXME here?
  NTA_CHECK (minOverlapDutyCycles_ == o.minOverlapDutyCycles_) << "SP equals: minOverlapDutyCycles";

  //detailed compare potentials
  for (UInt i = 0; i < numColumns_; i++) {
    std::vector<UInt> potential1(numInputs_, 0);
    std::vector<UInt> potential2(numInputs_, 0);
    this->getPotential(i, potential1.data()); //TODO make the method return vect?
    o.getPotential(i, potential2.data());
    NTA_CHECK(potential1 == potential2) << "SP equals: potentials"; //FIXME
  }

  // check get permanences
  for (UInt i = 0; i < numColumns_; i++) {
    const auto& perm1 = this->getPermanence(i);
    const auto& perm2 = o.getPermanence(i);
    NTA_CHECK(perm1 == perm2) << "SP equals: permanences";
  }

  // check get connected synapses
  for (UInt i = 0; i < numColumns_; i++) {
    const auto& con1 = this->getPermanence(i, this->connections.getConnectedThreshold());
    const auto& con2 = o.getPermanence(i, o.connections.getConnectedThreshold());
    NTA_CHECK(con1 == con2) << "SP equals: connected synapses";
  }

  {
  std::vector<UInt> conCounts1(numColumns_, 0);
  std::vector<UInt> conCounts2(numColumns_, 0);
  this->getConnectedCounts(conCounts1.data());
  o.getConnectedCounts(conCounts2.data());
  NTA_CHECK(conCounts1 == conCounts2) << "SP equals: connected column counts";
  }
  std::cout << "here\n";

  // compare connections
  NTA_CHECK (connections_ == o.connections_) << "SP equals: connections: " << connections_ << " vs. " << o.connections_;

  } catch(const htm::Exception& ex) {
    //some check failed -> not equal
    std::cout << "SPP " << ex.what();
    return false;
  }
  return true;

}

