//===- Module.h - MLIR Module Class -----------------------------*- C++ -*-===//
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
// Module is the top-level container for code in an MLIR program.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_IR_MODULE_H
#define MLIR_IR_MODULE_H

#include "mlir/IR/Function.h"
#include <vector>

namespace mlir {

class AffineMap;

class Module {
public:
  explicit Module(MLIRContext *context);

  MLIRContext *getContext() const { return context; }

  // FIXME: wrong representation and API.
  // TODO(someone): This should switch to llvm::iplist<Function>.
  std::vector<Function*> functionList;

  // FIXME: wrong representation and API.
  // These affine maps are immutable
  std::vector<const AffineMap *> affineMapList;

  /// Perform (potentially expensive) checks of invariants, used to detect
  /// compiler bugs.  This aborts on failure.
  void verify() const;

  void print(raw_ostream &os) const;
  void dump() const;

private:
  MLIRContext *context;
};
} // end namespace mlir

#endif  // MLIR_IR_FUNCTION_H