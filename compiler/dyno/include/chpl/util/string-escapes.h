/*
 * Copyright 2021-2022 Hewlett Packard Enterprise Development LP
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

#ifndef CHPL_UTIL_STRING_ESCAPES_H
#define CHPL_UTIL_STRING_ESCAPES_H

#include <string>

namespace chpl {

/** escapes characters according to C quoting rules */
std::string quoteStringForC(const std::string& s);

/** checks for interior null bytes in the string */
bool stringContainsZeroBytes(const char* s, size_t len);


} // end namespace chpl

#endif