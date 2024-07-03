// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"

#include <algorithm>
#include <memory>
#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static void RecordUsageAndDeprecationsOneSelector(
    const CSSSelector* selector,
    const CSSParserContext* context);

namespace {

CSSParserTokenRange ConsumeNestedArgument(CSSParserTokenRange& range) {
  const CSSParserToken& first = range.Peek();
  while (!range.AtEnd() && range.Peek().GetType() != kCommaToken) {
    const CSSParserToken& token = range.Peek();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      range.ConsumeBlock();
      continue;
    }
    range.Consume();
  }
  return range.MakeSubRange(&first, &range.Peek());
}

bool AtEndIgnoringWhitespace(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  return range.AtEnd();
}

bool IsHostPseudoSelector(const CSSSelector& selector) {
  return selector.GetPseudoType() == CSSSelector::kPseudoHost ||
         selector.GetPseudoType() == CSSSelector::kPseudoHostContext;
}

// Some pseudo elements behave as if they have an implicit combinator to their
// left even though they are written without one. This method returns the
// correct implicit combinator. If no new combinator should be used,
// it returns RelationType::kSubSelector.
CSSSelector::RelationType GetImplicitShadowCombinatorForMatching(
    CSSSelector::PseudoType pseudo_type) {
  switch (pseudo_type) {
    case CSSSelector::PseudoType::kPseudoSlotted:
      return CSSSelector::RelationType::kShadowSlot;
    case CSSSelector::PseudoType::kPseudoWebKitCustomElement:
    case CSSSelector::PseudoType::kPseudoBlinkInternalElement:
    case CSSSelector::PseudoType::kPseudoCue:
    case CSSSelector::PseudoType::kPseudoDetailsContent:
    case CSSSelector::PseudoType::kPseudoPlaceholder:
    case CSSSelector::PseudoType::kPseudoFileSelectorButton:
    case CSSSelector::PseudoType::kPseudoSelectDatalist:
      return CSSSelector::RelationType::kUAShadow;
    case CSSSelector::PseudoType::kPseudoPart:
      return CSSSelector::RelationType::kShadowPart;
    default:
      return CSSSelector::RelationType::kSubSelector;
  }
}

bool NeedsImplicitShadowCombinatorForMatching(const CSSSelector& selector) {
  return GetImplicitShadowCombinatorForMatching(selector.GetPseudoType()) !=
         CSSSelector::RelationType::kSubSelector;
}

// Marks the end of parsing a complex selector. (In many cases, there may
// be more complex selectors after this, since we are often dealing with
// lists of complex selectors. Those are marked using SetLastInSelectorList(),
// which happens in CSSSelectorList::AdoptSelectorVector.)
void MarkAsEntireComplexSelector(base::span<CSSSelector> selectors) {
#if DCHECK_IS_ON()
  for (CSSSelector& selector : selectors.first(selectors.size() - 1)) {
    DCHECK(!selector.IsLastInComplexSelector());
  }
#endif
  selectors.back().SetLastInComplexSelector(true);
}

}  // namespace

// static
base::span<CSSSelector> CSSSelectorParser::ParseSelector(
    CSSParserTokenRange range,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    bool is_within_scope,
    bool semicolon_aborts_nested_selector,
    StyleSheetContents* style_sheet,
    HeapVector<CSSSelector>& arena) {
  CSSSelectorParser parser(context, parent_rule_for_nesting, is_within_scope,
                           semicolon_aborts_nested_selector, style_sheet,
                           arena);
  range.ConsumeWhitespace();
  base::span<CSSSelector> result =
      parser.ConsumeComplexSelectorList(range, nesting_type);
  if (!range.AtEnd()) {
    return {};
  }

  parser.RecordUsageAndDeprecations(result);
  return result;
}

// static
base::span<CSSSelector> CSSSelectorParser::ConsumeSelector(
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    bool is_within_scope,
    bool semicolon_aborts_nested_selector,
    StyleSheetContents* style_sheet,
    CSSParserObserver* observer,
    HeapVector<CSSSelector>& arena) {
  CSSSelectorParser parser(context, parent_rule_for_nesting, is_within_scope,
                           semicolon_aborts_nested_selector, style_sheet,
                           arena);
  stream.ConsumeWhitespace();
  base::span<CSSSelector> result =
      parser.ConsumeComplexSelectorList(stream, observer, nesting_type);
  parser.RecordUsageAndDeprecations(result);
  return result;
}

// static
std::optional<base::span<CSSSelector>> CSSSelectorParser::ParseScopeBoundary(
    CSSParserTokenRange range,
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    bool is_within_scope,
    StyleSheetContents* style_sheet,
    HeapVector<CSSSelector>& arena) {
  CSSSelectorParser parser(context, parent_rule_for_nesting, is_within_scope,
                           /*semicolon_aborts_nested_selector=*/false,
                           style_sheet, arena);
  DisallowPseudoElementsScope disallow_pseudo_elements(&parser);

  range.ConsumeWhitespace();
  std::optional<base::span<CSSSelector>> result =
      parser.ConsumeForgivingComplexSelectorList(range, nesting_type);
  DCHECK(result.has_value());
  if (!range.AtEnd()) {
    return std::nullopt;
  }
  parser.RecordUsageAndDeprecations(result.value());
  return result;
}

// static
bool CSSSelectorParser::SupportsComplexSelector(
    CSSParserTokenRange range,
    const CSSParserContext* context) {
  range.ConsumeWhitespace();
  HeapVector<CSSSelector> arena;
  CSSSelectorParser parser(
      context, /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
      /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
  parser.SetInSupportsParsing();
  base::span<CSSSelector> selectors =
      parser.ConsumeComplexSelector(range, CSSNestingType::kNone,
                                    /*first_in_complex_selector_list=*/true);
  if (parser.failed_parsing_ || !range.AtEnd() || selectors.empty()) {
    return false;
  }
  if (ContainsUnknownWebkitPseudoElements(selectors)) {
    return false;
  }
  return true;
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext* context,
                                     const StyleRule* parent_rule_for_nesting,
                                     bool is_within_scope,
                                     bool semicolon_aborts_nested_selector,
                                     StyleSheetContents* style_sheet,
                                     HeapVector<CSSSelector>& output)
    : context_(context),
      parent_rule_for_nesting_(parent_rule_for_nesting),
      is_within_scope_(is_within_scope),
      semicolon_aborts_nested_selector_(semicolon_aborts_nested_selector),
      style_sheet_(style_sheet),
      output_(output) {}

base::span<CSSSelector> CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenRange& range,
    CSSNestingType nesting_type) {
  ResetVectorAfterScope reset_vector(output_);
  if (ConsumeComplexSelector(range, nesting_type,
                             /*first_in_complex_selector_list=*/true)
          .empty()) {
    return {};
  }
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    if (ConsumeComplexSelector(range, nesting_type,
                               /*first_in_complex_selector_list=*/false)
            .empty()) {
      return {};
    }
  }

  if (failed_parsing_) {
    return {};
  }

  return reset_vector.CommitAddedElements();
}

base::span<CSSSelector> CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenStream& stream,
    CSSParserObserver* observer,
    CSSNestingType nesting_type) {
  ResetVectorAfterScope reset_vector(output_);

  bool first_in_complex_selector_list = true;
  while (true) {
    const wtf_size_t selector_offset_start = stream.LookAheadOffset();
    CSSParserTokenRange complex_selector =
        AbortsNestedSelectorParsing(
            kSemicolonToken, semicolon_aborts_nested_selector_, nesting_type)
            ? stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken, kCommaToken,
                                              kSemicolonToken>()
            : stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken, kCommaToken>();
    const wtf_size_t selector_offset_end = stream.LookAheadOffset();

    if (stream.UncheckedAtEnd()) {
      return {};
    }

    if (ConsumeComplexSelector(complex_selector, nesting_type,
                               first_in_complex_selector_list)
            .empty() ||
        failed_parsing_ || !complex_selector.AtEnd()) {
      return {};
    }
    first_in_complex_selector_list = false;

    if (observer) {
      observer->ObserveSelector(selector_offset_start, selector_offset_end);
    }

    if (stream.Peek().GetType() == kLeftBraceToken ||
        AbortsNestedSelectorParsing(stream.Peek().GetType(),
                                    semicolon_aborts_nested_selector_,
                                    nesting_type)) {
      break;
    }

    DCHECK_EQ(stream.Peek().GetType(), kCommaToken);
    stream.ConsumeIncludingWhitespace();
  }

  return reset_vector.CommitAddedElements();
}

