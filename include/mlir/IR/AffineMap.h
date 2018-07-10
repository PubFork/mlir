//===- AffineMap.h - MLIR Affine Map Class ----------------------*- C++ -*-===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================
//
// Affine maps are mathematical functions which map a list of dimension
// identifiers and symbols, to multidimensional affine expressions.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_IR_AFFINE_MAP_H
#define MLIR_IR_AFFINE_MAP_H

#include <vector>

#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"

namespace mlir {

class MLIRContext;
class AffineExpr;

/// A multi-dimensional affine map
/// Affine map's are immutable like Type's, and they are uniqued.
/// Eg: (d0, d1) -> (d0/128, d0 mod 128, d1)
/// The names used (d0, d1) don't matter - it's the mathematical function that
/// is unique to this affine map.
class AffineMap {
public:
  static AffineMap *get(unsigned dimCount, unsigned symbolCount,
                        ArrayRef<AffineExpr *> results, MLIRContext *context);

  // Prints affine map to 'os'.
  void print(raw_ostream &os) const;
  void dump() const;

  unsigned getNumDims() const { return numDims; }
  unsigned getNumSymbols() const { return numSymbols; }
  unsigned getNumResults() const { return numResults; }

  ArrayRef<AffineExpr *> getResults() const {
    return ArrayRef<AffineExpr *>(results, numResults);
  }

 private:
  AffineMap(unsigned numDims, unsigned numSymbols, unsigned numResults,
            AffineExpr *const *results);

  AffineMap(const AffineMap&) = delete;
  void operator=(const AffineMap&) = delete;

  const unsigned numDims;
  const unsigned numSymbols;
  const unsigned numResults;

  /// The affine expressions for this (multi-dimensional) map.
  /// TODO: use trailing objects for this.
  AffineExpr *const *const results;
};

}  // end namespace mlir

#endif  // MLIR_IR_AFFINE_MAP_H