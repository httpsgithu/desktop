// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "TestDictionaryDerivedImplementedAs.h"

namespace blink {

TestDictionaryDerivedImplementedAs::TestDictionaryDerivedImplementedAs() {
  setDerivedStringMemberWithDefault(String("default string value"));
}

TestDictionaryDerivedImplementedAs::~TestDictionaryDerivedImplementedAs() {}

TestDictionaryDerivedImplementedAs::TestDictionaryDerivedImplementedAs(const TestDictionaryDerivedImplementedAs&) = default;

TestDictionaryDerivedImplementedAs& TestDictionaryDerivedImplementedAs::operator=(const TestDictionaryDerivedImplementedAs&) = default;

bool TestDictionaryDerivedImplementedAs::hasDerivedStringMember() const {
  return !m_derivedStringMember.isNull();
}
String TestDictionaryDerivedImplementedAs::derivedStringMember() const {
  return m_derivedStringMember;
}
void TestDictionaryDerivedImplementedAs::setDerivedStringMember(String value) {
  m_derivedStringMember = value;
}
bool TestDictionaryDerivedImplementedAs::hasDerivedStringMemberWithDefault() const {
  return !m_derivedStringMemberWithDefault.isNull();
}
String TestDictionaryDerivedImplementedAs::derivedStringMemberWithDefault() const {
  return m_derivedStringMemberWithDefault;
}
void TestDictionaryDerivedImplementedAs::setDerivedStringMemberWithDefault(String value) {
  m_derivedStringMemberWithDefault = value;
}
bool TestDictionaryDerivedImplementedAs::hasRequiredLongMember() const {
  return m_hasRequiredLongMember;
}
int TestDictionaryDerivedImplementedAs::requiredLongMember() const {
  DCHECK(m_hasRequiredLongMember);
  return m_requiredLongMember;
}
void TestDictionaryDerivedImplementedAs::setRequiredLongMember(int value) {
  m_requiredLongMember = value;
  m_hasRequiredLongMember = true;
}
bool TestDictionaryDerivedImplementedAs::hasStringOrDoubleSequenceMember() const {
  return m_hasStringOrDoubleSequenceMember;
}
const HeapVector<StringOrDouble>& TestDictionaryDerivedImplementedAs::stringOrDoubleSequenceMember() const {
  DCHECK(m_hasStringOrDoubleSequenceMember);
  return m_stringOrDoubleSequenceMember;
}
void TestDictionaryDerivedImplementedAs::setStringOrDoubleSequenceMember(const HeapVector<StringOrDouble>& value) {
  m_stringOrDoubleSequenceMember = value;
  m_hasStringOrDoubleSequenceMember = true;
}

DEFINE_TRACE(TestDictionaryDerivedImplementedAs) {
  visitor->trace(m_stringOrDoubleSequenceMember);
  TestDictionary::trace(visitor);
}

}  // namespace blink