CSSSelectorList* CSSSelectorParser::ConsumeCompoundSelectorList(
    CSSParserTokenRange& range) {
  ResetVectorAfterScope reset_vector(output_);

  base::span<CSSSelector> selector =
      ConsumeCompoundSelector(range, CSSNestingType::kNone);
  range.ConsumeWhitespace();
  if (selector.empty()) {
    return nullptr;
  }
  MarkAsEntireComplexSelector(selector);
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    selector = ConsumeCompoundSelector(range, CSSNestingType::kNone);
    range.ConsumeWhitespace();
    if (selector.empty()) {
      return nullptr;
    }
    MarkAsEntireComplexSelector(selector);
  }

  if (failed_parsing_) {
    return nullptr;
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

CSSSelectorList* CSSSelectorParser::ConsumeNestedSelectorList(
    CSSParserTokenRange& range) {
  if (inside_compound_pseudo_) {
    return ConsumeCompoundSelectorList(range);
  }

  ResetVectorAfterScope reset_vector(output_);
  base::span<CSSSelector> result =
      ConsumeComplexSelectorList(range, CSSNestingType::kNone);
  if (result.empty()) {
    return {};
  } else {
    CSSSelectorList* selector_list =
        CSSSelectorList::AdoptSelectorVector(result);
    return selector_list;
  }
}

CSSSelectorList* CSSSelectorParser::ConsumeForgivingNestedSelectorList(
    CSSParserTokenRange& range) {
  if (inside_compound_pseudo_) {
    return ConsumeForgivingCompoundSelectorList(range);
  }
  ResetVectorAfterScope reset_vector(output_);
  std::optional<base::span<CSSSelector>> forgiving_list =
      ConsumeForgivingComplexSelectorList(range, CSSNestingType::kNone);
  if (!forgiving_list.has_value()) {
    return nullptr;
  }
  return CSSSelectorList::AdoptSelectorVector(forgiving_list.value());
}

std::optional<base::span<CSSSelector>>
CSSSelectorParser::ConsumeForgivingComplexSelectorList(
    CSSParserTokenRange& range,
    CSSNestingType nesting_type) {
  if (in_supports_parsing_) {
    base::span<CSSSelector> selectors =
        ConsumeComplexSelectorList(range, nesting_type);
    if (selectors.empty()) {
      return std::nullopt;
    } else {
      return selectors;
    }
  }

  ResetVectorAfterScope reset_vector(output_);

  bool first_in_complex_selector_list = true;
  while (!range.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenRange argument = ConsumeNestedArgument(range);
    CSSParserTokenRange argument_copy = argument;
    wtf_size_t subpos = output_.size();
    base::span<CSSSelector> selector = ConsumeComplexSelector(
        argument, nesting_type, first_in_complex_selector_list);
    if (selector.empty() || failed_parsing_ || !argument.AtEnd()) {
      output_.resize(subpos);  // Drop what we parsed so far.
      AddPlaceholderSelectorIfNeeded(argument_copy);
    }
    if (range.Peek().GetType() != kCommaToken) {
      break;
    }
    range.ConsumeIncludingWhitespace();
    first_in_complex_selector_list = false;
  }

  if (reset_vector.AddedElements().empty()) {
    // Parsed nothing that was supported.
    return base::span<CSSSelector>();
  }

  return reset_vector.CommitAddedElements();
}

// If the argument was unparsable but contained a parent-referencing selector
// (& or :scope), we need to keep it so that we still consider the :is()
// as containing that selector; furthermore, we need to keep it on serialization
// so that a round-trip doesn't lose this information.
// We have similar weaknesses here as in CSS custom properties,
// such as not preserving comments fully.
void CSSSelectorParser::AddPlaceholderSelectorIfNeeded(
    const CSSParserTokenRange& argument) {
  CSSNestingType nesting_type = CSSNestingType::kNone;

  const CSSParserToken* previous_token = nullptr;

  for (const CSSParserToken& token : argument) {
    if (token.GetType() == kDelimiterToken && token.Delimiter() == '&') {
      nesting_type = CSSNestingType::kNesting;
      // Note that a nest-containing selector is also scope-containing, so
      // no need to look for :scope if '&' has been found.
      break;
    }
    if (previous_token && previous_token->GetType() == kColonToken &&
        token.GetType() == kIdentToken &&
        EqualIgnoringASCIICase(token.Value(), "scope")) {
      DCHECK_EQ(nesting_type, CSSNestingType::kNone);
      nesting_type = CSSNestingType::kScope;
    }

    previous_token = &token;
  }

  if (nesting_type != CSSNestingType::kNone) {
    CSSSelector placeholder_selector;
    placeholder_selector.SetMatch(CSSSelector::kPseudoClass);
    placeholder_selector.SetUnparsedPlaceholder(
        nesting_type, AtomicString(argument.Serialize()));
    placeholder_selector.SetLastInComplexSelector(true);
    output_.push_back(placeholder_selector);
  }
}

CSSSelectorList* CSSSelectorParser::ConsumeForgivingCompoundSelectorList(
    CSSParserTokenRange& range) {
  if (in_supports_parsing_) {
    CSSSelectorList* selector_list = ConsumeCompoundSelectorList(range);
    if (!selector_list || !selector_list->IsValid()) {
      return nullptr;
    }
    return selector_list;
  }

  ResetVectorAfterScope reset_vector(output_);
  while (!range.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenRange argument = ConsumeNestedArgument(range);
    wtf_size_t subpos = output_.size();
    base::span<CSSSelector> selector =
        ConsumeCompoundSelector(argument, CSSNestingType::kNone);
    argument.ConsumeWhitespace();
    if (selector.empty() || failed_parsing_ || !argument.AtEnd()) {
      output_.resize(subpos);  // Drop what we parsed so far.
    } else {
      MarkAsEntireComplexSelector(selector);
    }
    if (range.Peek().GetType() != kCommaToken) {
      break;
    }
    range.ConsumeIncludingWhitespace();
  }

  if (reset_vector.AddedElements().empty()) {
    return CSSSelectorList::Empty();
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

CSSSelectorList* CSSSelectorParser::ConsumeForgivingRelativeSelectorList(
    CSSParserTokenRange& range) {
  if (in_supports_parsing_) {
    CSSSelectorList* selector_list = ConsumeRelativeSelectorList(range);
    if (!selector_list || !selector_list->IsValid()) {
      return nullptr;
    }
    return selector_list;
  }

  ResetVectorAfterScope reset_vector(output_);
  while (!range.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenRange argument = ConsumeNestedArgument(range);
    wtf_size_t subpos = output_.size();
    base::span<CSSSelector> selector = ConsumeRelativeSelector(argument);
    if (selector.empty() || failed_parsing_ || !argument.AtEnd()) {
      output_.resize(subpos);  // Drop what we parsed so far.
    }
    if (range.Peek().GetType() != kCommaToken) {
      break;
    }
    range.ConsumeIncludingWhitespace();
  }

  // :has() is not allowed in the pseudos accepting only compound selectors, or
  // not allowed after pseudo elements.
  // (e.g. '::slotted(:has(.a))', '::part(foo):has(:hover)')
  if (inside_compound_pseudo_ ||
      restricting_pseudo_element_ != CSSSelector::kPseudoUnknown ||
      reset_vector.AddedElements().empty()) {
    // TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
    // JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
    // Should return empty CSSSelectorList. (return CSSSelectorList::Empty())
    return nullptr;
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

CSSSelectorList* CSSSelectorParser::ConsumeRelativeSelectorList(
    CSSParserTokenRange& range) {
  ResetVectorAfterScope reset_vector(output_);
  if (ConsumeRelativeSelector(range).empty()) {
    return nullptr;
  }
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    if (ConsumeRelativeSelector(range).empty()) {
      return nullptr;
    }
  }

  if (failed_parsing_) {
    return nullptr;
  }

  // :has() is not allowed in the pseudos accepting only compound selectors, or
  // not allowed after pseudo elements.
  // (e.g. '::slotted(:has(.a))', '::part(foo):has(:hover)')
  if (inside_compound_pseudo_ ||
      restricting_pseudo_element_ != CSSSelector::kPseudoUnknown ||
      reset_vector.AddedElements().empty()) {
    return nullptr;
  }

  return CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements());
}

namespace {

enum CompoundSelectorFlags {
  kHasPseudoElementForRightmostCompound = 1 << 0,
};

unsigned ExtractCompoundFlags(const CSSSelector& simple_selector,
                              CSSParserMode parser_mode) {
  if (simple_selector.Match() != CSSSelector::kPseudoElement) {
    return 0;
  }
  // We don't restrict what follows custom ::-webkit-* pseudo elements in UA
  // sheets. We currently use selectors in mediaControls.css like this:
  //
  // video::-webkit-media-text-track-region-container.scrolling
  if (parser_mode == kUASheetMode &&
      simple_selector.GetPseudoType() ==
          CSSSelector::kPseudoWebKitCustomElement) {
    return 0;
  }
  return kHasPseudoElementForRightmostCompound;
}

unsigned ExtractCompoundFlags(const base::span<CSSSelector> compound_selector,
                              CSSParserMode parser_mode) {
  unsigned compound_flags = 0;
  for (const CSSSelector& simple : compound_selector) {
    if (compound_flags) {
      break;
    }
    compound_flags |= ExtractCompoundFlags(simple, parser_mode);
  }
  return compound_flags;
}

}  // namespace

base::span<CSSSelector> CSSSelectorParser::ConsumeRelativeSelector(
    CSSParserTokenRange& range) {
  ResetVectorAfterScope reset_vector(output_);

  CSSSelector selector;
  selector.SetMatch(CSSSelector::kPseudoClass);
  selector.UpdatePseudoType(AtomicString("-internal-relative-anchor"),
                            *context_, false /*has_arguments*/,
                            context_->Mode());
  DCHECK_EQ(selector.GetPseudoType(), CSSSelector::kPseudoRelativeAnchor);
  output_.push_back(selector);

  CSSSelector::RelationType combinator =
      ConvertRelationToRelative(ConsumeCombinator(range));
  unsigned previous_compound_flags = 0;

  if (!ConsumePartialComplexSelector(range, combinator, previous_compound_flags,
                                     CSSNestingType::kNone)) {
    return {};
  }

  // See ConsumeComplexSelector().
  std::reverse(reset_vector.AddedElements().begin(),
               reset_vector.AddedElements().end());

  MarkAsEntireComplexSelector(reset_vector.AddedElements());
  return reset_vector.CommitAddedElements();
}

// This acts like CSSSelector::GetNestingType, except across a whole
// selector list.
//
// A return value of CSSNestingType::kNesting means that the list
// "contains the nesting selector".
// https://drafts.csswg.org/css-nesting-1/#contain-the-nesting-selector
//
// A return value of CSSNestingType::kScope means that the list
// contains the :scope selector.
static CSSNestingType GetNestingTypeForSelectorList(
    const CSSSelector* selector) {
  if (selector == nullptr) {
    return CSSNestingType::kNone;
  }
  CSSNestingType nesting_type = CSSNestingType::kNone;
  for (;;) {  // Termination condition within loop.
    nesting_type = std::max(nesting_type, selector->GetNestingType());
    if (selector->SelectorList() != nullptr) {
      nesting_type = std::max(
          nesting_type,
          GetNestingTypeForSelectorList(selector->SelectorList()->First()));
    }
    if (selector->IsLastInSelectorList() ||
        nesting_type == CSSNestingType::kNesting) {
      break;
    }
    ++selector;
  }
  return nesting_type;
}

// https://drafts.csswg.org/selectors/#relative-selector-anchor-elements
static CSSSelector CreateImplicitAnchor(
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting) {
  if (nesting_type == CSSNestingType::kNesting) {
    return CSSSelector(parent_rule_for_nesting, /*is_implicit=*/true);
  }
  DCHECK_EQ(nesting_type, CSSNestingType::kScope);
  return CSSSelector(AtomicString("scope"), /*is_implicit=*/true);
}

// Within @scope, each compound that contains either :scope or '&' is prepended
// with an implicit :true + relation=kScopeActivation. This makes it possible
// for SelectorChecker to (re)try the selector's NextSimpleSelector with
// different :scope nodes.
static CSSSelector CreateImplicitScopeActivation() {
  CSSSelector selector;
  selector.SetTrue();
  selector.SetRelation(CSSSelector::kScopeActivation);
  return selector;
}

static std::optional<CSSSelector> MaybeCreateImplicitDescendantAnchor(
    CSSNestingType nesting_type,
    const StyleRule* parent_rule_for_nesting,
    const CSSSelector* selector) {
  switch (nesting_type) {
    case CSSNestingType::kNone:
      break;
    case CSSNestingType::kScope:
    case CSSNestingType::kNesting:
      static_assert(CSSNestingType::kNone < CSSNestingType::kScope);
      static_assert(CSSNestingType::kScope < CSSNestingType::kNesting);
      // For kNesting, we should only produce an implied descendant combinator
      // if the selector list is not nest-containing.
      //
      // For kScope, we should should only produce an implied descendant
      // combinator if the selector list is not :scope-containing. Note however
      // that selectors which are nest-containing are also treated as
      // :scope-containing.
      if (GetNestingTypeForSelectorList(selector) < nesting_type) {
        return CreateImplicitAnchor(nesting_type, parent_rule_for_nesting);
      }
      break;
  }
  return std::nullopt;
}

// A nested rule that starts with a combinator; very similar to
// ConsumeRelativeSelector() (but we don't use the kRelative* relations,
// as they have different matching semantics). There's an implicit anchor
// compound in front, which for CSSNestingType::kNesting is the nesting
// selector (&) and for CSSNestingType::kScope is the :scope pseudo class.
// E.g. given CSSNestingType::kNesting, “> .a” is parsed as “& > .a” ().
base::span<CSSSelector> CSSSelectorParser::ConsumeNestedRelativeSelector(
    CSSParserTokenRange& range,
    CSSNestingType nesting_type) {
  DCHECK_NE(nesting_type, CSSNestingType::kNone);

  ResetVectorAfterScope reset_vector(output_);
  output_.push_back(
      CreateImplicitAnchor(nesting_type, parent_rule_for_nesting_));
  if (nesting_type == CSSNestingType::kScope) {
    output_.push_back(CreateImplicitScopeActivation());
  }
  CSSSelector::RelationType combinator = ConsumeCombinator(range);
  unsigned previous_compound_flags = 0;
  if (!ConsumePartialComplexSelector(range, combinator, previous_compound_flags,
                                     nesting_type)) {
    return {};
  }

  std::reverse(reset_vector.AddedElements().begin(),
               reset_vector.AddedElements().end());

  MarkAsEntireComplexSelector(reset_vector.AddedElements());
  return reset_vector.CommitAddedElements();
}

base::span<CSSSelector> CSSSelectorParser::ConsumeComplexSelector(
    CSSParserTokenRange& range,
    CSSNestingType nesting_type,
    bool first_in_complex_selector_list) {
  if (nesting_type != CSSNestingType::kNone && PeekIsCombinator(range)) {
    // Nested selectors that start with a combinator are to be
    // interpreted as relative selectors (with the anchor being
    // the parent selector, i.e., &).
    return ConsumeNestedRelativeSelector(range, nesting_type);
  }

  ResetVectorAfterScope reset_vector(output_);
  base::span<CSSSelector> compound_selector =
      ConsumeCompoundSelector(range, nesting_type);
  if (compound_selector.empty()) {
    return {};
  }

  // Reverse the compound selector, so that it comes out properly
  // after we reverse everything below.
  std::reverse(compound_selector.begin(), compound_selector.end());

  if (CSSSelector::RelationType combinator = ConsumeCombinator(range)) {
    if (is_inside_has_argument_ &&
        is_inside_logical_combination_in_has_argument_) {
      found_complex_logical_combinations_in_has_argument_ = true;
    }
    unsigned previous_compound_flags =
        ExtractCompoundFlags(compound_selector, context_->Mode());
    if (!ConsumePartialComplexSelector(range, combinator,
                                       previous_compound_flags, nesting_type)) {
      return {};
    }
  }

  // Complex selectors (i.e., groups of compound selectors) are stored
  // right-to-left, ie., the opposite direction of what we parse them. However,
  // within each compound selector, the simple selectors are stored
  // left-to-right. The simplest way of doing this in-place is to reverse each
  // compound selector after we've parsed it (which we do above), and then
  // reverse the entire list in the end. So if the CSS text says:
  //
  //   .a.b.c .d.e.f .g.h
  //
  // we first parse and reverse each compound selector:
  //
  //   .c.b.a .f.e.d .h.g
  //
  // and then reverse the entire list, giving the desired in-memory layout:
  //
  //   .g.h .d.e.f .a.b.c
  //
  // The boundaries between the compound selectors are implicit; they are given
  // by having a Relation() not equal to kSubSelector, so they follow
  // automatically when we do the reversal.
  std::reverse(reset_vector.AddedElements().begin(),
               reset_vector.AddedElements().end());

  if (nesting_type != CSSNestingType::kNone) {
    // In nested top-level rules, if we do not have a & anywhere in the list,
    // we are a relative selector (with & as the anchor), and we must prepend
    // (or append, since we're storing reversed) an implicit & using
    // a descendant combinator.
    //
    // We need to temporarily mark the end of the selector list, for the benefit
    // of GetNestingTypeForSelectorList().
    wtf_size_t last_index = output_.size() - 1;
    output_[last_index].SetLastInSelectorList(true);
    if (std::optional<CSSSelector> anchor = MaybeCreateImplicitDescendantAnchor(
            nesting_type, parent_rule_for_nesting_,
            reset_vector.AddedElements().data())) {
      output_.back().SetRelation(CSSSelector::kDescendant);
      if (nesting_type != CSSNestingType::kNone && is_within_scope_) {
        output_.push_back(CreateImplicitScopeActivation());
      }
      output_.push_back(anchor.value());
    }

    output_[last_index].SetLastInSelectorList(false);
  }

  MarkAsEntireComplexSelector(reset_vector.AddedElements());

  return reset_vector.CommitAddedElements();
}

bool CSSSelectorParser::ConsumePartialComplexSelector(
    CSSParserTokenRange& range,
    CSSSelector::RelationType& combinator,
    unsigned previous_compound_flags,
    CSSNestingType nesting_type) {
  do {
    base::span<CSSSelector> compound_selector =
        ConsumeCompoundSelector(range, nesting_type);
    if (compound_selector.empty()) {
      // No more selectors. If we ended with some explicit combinator
      // (e.g. “a >” and then nothing), that's a parse error.
      // But if not, we're simply done and return everything
      // we've parsed so far.
      return combinator == CSSSelector::kDescendant;
    }
    compound_selector.back().SetRelation(combinator);

    // See ConsumeComplexSelector().
    std::reverse(compound_selector.begin(), compound_selector.end());

    if (previous_compound_flags & kHasPseudoElementForRightmostCompound) {
      // If we've already seen a compound that needs to be rightmost, and still
      // get more, that's a parse error.
      return false;
    }
    previous_compound_flags =
        ExtractCompoundFlags(compound_selector, context_->Mode());
  } while ((combinator = ConsumeCombinator(range)));

  return true;
}

// static
CSSSelector::PseudoType CSSSelectorParser::ParsePseudoType(
    const AtomicString& name,
    bool has_arguments,
    const Document* document) {
  CSSSelector::PseudoType pseudo_type =
      CSSSelector::NameToPseudoType(name, has_arguments, document);

  if (pseudo_type != CSSSelector::PseudoType::kPseudoUnknown) {
    return pseudo_type;
  }

  if (name.StartsWith("-webkit-")) {
    return CSSSelector::PseudoType::kPseudoWebKitCustomElement;
  }
  if (name.StartsWith("-internal-")) {
    return CSSSelector::PseudoType::kPseudoBlinkInternalElement;
  }
  if (name.StartsWith("--")) {
    String custom_name = name.GetString().Substring(2);
    if (ExecutionContext* context =
            document ? document->GetExecutionContext() : nullptr) {
      Deprecation::CountDeprecation(
          context, WebFeature::kCSSCustomStateDeprecatedSyntax);
    }
    if (RuntimeEnabledFeatures::CSSCustomStateDeprecatedSyntaxEnabled()) {
      if (document) {
        // TODO(crbug.com/1514397): Add DevTools deprecations here as well
        document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kDeprecation,
            mojom::ConsoleMessageLevel::kError,
            "Custom state pseudo classes are changing from \":--" +
                custom_name + "\" to \":state(" + custom_name +
                ")\" soon. See more"
                " here: https://github.com/w3c/csswg-drafts/issues/4805"));
      }
      return CSSSelector::PseudoType::kPseudoStateDeprecatedSyntax;
    } else if (document) {
      document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kDeprecation,
          mojom::ConsoleMessageLevel::kError,
          "Custom state pseudo classes have been changed from \":--" +
              custom_name + "\" to \":state(" + custom_name +
              ")\". See more here: "
              "https://github.com/w3c/csswg-drafts/issues/4805"));
    }
  }

  return CSSSelector::PseudoType::kPseudoUnknown;
}

