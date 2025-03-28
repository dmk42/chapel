/*
 * Copyright 2021-2025 Hewlett Packard Enterprise Development LP
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CHPL_UAST_EXTERNBLOCK_H
#define CHPL_UAST_EXTERNBLOCK_H

#include "chpl/framework/Location.h"
#include "chpl/uast/AstNode.h"
#include "chpl/uast/SimpleBlockLike.h"

namespace chpl {
namespace uast {


/**
  This class represents an extern block. For example:

  \rst
  .. code-block:: chapel

    extern {
      int cAdd(int a, int b) {
        return a + b;
      }

      writeln(cAdd(2, 2));
    }

  \endrst

 */
class ExternBlock final : public AstNode {
 friend class AstNode;

 private:
  std::string code_;

  ExternBlock(std::string code)
    : AstNode(asttags::ExternBlock),
      code_(std::move(code)) {
    CHPL_ASSERT(numChildren() == 0);
  }

  void serializeInner(Serializer& ser) const override {
    ser.write(code_);
  }

  explicit ExternBlock(Deserializer& des)
    : AstNode(asttags::ExternBlock, des) {
    code_ = des.read<std::string>();
  }

  bool contentsMatchInner(const AstNode* other) const override {
    const ExternBlock* rhs = (const ExternBlock*)other;
    return this->code_ == rhs->code_;
  }

  void markUniqueStringsInner(Context* context) const override {
  }

 public:
  ~ExternBlock() override = default;

  /**
    Create and return an ExternBlock with the given C code.
  */
  static owned<ExternBlock> build(Builder* builder, Location loc,
                                  std::string code);

  const std::string& code() const {
    return code_;
  }
};


} // end namespace uast
} // end namespace chpl

#endif
