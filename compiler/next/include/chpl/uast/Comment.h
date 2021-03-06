/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP
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

#ifndef CHPL_UAST_COMMENT_H
#define CHPL_UAST_COMMENT_H

#include "chpl/uast/Expression.h"
#include "chpl/queries/Location.h"

#include <string>

namespace chpl {
namespace uast {


class Builder;

/**
  This class represents a comment that might be used for documentation.
  Not all comments are represented in the AST (since the comments could
  go anywhere and that would be hard to represent). However, comments that
  are at a statement level will be represented with this type.
 */
class Comment final : public Expression {
 private:
  std::string comment_;

  Comment(std::string s)
   : Expression(asttags::Comment), comment_(std::move(s)) {
  }

  bool contentsMatchInner(const ASTNode* other) const override;
  void markUniqueStringsInner(Context* context) const override;

 public:
  ~Comment() override = default;
  static owned<Comment> build(Builder* builder, Location loc, std::string c);
  const char* c_str() const { return comment_.c_str(); }
  const std::string& str() const { return comment_; }
};


} // end namespace uast
} // end namespace chpl

#endif