namespace {
PseudoId ParsePseudoElementLegacy(const String& selector_string,
                                  const Node* parent) {
  CSSTokenizer tokenizer(selector_string);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  int number_of_colons = 0;
  while (!range.AtEnd() && range.Peek().GetType() == kColonToken) {
    number_of_colons++;
    range.Consume();
  }

  // TODO(crbug.com/1197620): allowing 0 or 1 preceding colons is not aligned
  // with specs.
  if (!range.AtEnd() && number_of_colons <= 2 &&
      (range.Peek().GetType() == kIdentToken ||
       range.Peek().GetType() == kFunctionToken)) {
    CSSParserToken selector_name_token = range.Consume();
    PseudoId pseudo_id =
        CSSSelector::GetPseudoId(CSSSelectorParser::ParsePseudoType(
            selector_name_token.Value().ToAtomicString(),
            selector_name_token.GetType() == kFunctionToken,
            parent ? &parent->GetDocument() : nullptr));

    if (PseudoElement::IsWebExposed(pseudo_id, parent) &&
        ((PseudoElementHasArguments(pseudo_id) &&
          range.Peek(0).GetType() == kIdentToken &&
          range.Peek(1).GetType() == kRightParenthesisToken &&
          range.Peek(2).GetType() == kEOFToken) ||
         range.Peek().GetType() == kEOFToken)) {
      return pseudo_id;
    }
  }

  return kPseudoIdNone;
}

AtomicString ParsePseudoElementArgument(const String& selector_string) {
  CSSTokenizer tokenizer(selector_string);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  int number_of_colons = 0;
  while (!range.AtEnd() && range.Peek().GetType() == kColonToken) {
    number_of_colons++;
    range.Consume();
  }

  // TODO(crbug.com/1197620): allowing 0 or 1 preceding colons is not aligned
  // with specs.
  if (number_of_colons > 2 || range.Peek(0).GetType() != kFunctionToken ||
      range.Peek(1).GetType() != kIdentToken ||
      range.Peek(2).GetType() != kRightParenthesisToken ||
      range.Peek(3).GetType() != kEOFToken) {
    return g_null_atom;
  }

  return range.Peek(1).Value().ToAtomicString();
}
}  // namespace

// static
PseudoId CSSSelectorParser::ParsePseudoElement(const String& selector_string,
                                               const Node* parent,
                                               AtomicString& argument) {
  if (!RuntimeEnabledFeatures::
          CSSComputedStyleFullPseudoElementParserEnabled()) {
    PseudoId pseudo_id = ParsePseudoElementLegacy(selector_string, parent);
    if (PseudoElementHasArguments(pseudo_id)) {
      argument = ParsePseudoElementArgument(selector_string);
    }
    return pseudo_id;
  }

  CSSTokenizer tokenizer(selector_string);
  auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  int ident_start = 0;
  if (range.Peek().GetType() == kColonToken) {
    ++ident_start;
  }
  if (range.Peek(1).GetType() == kColonToken) {
    ++ident_start;
  }

  CSSParserToken selector_name_token = range.Peek(ident_start);
  if (selector_name_token.GetType() == kIdentToken) {
    if (!selector_name_token.Value().Is8Bit()) {
      return kPseudoIdInvalid;
    }
    if (range.Peek(ident_start + 1).GetType() != kEOFToken) {
      return kPseudoIdInvalid;
    }

    CSSSelector::PseudoType pseudo_type = ParsePseudoType(
        selector_name_token.Value().ToAtomicString(),
        /*has_arguments=*/false, parent ? &parent->GetDocument() : nullptr);

    PseudoId pseudo_id = CSSSelector::GetPseudoId(pseudo_type);
    if (pseudo_id == kPseudoIdBefore || pseudo_id == kPseudoIdAfter ||
        pseudo_id == kPseudoIdFirstLetter || pseudo_id == kPseudoIdFirstLine) {
      return pseudo_id;
    }

    // Keep current behavior for shadow pseudo-elements like ::placeholder.
    if (GetImplicitShadowCombinatorForMatching(pseudo_type) ==
            CSSSelector::kUAShadow &&
        ident_start == 2) {
      return kPseudoIdNone;
    }
  }

  if (ident_start != 2) {
    return ident_start == 1 ? kPseudoIdInvalid : kPseudoIdNone;
  }

  HeapVector<CSSSelector> arena;
  CSSSelectorParser parser(
      StrictCSSParserContext(SecureContextMode::kInsecureContext),
      /*parent_rule_for_nesting=*/nullptr,
      /*is_within_scope=*/false, /*semicolon_aborts_nested_selector=*/false,
      /*style_sheet=*/nullptr, arena);

  ResetVectorAfterScope reset_vector(parser.output_);
  if (!parser.ConsumePseudo(range)) {
    return kPseudoIdInvalid;
  }

  auto selector = reset_vector.AddedElements();
  if (selector.size() != 1 || !range.AtEnd()) {
    return kPseudoIdInvalid;
  }

  const CSSSelector& result = selector[0];
  if (!result.MatchesPseudoElement()) {
    return kPseudoIdInvalid;
  }

  PseudoId pseudo_id = result.GetPseudoId(result.GetPseudoType());
  if (!PseudoElement::IsWebExposed(pseudo_id, parent)) {
    return kPseudoIdInvalid;
  }

  switch (pseudo_id) {
    case kPseudoIdHighlight: {
      argument = result.Argument();
      return pseudo_id;
    }

    case kPseudoIdViewTransitionGroup:
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew: {
      if (result.IdentList().size() != 1 ||
          result.IdentList()[0] == CSSSelector::UniversalSelectorAtom()) {
        return kPseudoIdInvalid;
      }
      argument = result.IdentList()[0];
      return pseudo_id;
    }

    default:
      return pseudo_id;
  }
}

namespace {

bool IsScrollbarPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoActive:
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoCornerPresent:
    case CSSSelector::kPseudoWindowInactive:
      return true;
    default:
      return false;
  }
}

bool IsUserActionPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoFocus:
    case CSSSelector::kPseudoFocusVisible:
    case CSSSelector::kPseudoFocusWithin:
    case CSSSelector::kPseudoActive:
      return true;
    default:
      return false;
  }
}

bool IsPseudoClassValidAfterPseudoElement(
    CSSSelector::PseudoType pseudo_class,
    CSSSelector::PseudoType compound_pseudo_element) {
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoResizer:
    case CSSSelector::kPseudoScrollbar:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece:
      return IsScrollbarPseudoClass(pseudo_class);
    case CSSSelector::kPseudoSelection:
      return pseudo_class == CSSSelector::kPseudoWindowInactive;
    case CSSSelector::kPseudoPart:
    case CSSSelector::kPseudoSelectDatalist:
      return IsUserActionPseudoClass(pseudo_class) ||
             pseudo_class == CSSSelector::kPseudoState ||
             pseudo_class == CSSSelector::kPseudoStateDeprecatedSyntax;
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
    case CSSSelector::kPseudoFileSelectorButton:
      return IsUserActionPseudoClass(pseudo_class);
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoViewTransitionNew:
      return pseudo_class == CSSSelector::kPseudoOnlyChild;
    default:
      return false;
  }
}

bool IsSimpleSelectorValidAfterPseudoElement(
    const CSSSelector& simple_selector,
    CSSSelector::PseudoType compound_pseudo_element) {
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoUnknown:
      return true;
    case CSSSelector::kPseudoAfter:
    case CSSSelector::kPseudoBefore:
      if (simple_selector.GetPseudoType() == CSSSelector::kPseudoMarker &&
          RuntimeEnabledFeatures::CSSMarkerNestedPseudoElementEnabled()) {
        return true;
      }
      break;
    case CSSSelector::kPseudoSlotted:
      return simple_selector.IsTreeAbidingPseudoElement();
    case CSSSelector::kPseudoPart:
      if (simple_selector.IsAllowedAfterPart()) {
        return true;
      }
      break;
    default:
      break;
  }
  if (simple_selector.Match() != CSSSelector::kPseudoClass) {
    return false;
  }
  CSSSelector::PseudoType pseudo = simple_selector.GetPseudoType();
  switch (pseudo) {
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoNot:
    case CSSSelector::kPseudoHas:
      // These pseudo-classes are themselves always valid.
      // CSSSelectorParser::restricting_pseudo_element_ ensures that invalid
      // nested selectors will be dropped if they are invalid according to
      // this function.
      return true;
    default:
      break;
  }
  return IsPseudoClassValidAfterPseudoElement(pseudo, compound_pseudo_element);
}

bool IsPseudoClassValidWithinHasArgument(CSSSelector& selector) {
  DCHECK_EQ(selector.Match(), CSSSelector::kPseudoClass);
  switch (selector.GetPseudoType()) {
    // Limited nested :has() to avoid increasing :has() invalidation complexity.
    case CSSSelector::kPseudoHas:
      return false;
    default:
      return true;
  }
}

// Checks if an implicit scope activation (see CreateImplicitScopeActivation())
// must be prepended to a given compound selector.
static bool SelectorListRequiresScopeActivation(const CSSSelectorList& list);

static bool SimpleSelectorRequiresScopeActivation(const CSSSelector& selector) {
  if (selector.SelectorList()) {
    return SelectorListRequiresScopeActivation(*selector.SelectorList());
  }
  return selector.GetPseudoType() == CSSSelector::kPseudoScope ||
         selector.GetPseudoType() == CSSSelector::kPseudoParent;
}

static bool SelectorListRequiresScopeActivation(const CSSSelectorList& list) {
  for (const CSSSelector* selector = list.First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    for (const CSSSelector* simple = selector; simple;
         simple = simple->NextSimpleSelector()) {
      if (SimpleSelectorRequiresScopeActivation(*simple)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

base::span<CSSSelector> CSSSelectorParser::ConsumeCompoundSelector(
    CSSParserTokenRange& range,
    CSSNestingType nesting_type) {
  ResetVectorAfterScope reset_vector(output_);
  wtf_size_t start_pos = output_.size();
  base::AutoReset<CSSSelector::PseudoType> reset_restricting(
      &restricting_pseudo_element_, restricting_pseudo_element_);

  // See if the compound selector starts with a tag name, universal selector
  // or the likes (these can only be at the beginning). Note that we don't
  // add this to output_ yet, because there are situations where it should
  // be ignored (like if we have a universal selector and don't need it;
  // e.g. *:hover is the same as :hover). Thus, we just keep its data around
  // and prepend it if needed.
  //
  // TODO(sesse): In 99% of cases, we should add this, so the prepending logic
  // gets very complex with having to deal with both the explicit and the
  // implicit case. Consider just inserting it, and then removing it
  // afterwards if we really don't need it.
  AtomicString namespace_prefix;
  AtomicString element_name;
  const bool has_q_name = ConsumeName(range, element_name, namespace_prefix);
  if (context_->IsHTMLDocument()) {
    element_name = element_name.LowerASCII();
  }

  // Consume all the simple selectors that are not tag names.
  while (ConsumeSimpleSelector(range)) {
    const CSSSelector& simple_selector = output_.back();
    if (simple_selector.Match() == CSSSelector::kPseudoElement) {
      restricting_pseudo_element_ = simple_selector.GetPseudoType();
    }
    output_.back().SetRelation(CSSSelector::kSubSelector);
  }

  // While inside a nested selector like :is(), the default namespace shall
  // be ignored when [1]:
  //
  // - The compound selector represents the subject [2], and
  // - The compound selector does not contain a type/universal selector.
  //
  // [1] https://drafts.csswg.org/selectors/#matches
  // [2] https://drafts.csswg.org/selectors/#selector-subject
  base::AutoReset<bool> ignore_namespace(
      &ignore_default_namespace_,
      ignore_default_namespace_ || (resist_default_namespace_ && !has_q_name &&
                                    AtEndIgnoringWhitespace(range)));

  if (reset_vector.AddedElements().empty()) {
    // No simple selectors except for the tag name.
    // TODO(sesse): Does this share too much code with
    // PrependTypeSelectorIfNeeded()?
    if (!has_q_name) {
      // No tag name either, so we fail parsing of this selector.
      return {};
    }
    DCHECK(has_q_name);
    AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
    if (namespace_uri.IsNull()) {
      context_->Count(WebFeature::kCSSUnknownNamespacePrefixInSelector);
      failed_parsing_ = true;
      return {};
    }
    if (namespace_uri == DefaultNamespace()) {
      namespace_prefix = g_null_atom;
    }
    context_->Count(WebFeature::kHasIDClassTagAttribute);
    output_.push_back(CSSSelector(
        QualifiedName(namespace_prefix, element_name, namespace_uri)));
    return reset_vector.CommitAddedElements();
  }

  // Prepend a tag selector if we have one, either explicitly or implicitly.
  // One could be added implicitly e.g. if we are in a non-default namespace
  // and have no tag selector already, we may need to convert .foo to
  // (ns|*).foo, with an implicit universal selector prepended before .foo.
  // The explicit case is when we simply have a tag; e.g. if someone wrote
  // div.foo.bar, we've added .foo.bar earlier and are prepending div now.
  //
  // TODO(futhark@chromium.org): Prepending a type selector to the compound is
  // unnecessary if this compound is an argument to a pseudo selector like
  // :not(), since a type selector will be prepended at the top level of the
  // selector if necessary. We need to propagate that context information here
  // to tell if we are at the top level.
  PrependTypeSelectorIfNeeded(namespace_prefix, has_q_name, element_name,
                              start_pos);

  // The relationship between all of these are that they are sub-selectors.
  for (CSSSelector& selector : reset_vector.AddedElements().first(
           reset_vector.AddedElements().size() - 1)) {
    selector.SetRelation(CSSSelector::kSubSelector);
  }

  // See CSSSelector::RelationType::kScopeActivation.
  bool insert_scope_activation = false;

  if (is_within_scope_ && nesting_type != CSSNestingType::kNone) {
    for (CSSSelector& selector : reset_vector.AddedElements()) {
      if (SimpleSelectorRequiresScopeActivation(selector)) {
        insert_scope_activation = true;
      }
    }
  }

  if (insert_scope_activation) {
    output_.insert(start_pos, CreateImplicitScopeActivation());
  }

  SplitCompoundAtImplicitShadowCrossingCombinator(reset_vector.AddedElements());
  return reset_vector.CommitAddedElements();
}

bool CSSSelectorParser::ConsumeSimpleSelector(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  bool ok;
  if (token.GetType() == kHashToken) {
    ok = ConsumeId(range);
  } else if (token.GetType() == kDelimiterToken && token.Delimiter() == '.') {
    ok = ConsumeClass(range);
  } else if (token.GetType() == kLeftBracketToken) {
    ok = ConsumeAttribute(range);
  } else if (token.GetType() == kColonToken) {
    ok = ConsumePseudo(range);
  } else if (token.GetType() == kDelimiterToken && token.Delimiter() == '&') {
    ok = ConsumeNestingParent(range);
  } else {
    return false;
  }
  // TODO(futhark@chromium.org): crbug.com/578131
  // The UASheetMode check is a work-around to allow this selector in
  // mediaControls(New).css:
  // video::-webkit-media-text-track-region-container.scrolling
  if (!ok || (context_->Mode() != kUASheetMode &&
              !IsSimpleSelectorValidAfterPseudoElement(
                  output_.back(), restricting_pseudo_element_))) {
    failed_parsing_ = true;
    return false;
  }
  return true;
}

bool CSSSelectorParser::ConsumeName(CSSParserTokenRange& range,
                                    AtomicString& name,
                                    AtomicString& namespace_prefix) {
  name = g_null_atom;
  namespace_prefix = g_null_atom;

  const CSSParserToken& first_token = range.Peek();
  if (first_token.GetType() == kIdentToken) {
    name = first_token.Value().ToAtomicString();
    range.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '*') {
    name = CSSSelector::UniversalSelectorAtom();
    range.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '|') {
    // This is an empty namespace, which'll get assigned this value below
    name = g_empty_atom;
  } else {
    return false;
  }

  if (range.Peek().GetType() != kDelimiterToken ||
      range.Peek().Delimiter() != '|') {
    return true;
  }

  namespace_prefix =
      name == CSSSelector::UniversalSelectorAtom() ? g_star_atom : name;
  if (range.Peek(1).GetType() == kIdentToken) {
    range.Consume();
    name = range.Consume().Value().ToAtomicString();
  } else if (range.Peek(1).GetType() == kDelimiterToken &&
             range.Peek(1).Delimiter() == '*') {
    range.Consume();
    range.Consume();
    name = CSSSelector::UniversalSelectorAtom();
  } else {
    name = g_null_atom;
    namespace_prefix = g_null_atom;
    return false;
  }

  return true;
}

bool CSSSelectorParser::ConsumeId(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kHashToken);
  if (range.Peek().GetHashTokenType() != kHashTokenId) {
    return false;
  }
  CSSSelector selector;
  selector.SetMatch(CSSSelector::kId);
  AtomicString value = range.Consume().Value().ToAtomicString();
  selector.SetValue(value, IsQuirksModeBehavior(context_->Mode()));
  output_.push_back(std::move(selector));
  context_->Count(WebFeature::kHasIDClassTagAttribute);
  return true;
}

bool CSSSelectorParser::ConsumeClass(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kDelimiterToken);
  DCHECK_EQ(range.Peek().Delimiter(), '.');
  range.Consume();
  if (range.Peek().GetType() != kIdentToken) {
    return false;
  }
  CSSSelector selector;
  selector.SetMatch(CSSSelector::kClass);
  AtomicString value = range.Consume().Value().ToAtomicString();
  selector.SetValue(value, IsQuirksModeBehavior(context_->Mode()));
  output_.push_back(std::move(selector));
  context_->Count(WebFeature::kHasIDClassTagAttribute);
  return true;
}

bool CSSSelectorParser::ConsumeAttribute(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kLeftBracketToken);
  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();

  AtomicString namespace_prefix;
  AtomicString attribute_name;
  if (!ConsumeName(block, attribute_name, namespace_prefix)) {
    return false;
  }
  if (attribute_name == CSSSelector::UniversalSelectorAtom()) {
    return false;
  }
  block.ConsumeWhitespace();

  if (context_->IsHTMLDocument()) {
    attribute_name = attribute_name.LowerASCII();
  }

  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull()) {
    return false;
  }

  QualifiedName qualified_name =
      namespace_prefix.IsNull()
          ? QualifiedName(attribute_name)
          : QualifiedName(namespace_prefix, attribute_name, namespace_uri);

  if (block.AtEnd()) {
    CSSSelector selector;
    selector.SetAttribute(qualified_name,
                          CSSSelector::AttributeMatchType::kCaseSensitive);
    selector.SetMatch(CSSSelector::kAttributeSet);
    output_.push_back(std::move(selector));
    context_->Count(WebFeature::kHasIDClassTagAttribute);
    return true;
  }

  CSSSelector selector;
  selector.SetMatch(ConsumeAttributeMatch(block));

  const CSSParserToken& attribute_value = block.ConsumeIncludingWhitespace();
  if (attribute_value.GetType() != kIdentToken &&
      attribute_value.GetType() != kStringToken) {
    return false;
  }
  selector.SetValue(attribute_value.Value().ToAtomicString());
  selector.SetAttribute(qualified_name, ConsumeAttributeFlags(block));

  if (!block.AtEnd()) {
    return false;
  }
  output_.push_back(std::move(selector));
  context_->Count(WebFeature::kHasIDClassTagAttribute);
  return true;
}

bool CSSSelectorParser::ConsumePseudo(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kColonToken);
  range.Consume();

  int colons = 1;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    colons++;
  }

  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken && token.GetType() != kFunctionToken) {
    return false;
  }

  CSSSelector selector;
  selector.SetMatch(colons == 1 ? CSSSelector::kPseudoClass
                                : CSSSelector::kPseudoElement);

  bool has_arguments = token.GetType() == kFunctionToken;
  selector.UpdatePseudoType(token.Value().ToAtomicString(), *context_,
                            has_arguments, context_->Mode());

  if (selector.Match() == CSSSelector::kPseudoElement) {
    switch (selector.GetPseudoType()) {
      case CSSSelector::kPseudoBefore:
      case CSSSelector::kPseudoAfter:
        context_->Count(WebFeature::kHasBeforeOrAfterPseudoElement);
        break;
      case CSSSelector::kPseudoMarker:
        if (context_->Mode() != kUASheetMode) {
          context_->Count(WebFeature::kHasMarkerPseudoElement);
        }
        break;
      default:
        break;
    }
  }

  if (selector.Match() == CSSSelector::kPseudoElement &&
      disallow_pseudo_elements_) {
    return false;
  }

  if (is_inside_has_argument_) {
    DCHECK(disallow_pseudo_elements_);
    if (!IsPseudoClassValidWithinHasArgument(selector)) {
      return false;
    }
    found_pseudo_in_has_argument_ = true;
  }

  if (token.GetType() == kIdentToken) {
    range.Consume();
    if (selector.GetPseudoType() == CSSSelector::kPseudoUnknown) {
      return false;
    }
    output_.push_back(std::move(selector));
    return true;
  }

  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  if (selector.GetPseudoType() == CSSSelector::kPseudoUnknown) {
    return false;
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoIs: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      base::AutoReset<bool> is_inside_logical_combination_in_has_argument(
          &is_inside_logical_combination_in_has_argument_,
          is_inside_has_argument_);

      CSSSelectorList* selector_list =
          ConsumeForgivingNestedSelectorList(block);
      if (!selector_list || !block.AtEnd()) {
        return false;
      }
      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoWhere: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      base::AutoReset<bool> is_inside_logical_combination_in_has_argument(
          &is_inside_logical_combination_in_has_argument_,
          is_inside_has_argument_);

      CSSSelectorList* selector_list =
          ConsumeForgivingNestedSelectorList(block);
      if (!selector_list || !block.AtEnd()) {
        return false;
      }
      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoCue: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> inside_compound(&inside_compound_pseudo_, true);
      base::AutoReset<bool> ignore_namespace(
          &ignore_default_namespace_,
          ignore_default_namespace_ ||
              selector.GetPseudoType() == CSSSelector::kPseudoCue);

      CSSSelectorList* selector_list = ConsumeCompoundSelectorList(block);
      if (!selector_list || !selector_list->IsValid() || !block.AtEnd()) {
        return false;
      }

      if (!selector_list->HasOneSelector()) {
        if (selector.GetPseudoType() == CSSSelector::kPseudoHost) {
          return false;
        }
        if (selector.GetPseudoType() == CSSSelector::kPseudoHostContext) {
          return false;
        }
      }

      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoHas: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);

      base::AutoReset<bool> is_inside_has_argument(&is_inside_has_argument_,
                                                   true);
      base::AutoReset<bool> found_pseudo_in_has_argument(
          &found_pseudo_in_has_argument_, false);
      base::AutoReset<bool> found_complex_logical_combinations_in_has_argument(
          &found_complex_logical_combinations_in_has_argument_, false);

      CSSSelectorList* selector_list;
      selector_list = ConsumeRelativeSelectorList(block);
      if (!selector_list || !selector_list->IsValid() || !block.AtEnd()) {
        return false;
      }
      selector.SetSelectorList(selector_list);
      if (found_pseudo_in_has_argument_) {
        selector.SetContainsPseudoInsideHasPseudoClass();
      }
      if (found_complex_logical_combinations_in_has_argument_) {
        selector.SetContainsComplexLogicalCombinationsInsideHasPseudoClass();
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoNot: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      base::AutoReset<bool> is_inside_logical_combination_in_has_argument(
          &is_inside_logical_combination_in_has_argument_,
          is_inside_has_argument_);

      CSSSelectorList* selector_list = ConsumeNestedSelectorList(block);
      if (!selector_list || !selector_list->IsValid() || !block.AtEnd()) {
        return false;
      }

      selector.SetSelectorList(selector_list);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoDir:
    case CSSSelector::kPseudoState: {
      CHECK(selector.GetPseudoType() != CSSSelector::kPseudoState ||
            RuntimeEnabledFeatures::CSSCustomStateNewSyntaxEnabled());
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd()) {
        return false;
      }
      selector.SetArgument(ident.Value().ToAtomicString());
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoPart: {
      Vector<AtomicString> parts;
      do {
        const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
        if (ident.GetType() != kIdentToken) {
          return false;
        }
        parts.push_back(ident.Value().ToAtomicString());
      } while (!block.AtEnd());
      selector.SetIdentList(std::make_unique<Vector<AtomicString>>(parts));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoActiveViewTransitionType: {
      if (!RuntimeEnabledFeatures::ViewTransitionTypesEnabled()) {
        return false;
      }

      Vector<AtomicString> types;
      for (;;) {
        const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
        if (ident.GetType() != kIdentToken) {
          return false;
        }
        types.push_back(ident.Value().ToAtomicString());

        if (block.AtEnd()) {
          break;
        }

        const CSSParserToken& comma = block.ConsumeIncludingWhitespace();
        if (comma.GetType() != kCommaToken || block.AtEnd()) {
          return false;
        }
      }
      selector.SetIdentList(std::make_unique<Vector<AtomicString>>(types));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoViewTransitionNew: {
      std::unique_ptr<Vector<AtomicString>> name_and_classes =
          std::make_unique<Vector<AtomicString>>();
      if (RuntimeEnabledFeatures::CSSViewTransitionClassEnabled()) {
        if (block.Peek().GetType() == kDelimiterToken &&
            block.Peek().Delimiter() == '.') {
          name_and_classes->push_back(CSSSelector::UniversalSelectorAtom());
        }
      }

      if (name_and_classes->empty()) {
        const CSSParserToken& ident = block.Consume();
        if (ident.GetType() == kIdentToken) {
          name_and_classes->push_back(ident.Value().ToAtomicString());
        } else if (ident.GetType() == kDelimiterToken &&
                   ident.Delimiter() == '*') {
          name_and_classes->push_back(CSSSelector::UniversalSelectorAtom());
        } else {
          return false;
        }
      }

      CHECK_EQ(name_and_classes->size(), 1ull);

      if (RuntimeEnabledFeatures::CSSViewTransitionClassEnabled()) {
        while (!block.AtEnd() && block.Peek().GetType() != kWhitespaceToken) {
          if (block.Peek().GetType() != kDelimiterToken ||
              block.Consume().Delimiter() != '.') {
            return false;
          }

          if (block.Peek().GetType() != kIdentToken) {
            return false;
          }
          name_and_classes->push_back(block.Consume().Value().ToAtomicString());
        }
      }

      block.ConsumeWhitespace();

      if (!block.AtEnd()) {
        return false;
      }

      selector.SetIdentList(std::move(name_and_classes));
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoSlotted: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> inside_compound(&inside_compound_pseudo_, true);

      {
        ResetVectorAfterScope reset_vector(output_);
        base::span<CSSSelector> inner_selector =
            ConsumeCompoundSelector(block, CSSNestingType::kNone);
        block.ConsumeWhitespace();
        if (inner_selector.empty() || !block.AtEnd()) {
          return false;
        }
        MarkAsEntireComplexSelector(reset_vector.AddedElements());
        selector.SetSelectorList(
            CSSSelectorList::AdoptSelectorVector(reset_vector.AddedElements()));
      }
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoLang: {
      // FIXME: CSS Selectors Level 4 allows :lang(*-foo)
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd()) {
        return false;
      }
      selector.SetArgument(ident.Value().ToAtomicString());
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastOfType: {
      std::pair<int, int> ab;
      if (!ConsumeANPlusB(block, ab)) {
        return false;
      }
      block.ConsumeWhitespace();
      if (block.AtEnd()) {
        selector.SetNth(ab.first, ab.second, nullptr);
        output_.push_back(std::move(selector));
        return true;
      }

      // See if there's an “of ...” part.
      if (selector.GetPseudoType() != CSSSelector::kPseudoNthChild &&
          selector.GetPseudoType() != CSSSelector::kPseudoNthLastChild) {
        return false;
      }

      CSSSelectorList* sub_selectors = ConsumeNthChildOfSelectors(block);
      if (sub_selectors == nullptr) {
        return false;
      }
      block.ConsumeWhitespace();
      if (!block.AtEnd()) {
        return false;
      }

      selector.SetNth(ab.first, ab.second, sub_selectors);
      output_.push_back(std::move(selector));
      return true;
    }
    case CSSSelector::kPseudoHighlight: {
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd()) {
        return false;
      }
      selector.SetArgument(ident.Value().ToAtomicString());
      output_.push_back(std::move(selector));
      return true;
    }
    default:
      break;
  }

  return false;
}

bool CSSSelectorParser::ConsumeNestingParent(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kDelimiterToken);
  DCHECK_EQ(range.Peek().Delimiter(), '&');
  range.Consume();

  output_.push_back(
      CSSSelector(parent_rule_for_nesting_, /*is_implicit=*/false));

  if (is_inside_has_argument_) {
    // In case that a nesting parent selector is inside a :has() pseudo class,
    // mark the :has() containing a pseudo selector so that the StyleEngine can
    // invalidate the anchor element of the :has() for a pseudo state change
    // in the parent selector. (crbug.com/1517866)
    // This ignores whether the nesting parent actually contains a pseudo to
    // avoid nesting parent lookup overhead and the complexity caused by
    // reparenting style rules.
    found_pseudo_in_has_argument_ = true;
  }

  return true;
}

bool CSSSelectorParser::PeekIsCombinator(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();

  if (range.Peek().GetType() != kDelimiterToken) {
    return false;
  }

  switch (range.Peek().Delimiter()) {
    case '+':
    case '~':
    case '>':
      return true;
    default:
      return false;
  }
}

CSSSelector::RelationType CSSSelectorParser::ConsumeCombinator(
    CSSParserTokenRange& range) {
  CSSSelector::RelationType fallback_result = CSSSelector::kSubSelector;
  while (range.Peek().GetType() == kWhitespaceToken) {
    range.Consume();
    fallback_result = CSSSelector::kDescendant;
  }

  if (range.Peek().GetType() != kDelimiterToken) {
    return fallback_result;
  }

  switch (range.Peek().Delimiter()) {
    case '+':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kDirectAdjacent;

    case '~':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kIndirectAdjacent;

    case '>':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kChild;

    default:
      break;
  }
  return fallback_result;
}

CSSSelector::MatchType CSSSelectorParser::ConsumeAttributeMatch(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  switch (token.GetType()) {
    case kIncludeMatchToken:
      return CSSSelector::kAttributeList;
    case kDashMatchToken:
      return CSSSelector::kAttributeHyphen;
    case kPrefixMatchToken:
      return CSSSelector::kAttributeBegin;
    case kSuffixMatchToken:
      return CSSSelector::kAttributeEnd;
    case kSubstringMatchToken:
      return CSSSelector::kAttributeContain;
    case kDelimiterToken:
      if (token.Delimiter() == '=') {
        return CSSSelector::kAttributeExact;
      }
      [[fallthrough]];
    default:
      failed_parsing_ = true;
      return CSSSelector::kAttributeExact;
  }
}

CSSSelector::AttributeMatchType CSSSelectorParser::ConsumeAttributeFlags(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken) {
    return CSSSelector::AttributeMatchType::kCaseSensitive;
  }
  const CSSParserToken& flag = range.ConsumeIncludingWhitespace();
  if (EqualIgnoringASCIICase(flag.Value(), "i")) {
    return CSSSelector::AttributeMatchType::kCaseInsensitive;
  } else if (EqualIgnoringASCIICase(flag.Value(), "s") &&
             RuntimeEnabledFeatures::CSSCaseSensitiveSelectorEnabled()) {
    return CSSSelector::AttributeMatchType::kCaseSensitiveAlways;
  }
  failed_parsing_ = true;
  return CSSSelector::AttributeMatchType::kCaseSensitive;
}

bool CSSSelectorParser::ConsumeANPlusB(CSSParserTokenRange& range,
                                       std::pair<int, int>& result) {
  const CSSParserToken& token = range.Consume();
  if (token.GetType() == kNumberToken &&
      token.GetNumericValueType() == kIntegerValueType) {
    result = std::make_pair(0, ClampTo<int>(token.NumericValue()));
    return true;
  }
  if (token.GetType() == kIdentToken) {
    if (EqualIgnoringASCIICase(token.Value(), "odd")) {
      result = std::make_pair(2, 1);
      return true;
    }
    if (EqualIgnoringASCIICase(token.Value(), "even")) {
      result = std::make_pair(2, 0);
      return true;
    }
  }

  // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
  // this will store a string of the form 'n', 'n-', or 'n-123'.
  String n_string;

  if (token.GetType() == kDelimiterToken && token.Delimiter() == '+' &&
      range.Peek().GetType() == kIdentToken) {
    result.first = 1;
    n_string = range.Consume().Value().ToString();
  } else if (token.GetType() == kDimensionToken &&
             token.GetNumericValueType() == kIntegerValueType) {
    result.first = ClampTo<int>(token.NumericValue());
    n_string = token.Value().ToString();
  } else if (token.GetType() == kIdentToken) {
    if (token.Value()[0] == '-') {
      result.first = -1;
      n_string = token.Value().ToString().Substring(1);
    } else {
      result.first = 1;
      n_string = token.Value().ToString();
    }
  }

  range.ConsumeWhitespace();

  if (n_string.empty() || !IsASCIIAlphaCaselessEqual(n_string[0], 'n')) {
    return false;
  }
  if (n_string.length() > 1 && n_string[1] != '-') {
    return false;
  }

  if (n_string.length() > 2) {
    bool valid;
    result.second = n_string.Substring(1).ToIntStrict(&valid);
    return valid;
  }

  NumericSign sign = n_string.length() == 1 ? kNoSign : kMinusSign;
  if (sign == kNoSign && range.Peek().GetType() == kDelimiterToken) {
    char delimiter_sign = range.ConsumeIncludingWhitespace().Delimiter();
    if (delimiter_sign == '+') {
      sign = kPlusSign;
    } else if (delimiter_sign == '-') {
      sign = kMinusSign;
    } else {
      return false;
    }
  }

  if (sign == kNoSign && range.Peek().GetType() != kNumberToken) {
    result.second = 0;
    return true;
  }

  const CSSParserToken& b = range.Consume();
  if (b.GetType() != kNumberToken ||
      b.GetNumericValueType() != kIntegerValueType) {
    return false;
  }
  if ((b.GetNumericSign() == kNoSign) == (sign == kNoSign)) {
    return false;
  }
  result.second = ClampTo<int>(b.NumericValue());
  if (sign == kMinusSign) {
    // Negating minimum integer returns itself, instead return max integer.
    if (UNLIKELY(result.second == std::numeric_limits<int>::min())) {
      result.second = std::numeric_limits<int>::max();
    } else {
      result.second = -result.second;
    }
  }
  return true;
}

// Consumes the “of ...” part of :nth_child(An+B of ...).
// Returns nullptr on failure.
CSSSelectorList* CSSSelectorParser::ConsumeNthChildOfSelectors(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken ||
      range.Consume().Value() != "of") {
    return nullptr;
  }
  range.ConsumeWhitespace();

  ResetVectorAfterScope reset_vector(output_);
  base::span<CSSSelector> selectors =
      ConsumeComplexSelectorList(range, CSSNestingType::kNone);
  if (selectors.empty()) {
    return nullptr;
  }
  return CSSSelectorList::AdoptSelectorVector(selectors);
}

const AtomicString& CSSSelectorParser::DefaultNamespace() const {
  if (!style_sheet_ || ignore_default_namespace_) {
    return g_star_atom;
  }
  return style_sheet_->DefaultNamespace();
}

const AtomicString& CSSSelectorParser::DetermineNamespace(
    const AtomicString& prefix) {
  if (prefix.IsNull()) {
    return DefaultNamespace();
  }
  if (prefix.empty()) {
    return g_empty_atom;  // No namespace. If an element/attribute has a
                          // namespace, we won't match it.
  }
  if (prefix == g_star_atom) {
    return g_star_atom;  // We'll match any namespace.
  }
  if (!style_sheet_) {
    return g_null_atom;  // Cannot resolve prefix to namespace without a
                         // stylesheet, syntax error.
  }
  return style_sheet_->NamespaceURIFromPrefix(prefix);
}

void CSSSelectorParser::PrependTypeSelectorIfNeeded(
    const AtomicString& namespace_prefix,
    bool has_q_name,
    const AtomicString& element_name,
    wtf_size_t start_index_of_compound_selector) {
  const CSSSelector& compound_selector =
      output_[start_index_of_compound_selector];

  if (!has_q_name && DefaultNamespace() == g_star_atom &&
      !NeedsImplicitShadowCombinatorForMatching(compound_selector)) {
    return;
  }

  AtomicString determined_element_name =
      !has_q_name ? CSSSelector::UniversalSelectorAtom() : element_name;
  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull()) {
    failed_parsing_ = true;
    return;
  }
  AtomicString determined_prefix = namespace_prefix;
  if (namespace_uri == DefaultNamespace()) {
    determined_prefix = g_null_atom;
  }
  QualifiedName tag =
      QualifiedName(determined_prefix, determined_element_name, namespace_uri);

  // *:host/*:host-context never matches, so we can't discard the *,
  // otherwise we can't tell the difference between *:host and just :host.
  //
  // Also, selectors where we use a ShadowPseudo combinator between the
  // element and the pseudo element for matching (custom pseudo elements,
  // ::cue, ::shadow), we need a universal selector to set the combinator
  // (relation) on in the cases where there are no simple selectors preceding
  // the pseudo element.
  bool is_host_pseudo = IsHostPseudoSelector(compound_selector);
  if (is_host_pseudo && !has_q_name && namespace_prefix.IsNull()) {
    return;
  }
  if (tag != AnyQName() || is_host_pseudo ||
      NeedsImplicitShadowCombinatorForMatching(compound_selector)) {
    const bool is_implicit =
        determined_prefix == g_null_atom &&
        determined_element_name == CSSSelector::UniversalSelectorAtom() &&
        !is_host_pseudo;

    output_.insert(start_index_of_compound_selector,
                   CSSSelector(tag, is_implicit));
  }
}

// If we have a compound that implicitly crosses a shadow root, rewrite it to
// have a shadow-crossing combinator (kUAShadow, which has no symbol, but let's
// call it >> for the same of the argument) instead of kSubSelector. E.g.:
//
//   video::-webkit-video-controls => video >> ::webkit-video-controls
//
// This is required because the element matching ::-webkit-video-controls is
// not the video element itself, but an element somewhere down in <video>'s
// shadow DOM tree. Note that since we store compounds right-to-left, this may
// require rearranging elements in memory (see the comment below).
void CSSSelectorParser::SplitCompoundAtImplicitShadowCrossingCombinator(
    base::span<CSSSelector> selectors) {
  // The simple selectors are stored in an array that stores
  // combinator-separated compound selectors from right-to-left. Yet, within a
  // single compound selector, stores the simple selectors from left-to-right.
  //
  // ".a.b > div#id" is stored as [div, #id, .a, .b], each element in the list
  // stored with an associated relation (combinator or SubSelector).
  //
  // ::cue, ::shadow, and custom pseudo elements have an implicit ShadowPseudo
  // combinator to their left, which really makes for a new compound selector,
  // yet it's consumed by the selector parser as a single compound selector.
  //
  // Example:
  //
  // input#x::-webkit-clear-button -> [ ::-webkit-clear-button, input, #x ]
  //
  // Likewise, ::slotted() pseudo element has an implicit ShadowSlot combinator
  // to its left for finding matching slot element in other TreeScope.
  //
  // ::part has a implicit ShadowPart combinator to its left finding the host
  // element in the scope of the style rule.
  //
  // Example:
  //
  // slot[name=foo]::slotted(div) -> [ ::slotted(div), slot, [name=foo] ]
  for (size_t i = 1; i < selectors.size(); ++i) {
    if (NeedsImplicitShadowCombinatorForMatching(selectors[i])) {
      CSSSelector::RelationType relation =
          GetImplicitShadowCombinatorForMatching(selectors[i].GetPseudoType());
      std::rotate(selectors.begin(), selectors.begin() + i, selectors.end());

      base::span<CSSSelector> remaining = selectors.first(selectors.size() - i);
      // We might need to split the compound twice, since ::placeholder is
      // allowed after ::slotted and they both need an implicit combinator for
      // matching.
      SplitCompoundAtImplicitShadowCrossingCombinator(remaining);
      remaining.back().SetRelation(relation);
      break;
    }
  }
}

namespace {

struct PseudoElementFeatureMapEntry {
  template <unsigned key_length>
  PseudoElementFeatureMapEntry(const char (&key)[key_length],
                               WebFeature feature)
      : key(key),
        key_length(base::checked_cast<uint16_t>(key_length - 1)),
        feature(base::checked_cast<uint16_t>(feature)) {}
  const char* const key;
  const uint16_t key_length;
  const uint16_t feature;
};

WebFeature FeatureForWebKitCustomPseudoElement(const AtomicString& name) {
  static const PseudoElementFeatureMapEntry feature_table[] = {
      {"cue", WebFeature::kCSSSelectorCue},
      {"-internal-media-controls-overlay-cast-button",
       WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton},
      {"-webkit-calendar-picker-indicator",
       WebFeature::kCSSSelectorWebkitCalendarPickerIndicator},
      {"-webkit-clear-button", WebFeature::kCSSSelectorWebkitClearButton},
      {"-webkit-color-swatch", WebFeature::kCSSSelectorWebkitColorSwatch},
      {"-webkit-color-swatch-wrapper",
       WebFeature::kCSSSelectorWebkitColorSwatchWrapper},
      {"-webkit-date-and-time-value",
       WebFeature::kCSSSelectorWebkitDateAndTimeValue},
      {"-webkit-datetime-edit", WebFeature::kCSSSelectorWebkitDatetimeEdit},
      {"-webkit-datetime-edit-ampm-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditAmpmField},
      {"-webkit-datetime-edit-day-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditDayField},
      {"-webkit-datetime-edit-fields-wrapper",
       WebFeature::kCSSSelectorWebkitDatetimeEditFieldsWrapper},
      {"-webkit-datetime-edit-hour-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditHourField},
      {"-webkit-datetime-edit-millisecond-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMillisecondField},
      {"-webkit-datetime-edit-minute-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMinuteField},
      {"-webkit-datetime-edit-month-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMonthField},
      {"-webkit-datetime-edit-second-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditSecondField},
      {"-webkit-datetime-edit-text",
       WebFeature::kCSSSelectorWebkitDatetimeEditText},
      {"-webkit-datetime-edit-week-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditWeekField},
      {"-webkit-datetime-edit-year-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditYearField},
      {"-webkit-file-upload-button",
       WebFeature::kCSSSelectorWebkitFileUploadButton},
      {"-webkit-inner-spin-button",
       WebFeature::kCSSSelectorWebkitInnerSpinButton},
      {"-webkit-input-placeholder",
       WebFeature::kCSSSelectorWebkitInputPlaceholder},
      {"-webkit-media-controls", WebFeature::kCSSSelectorWebkitMediaControls},
      {"-webkit-media-controls-current-time-display",
       WebFeature::kCSSSelectorWebkitMediaControlsCurrentTimeDisplay},
      {"-webkit-media-controls-enclosure",
       WebFeature::kCSSSelectorWebkitMediaControlsEnclosure},
      {"-webkit-media-controls-fullscreen-button",
       WebFeature::kCSSSelectorWebkitMediaControlsFullscreenButton},
      {"-webkit-media-controls-mute-button",
       WebFeature::kCSSSelectorWebkitMediaControlsMuteButton},
      {"-webkit-media-controls-overlay-enclosure",
       WebFeature::kCSSSelectorWebkitMediaControlsOverlayEnclosure},
      {"-webkit-media-controls-overlay-play-button",
       WebFeature::kCSSSelectorWebkitMediaControlsOverlayPlayButton},
      {"-webkit-media-controls-panel",
       WebFeature::kCSSSelectorWebkitMediaControlsPanel},
      {"-webkit-media-controls-play-button",
       WebFeature::kCSSSelectorWebkitMediaControlsPlayButton},
      {"-webkit-media-controls-timeline",
       WebFeature::kCSSSelectorWebkitMediaControlsTimeline},
      // Note: This feature is no longer implemented in Blink.
      {"-webkit-media-controls-timeline-container",
       WebFeature::kCSSSelectorWebkitMediaControlsTimelineContainer},
      {"-webkit-media-controls-time-remaining-display",
       WebFeature::kCSSSelectorWebkitMediaControlsTimeRemainingDisplay},
      {"-webkit-media-controls-toggle-closed-captions-button",
       WebFeature::kCSSSelectorWebkitMediaControlsToggleClosedCaptionsButton},
      {"-webkit-media-controls-volume-slider",
       WebFeature::kCSSSelectorWebkitMediaControlsVolumeSlider},
      {"-webkit-media-slider-container",
       WebFeature::kCSSSelectorWebkitMediaSliderContainer},
      {"-webkit-media-slider-thumb",
       WebFeature::kCSSSelectorWebkitMediaSliderThumb},
      {"-webkit-media-text-track-container",
       WebFeature::kCSSSelectorWebkitMediaTextTrackContainer},
      {"-webkit-media-text-track-display",
       WebFeature::kCSSSelectorWebkitMediaTextTrackDisplay},
      {"-webkit-media-text-track-region",
       WebFeature::kCSSSelectorWebkitMediaTextTrackRegion},
      {"-webkit-media-text-track-region-container",
       WebFeature::kCSSSelectorWebkitMediaTextTrackRegionContainer},
      {"-webkit-meter-bar", WebFeature::kCSSSelectorWebkitMeterBar},
      {"-webkit-meter-even-less-good-value",
       WebFeature::kCSSSelectorWebkitMeterEvenLessGoodValue},
      {"-webkit-meter-inner-element",
       WebFeature::kCSSSelectorWebkitMeterInnerElement},
      {"-webkit-meter-optimum-value",
       WebFeature::kCSSSelectorWebkitMeterOptimumValue},
      {"-webkit-meter-suboptimum-value",
       WebFeature::kCSSSelectorWebkitMeterSuboptimumValue},
      {"-webkit-progress-bar", WebFeature::kCSSSelectorWebkitProgressBar},
      {"-webkit-progress-inner-element",
       WebFeature::kCSSSelectorWebkitProgressInnerElement},
      {"-webkit-progress-value", WebFeature::kCSSSelectorWebkitProgressValue},
      {"-webkit-search-cancel-button",
       WebFeature::kCSSSelectorWebkitSearchCancelButton},
      {"-webkit-slider-container",
       WebFeature::kCSSSelectorWebkitSliderContainer},
      {"-webkit-slider-runnable-track",
       WebFeature::kCSSSelectorWebkitSliderRunnableTrack},
      {"-webkit-slider-thumb", WebFeature::kCSSSelectorWebkitSliderThumb},
      {"-webkit-textfield-decoration-container",
       WebFeature::kCSSSelectorWebkitTextfieldDecorationContainer},
  };
  // TODO(fs): Could use binary search once there's a less finicky way to
  // compare (order) String and StringView/non-String.
  for (const auto& entry : feature_table) {
    if (name == StringView(entry.key, entry.key_length)) {
      return static_cast<WebFeature>(entry.feature);
    }
  }
  return WebFeature::kCSSSelectorWebkitUnknownPseudo;
}

}  // namespace

static void RecordUsageAndDeprecationsOneSelector(
    const CSSSelector* selector,
    const CSSParserContext* context) {
  WebFeature feature = WebFeature::kNumberOfFeatures;
  switch (selector->GetPseudoType()) {
    case CSSSelector::kPseudoAny:
      feature = WebFeature::kCSSSelectorPseudoAny;
      break;
    case CSSSelector::kPseudoIs:
      feature = WebFeature::kCSSSelectorPseudoIs;
      break;
    case CSSSelector::kPseudoFocusVisible:
      feature = WebFeature::kCSSSelectorPseudoFocusVisible;
      break;
    case CSSSelector::kPseudoFocus:
      feature = WebFeature::kCSSSelectorPseudoFocus;
      break;
    case CSSSelector::kPseudoAnyLink:
      feature = WebFeature::kCSSSelectorPseudoAnyLink;
      break;
    case CSSSelector::kPseudoWebkitAnyLink:
      feature = WebFeature::kCSSSelectorPseudoWebkitAnyLink;
      break;
    case CSSSelector::kPseudoWhere:
      feature = WebFeature::kCSSSelectorPseudoWhere;
      break;
    case CSSSelector::kPseudoDefined:
      feature = WebFeature::kCSSSelectorPseudoDefined;
      break;
    case CSSSelector::kPseudoSlotted:
      feature = WebFeature::kCSSSelectorPseudoSlotted;
      break;
    case CSSSelector::kPseudoHost:
      feature = WebFeature::kCSSSelectorPseudoHost;
      break;
    case CSSSelector::kPseudoHostContext:
      feature = WebFeature::kCSSSelectorPseudoHostContext;
      break;
    case CSSSelector::kPseudoFullScreenAncestor:
      feature = WebFeature::kCSSSelectorPseudoFullScreenAncestor;
      break;
    case CSSSelector::kPseudoFullScreen:
      feature = WebFeature::kCSSSelectorPseudoFullScreen;
      break;
    case CSSSelector::kPseudoListBox:
      feature = WebFeature::kCSSSelectorInternalPseudoListBox;
      break;
    case CSSSelector::kPseudoWebKitCustomElement:
      feature = FeatureForWebKitCustomPseudoElement(selector->Value());
      break;
    case CSSSelector::kPseudoSpatialNavigationFocus:
      feature = WebFeature::kCSSSelectorInternalPseudoSpatialNavigationFocus;
      break;
    case CSSSelector::kPseudoReadOnly:
      feature = WebFeature::kCSSSelectorPseudoReadOnly;
      break;
    case CSSSelector::kPseudoReadWrite:
      feature = WebFeature::kCSSSelectorPseudoReadWrite;
      break;
    case CSSSelector::kPseudoDir:
      feature = WebFeature::kCSSSelectorPseudoDir;
      break;
    case CSSSelector::kPseudoHas:
      feature = WebFeature::kCSSSelectorPseudoHas;
      break;
    case CSSSelector::kPseudoState:
      feature = WebFeature::kCSSSelectorPseudoState;
      break;
    default:
      break;
  }
  if (feature != WebFeature::kNumberOfFeatures) {
    if (Deprecation::IsDeprecated(feature)) {
      context->CountDeprecation(feature);
    } else {
      context->Count(feature);
    }
  }
  if (selector->Relation() == CSSSelector::kIndirectAdjacent) {
    context->Count(WebFeature::kCSSSelectorIndirectAdjacent);
  }
  if (selector->SelectorList()) {
    for (const CSSSelector* current = selector->SelectorList()->First();
         current; current = current->NextSimpleSelector()) {
      RecordUsageAndDeprecationsOneSelector(current, context);
    }
  }
}

void CSSSelectorParser::RecordUsageAndDeprecations(
    const base::span<CSSSelector> selector_vector) {
  if (!context_->IsUseCounterRecordingEnabled()) {
    return;
  }
  if (context_->Mode() == kUASheetMode) {
    return;
  }

  for (const CSSSelector& current : selector_vector) {
    RecordUsageAndDeprecationsOneSelector(&current, context_);
  }
}

bool CSSSelectorParser::ContainsUnknownWebkitPseudoElements(
    base::span<CSSSelector> selectors) {
  for (const CSSSelector& current : selectors) {
    if (current.GetPseudoType() != CSSSelector::kPseudoWebKitCustomElement) {
      continue;
    }
    WebFeature feature = FeatureForWebKitCustomPseudoElement(current.Value());
    if (feature == WebFeature::kCSSSelectorWebkitUnknownPseudo) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
