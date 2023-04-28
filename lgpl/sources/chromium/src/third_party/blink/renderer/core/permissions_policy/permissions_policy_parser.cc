// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"

#include <bitset>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/origin.h"

namespace blink {
namespace {

class ParsedFeaturePolicies final
    : public GarbageCollected<ParsedFeaturePolicies>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static ParsedFeaturePolicies& From(ExecutionContext& context) {
    ParsedFeaturePolicies* policies =
        Supplement<ExecutionContext>::From<ParsedFeaturePolicies>(context);
    if (!policies) {
      policies = MakeGarbageCollected<ParsedFeaturePolicies>(context);
      Supplement<ExecutionContext>::ProvideTo(context, policies);
    }
    return *policies;
  }

  explicit ParsedFeaturePolicies(ExecutionContext& context)
      : Supplement<ExecutionContext>(context),
        policies_(static_cast<size_t>(
                      mojom::blink::PermissionsPolicyFeature::kMaxValue) +
                  1) {}

  bool Observed(mojom::blink::PermissionsPolicyFeature feature) {
    wtf_size_t feature_index = static_cast<wtf_size_t>(feature);
    if (policies_[feature_index])
      return true;
    policies_[feature_index] = true;
    return false;
  }

 private:
  // Tracks which permissions policies have already been parsed, so as not to
  // count them multiple times.
  Vector<bool> policies_;
};

const char ParsedFeaturePolicies::kSupplementName[] = "ParsedFeaturePolicies";

class FeatureObserver {
 public:
  // Returns whether the feature has been observed before or not.
  bool FeatureObserved(mojom::blink::PermissionsPolicyFeature feature);

 private:
  std::bitset<static_cast<size_t>(
                  mojom::blink::PermissionsPolicyFeature::kMaxValue) +
              1>
      features_specified_;
};

class ParsingContext {
  STACK_ALLOCATED();

 public:
  ParsingContext(PolicyParserMessageBuffer& logger,
                 scoped_refptr<const SecurityOrigin> self_origin,
                 scoped_refptr<const SecurityOrigin> src_origin,
                 const FeatureNameMap& feature_names,
                 ExecutionContext* execution_context)
      : logger_(logger),
        self_origin_(self_origin),
        src_origin_(src_origin),
        feature_names_(feature_names),
        execution_context_(execution_context) {}

  ~ParsingContext() = default;

  ParsedPermissionsPolicy ParseFeaturePolicy(const String& policy);
  ParsedPermissionsPolicy ParsePermissionsPolicy(const String& policy);
  ParsedPermissionsPolicy ParsePolicyFromNode(
      const PermissionsPolicyParser::Node& root);

 private:
  PermissionsPolicyParser::Node ParseFeaturePolicyToIR(const String& policy);
  PermissionsPolicyParser::Node ParsePermissionsPolicyToIR(
      const String& policy);

  // normally 1 char = 1 byte
  // max length to parse = 2^16 = 64 kB
  static constexpr wtf_size_t MAX_LENGTH_PARSE = 1 << 16;

  absl::optional<ParsedPermissionsPolicyDeclaration> ParseFeature(
      const PermissionsPolicyParser::Declaration& declaration_node,
      const OriginWithPossibleWildcards::NodeType type);

  struct ParsedAllowlist {
    std::vector<blink::OriginWithPossibleWildcards> allowed_origins
        ALLOW_DISCOURAGED_TYPE("Permission policy uses STL for code sharing");
    bool matches_all_origins{false};
    bool matches_opaque_src{false};

    ParsedAllowlist() : allowed_origins({}) {}
  };

  absl::optional<mojom::blink::PermissionsPolicyFeature> ParseFeatureName(
      const String& feature_name);

  // Parse allowlist for feature.
  ParsedAllowlist ParseAllowlist(
      const Vector<String>& origin_strings,
      const OriginWithPossibleWildcards::NodeType type);

  void ReportFeatureUsage(mojom::blink::PermissionsPolicyFeature feature);
  void ReportFeatureUsageLegacy(mojom::blink::PermissionsPolicyFeature feature);

  // This function should be called after Allowlist Histograms related flags
  // have been captured.
  void RecordAllowlistTypeUsage(size_t origin_count);
  // The use of various allowlist types should only be recorded once per page.
  // For simplicity, this recording assumes that the ParseHeader method is
  // called once when creating a new document, and similarly the ParseAttribute
  // method is called once for a frame. It is possible for multiple calls, but
  // the additional complexity to guarantee only one record isn't warranted as
  // yet.
  void ReportAllowlistTypeUsage();

  PolicyParserMessageBuffer& logger_;
  scoped_refptr<const SecurityOrigin> self_origin_;
  scoped_refptr<const SecurityOrigin> src_origin_;
  const FeatureNameMap& feature_names_;
  // `execution_context_` is used for reporting various WebFeatures
  // during the parsing process.
  // `execution_context_` should only be `nullptr` in tests.
  ExecutionContext* execution_context_;

  // Flags for the types of items which can be used in allowlists.
  bool allowlist_includes_star_ = false;
  bool allowlist_includes_self_ = false;
  bool allowlist_includes_src_ = false;
  bool allowlist_includes_none_ = false;
  bool allowlist_includes_origin_ = false;

  HashSet<FeaturePolicyAllowlistType> allowlist_types_used_;

  FeatureObserver feature_observer_;
};

bool FeatureObserver::FeatureObserved(
    mojom::blink::PermissionsPolicyFeature feature) {
  if (features_specified_[static_cast<size_t>(feature)]) {
    return true;
  } else {
    features_specified_.set(static_cast<size_t>(feature));
    return false;
  }
}

// TODO: Remove this function once we verified the new histogram counts
// are consistent with old ones.
void ParsingContext::ReportFeatureUsageLegacy(
    mojom::blink::PermissionsPolicyFeature feature) {
  if (src_origin_) {
    if (!execution_context_ ||
        !ParsedFeaturePolicies::From(*execution_context_).Observed(feature)) {
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Allow",
                                feature);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Header", feature);
  }
}

void ParsingContext::ReportFeatureUsage(
    mojom::blink::PermissionsPolicyFeature feature) {
  if (!execution_context_ || !execution_context_->IsWindow())
    return;

  LocalDOMWindow* local_dom_window = To<LocalDOMWindow>(execution_context_);

  auto usage_type =
      src_origin_ ? UseCounterImpl::PermissionsPolicyUsageType::kIframeAttribute
                  : UseCounterImpl::PermissionsPolicyUsageType::kHeader;

  local_dom_window->CountPermissionsPolicyUsage(feature, usage_type);
}

void ParsingContext::RecordAllowlistTypeUsage(size_t origin_count) {
  // Record the type of allowlist used.
  if (origin_count == 0) {
    allowlist_types_used_.insert(FeaturePolicyAllowlistType::kEmpty);
  } else if (origin_count == 1) {
    if (allowlist_includes_star_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kStar);
    else if (allowlist_includes_self_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kSelf);
    else if (allowlist_includes_src_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kSrc);
    else if (allowlist_includes_none_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kNone);
    else
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kOrigins);
  } else {
    if (allowlist_includes_origin_) {
      if (allowlist_includes_star_ || allowlist_includes_none_ ||
          allowlist_includes_src_ || allowlist_includes_self_)
        allowlist_types_used_.insert(FeaturePolicyAllowlistType::kMixed);
      else
        allowlist_types_used_.insert(FeaturePolicyAllowlistType::kOrigins);
    } else {
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kKeywordsOnly);
    }
  }
  // Reset all flags.
  allowlist_includes_star_ = false;
  allowlist_includes_self_ = false;
  allowlist_includes_src_ = false;
  allowlist_includes_none_ = false;
  allowlist_includes_origin_ = false;
}

void ParsingContext::ReportAllowlistTypeUsage() {
  for (const FeaturePolicyAllowlistType allowlist_type :
       allowlist_types_used_) {
    if (src_origin_) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.AttributeAllowlistType",
          allowlist_type);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.HeaderAllowlistType", allowlist_type);
    }
  }
}

absl::optional<mojom::blink::PermissionsPolicyFeature>
ParsingContext::ParseFeatureName(const String& feature_name) {
  DCHECK(!feature_name.empty());
  // window-management is an alias for window-placement (crbug.com/1328581).
  // Track usage of the alias used.
  if (feature_name == "window-placement") {
    UseCounter::Count(this->execution_context_,
                      WebFeature::kWindowPlacementPermissionPolicyParsed);
  }
  if (feature_name == "window-management") {
    UseCounter::Count(this->execution_context_,
                      WebFeature::kWindowManagementPermissionPolicyParsed);
  }
  const String& effective_feature_name =
      (feature_name == "window-management" &&
       RuntimeEnabledFeatures::WindowManagementPermissionAliasEnabled())
          ? "window-placement"
          : feature_name;
  if (!feature_names_.Contains(effective_feature_name)) {
    logger_.Warn("Unrecognized feature: '" + effective_feature_name + "'.");
    return absl::nullopt;
  }
  if (DisabledByOriginTrial(effective_feature_name, execution_context_)) {
    logger_.Warn("Origin trial controlled feature not enabled: '" +
                 effective_feature_name + "'.");
    return absl::nullopt;
  }
  mojom::blink::PermissionsPolicyFeature feature =
      feature_names_.at(effective_feature_name);

  // TODO(https://crbug.com/1324111): Remove this after OT.
  if (feature == mojom::blink::PermissionsPolicyFeature::kUnload) {
    if (!execution_context_ ||
        !RuntimeEnabledFeatures::PermissionsPolicyUnloadEnabled(
            execution_context_)) {
      // kUnload should not be recognised unless the OT is enabled.
      feature = mojom::blink::PermissionsPolicyFeature::kNotFound;
    } else if (execution_context_->IsWindow()) {
      // Counter is required for Origin Trial.
      execution_context_->CountUse(WebFeature::kPermissionsPolicyUnload);
    }
  }
  return feature;
}

ParsingContext::ParsedAllowlist ParsingContext::ParseAllowlist(
    const Vector<String>& origin_strings,
    const OriginWithPossibleWildcards::NodeType type) {
  // The source of the PermissionsPolicyParser::Node must have an explicit
  // source so that we know which wildcards can be enabled.
  DCHECK_NE(OriginWithPossibleWildcards::NodeType::kUnknown, type);
  ParsedAllowlist allowlist;
  if (origin_strings.empty()) {
    // If a policy entry has no listed origins (e.g. "feature_name1" in
    // allow="feature_name1; feature_name2 value"), enable the feature for:
    //     a. |self_origin|, if we are parsing a header policy (i.e.,
    //       |src_origin| is null);
    //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
    //       |src_origin| is not null), |src_origin| is not opaque; or
    //     c. the opaque origin of the frame, if |src_origin| is opaque.
    if (!src_origin_) {
      allowlist.allowed_origins.emplace_back(self_origin_->ToUrlOrigin(),
                                             /*has_subdomain_wildcard=*/false);
    } else if (!src_origin_->IsOpaque()) {
      allowlist.allowed_origins.emplace_back(src_origin_->ToUrlOrigin(),
                                             /*has_subdomain_wildcard=*/false);
    } else {
      allowlist.matches_opaque_src = true;
    }
  } else {
    for (const String& origin_string : origin_strings) {
      DCHECK(!origin_string.empty());

      if (!origin_string.ContainsOnlyASCIIOrEmpty()) {
        logger_.Warn("Non-ASCII characters in origin.");
        continue;
      }

      // Determine the target of the declaration. This may be a specific
      // origin, either explicitly written, or one of the special keywords
      // 'self' or 'src'. ('src' can only be used in the iframe allow
      // attribute.) Also determine if this target has a subdomain wildcard
      // (e.g., https://*.google.com).
      OriginWithPossibleWildcards origin_with_possible_wildcards;

      // If the iframe will have an opaque origin (for example, if it is
      // sandboxed, or has a data: URL), then 'src' needs to refer to the
      // opaque origin of the frame, which is not known yet. In this case,
      // the |matches_opaque_src| flag on the declaration is set, rather than
      // adding an origin to the allowlist.
      bool target_is_opaque = false;
      bool target_is_all = false;

      // 'self' origin is used if the origin is exactly 'self'.
      if (EqualIgnoringASCIICase(origin_string, "'self'")) {
        allowlist_includes_self_ = true;
        origin_with_possible_wildcards =
            OriginWithPossibleWildcards(self_origin_->ToUrlOrigin(),
                                        /*has_subdomain_wildcard=*/false);
      }
      // 'src' origin is used if |src_origin| is available and the
      // origin is a match for 'src'. |src_origin| is only set
      // when parsing an iframe allow attribute.
      else if (src_origin_ && EqualIgnoringASCIICase(origin_string, "'src'")) {
        allowlist_includes_src_ = true;
        if (!src_origin_->IsOpaque()) {
          origin_with_possible_wildcards =
              OriginWithPossibleWildcards(src_origin_->ToUrlOrigin(),
                                          /*has_subdomain_wildcard=*/false);
        } else {
          target_is_opaque = true;
        }
      } else if (EqualIgnoringASCIICase(origin_string, "'none'")) {
        allowlist_includes_none_ = true;
        continue;
      } else if (origin_string == "*") {
        allowlist_includes_star_ = true;
        target_is_all = true;
      }
      // Otherwise, parse the origin string and verify that the result is
      // valid. Invalid strings will produce an opaque origin, which will
      // result in an error message.
      else {
        origin_with_possible_wildcards =
            OriginWithPossibleWildcards::Parse(origin_string.Utf8(), type);
        if (!origin_with_possible_wildcards.origin.opaque()) {
          allowlist_includes_origin_ = true;
        } else {
          logger_.Warn("Unrecognized origin: '" + origin_string + "'.");
          continue;
        }
      }

      if (target_is_all) {
        allowlist.matches_all_origins = true;
        allowlist.matches_opaque_src = true;
      } else if (target_is_opaque) {
        allowlist.matches_opaque_src = true;
      } else {
        allowlist.allowed_origins.emplace_back(origin_with_possible_wildcards);
      }
    }
  }

  // Size reduction: remove all items in the allowlist if target is all.
  if (allowlist.matches_all_origins)
    allowlist.allowed_origins.clear();

  // Sort |allowed_origins| in alphabetical order.
  std::sort(allowlist.allowed_origins.begin(), allowlist.allowed_origins.end());

  RecordAllowlistTypeUsage(origin_strings.size());

  return allowlist;
}

absl::optional<ParsedPermissionsPolicyDeclaration> ParsingContext::ParseFeature(
    const PermissionsPolicyParser::Declaration& declaration_node,
    const OriginWithPossibleWildcards::NodeType type) {
  absl::optional<mojom::blink::PermissionsPolicyFeature> feature =
      ParseFeatureName(declaration_node.feature_name);
  if (!feature)
    return absl::nullopt;

  ParsedAllowlist parsed_allowlist =
      ParseAllowlist(declaration_node.allowlist, type);

  // If same feature appeared more than once, only the first one counts.
  if (feature_observer_.FeatureObserved(*feature))
    return absl::nullopt;

  ParsedPermissionsPolicyDeclaration parsed_feature(*feature);
  parsed_feature.allowed_origins = std::move(parsed_allowlist.allowed_origins);
  parsed_feature.matches_all_origins = parsed_allowlist.matches_all_origins;
  parsed_feature.matches_opaque_src = parsed_allowlist.matches_opaque_src;

  return parsed_feature;
}

ParsedPermissionsPolicy ParsingContext::ParseFeaturePolicy(
    const String& policy) {
  return ParsePolicyFromNode(ParseFeaturePolicyToIR(policy));
}

ParsedPermissionsPolicy ParsingContext::ParsePermissionsPolicy(
    const String& policy) {
  return ParsePolicyFromNode(ParsePermissionsPolicyToIR(policy));
}

ParsedPermissionsPolicy ParsingContext::ParsePolicyFromNode(
    const PermissionsPolicyParser::Node& root) {
  ParsedPermissionsPolicy parsed_policy;
  for (const PermissionsPolicyParser::Declaration& declaration_node :
       root.declarations) {
    absl::optional<ParsedPermissionsPolicyDeclaration> parsed_feature =
        ParseFeature(declaration_node, root.type);
    if (parsed_feature) {
      ReportFeatureUsage(parsed_feature->feature);
      ReportFeatureUsageLegacy(parsed_feature->feature);
      parsed_policy.push_back(*parsed_feature);
    }
  }
  ReportAllowlistTypeUsage();
  return parsed_policy;
}

PermissionsPolicyParser::Node ParsingContext::ParseFeaturePolicyToIR(
    const String& policy) {
  PermissionsPolicyParser::Node root{
      OriginWithPossibleWildcards::NodeType::kAttribute};

  if (policy.length() > MAX_LENGTH_PARSE) {
    logger_.Error("Feature policy declaration exceeds size limit(" +
                  String::Number(policy.length()) + ">" +
                  String::Number(MAX_LENGTH_PARSE) + ")");
    return {};
  }

  Vector<String> policy_items;

  if (src_origin_) {
    // Attribute parsing.
    policy_items.push_back(policy);
  } else {
    // Header parsing.
    // RFC2616, section 4.2 specifies that headers appearing multiple times can
    // be combined with a comma. Walk the header string, and parse each comma
    // separated chunk as a separate header.
    // policy_items = [ policy *( "," [ policy ] ) ]
    policy.Split(',', policy_items);
  }

  if (policy_items.size() > 1) {
    UseCounter::Count(
        execution_context_,
        mojom::blink::WebFeature::kFeaturePolicyCommaSeparatedDeclarations);
  }

  for (const String& item : policy_items) {
    Vector<String> feature_entries;
    // feature_entries = [ feature_entry *( ";" [ feature_entry ] ) ]
    item.Split(';', feature_entries);

    if (feature_entries.size() > 1) {
      UseCounter::Count(execution_context_,
                        mojom::blink::WebFeature::
                            kFeaturePolicySemicolonSeparatedDeclarations);
    }

    for (const String& feature_entry : feature_entries) {
      Vector<String> tokens;
      feature_entry.Split(' ', tokens);

      if (tokens.empty())
        continue;

      PermissionsPolicyParser::Declaration declaration_node;
      // Break tokens into head & tail, where
      // head = feature_name
      // tail = allowlist
      // After feature_name has been set, take tail of tokens vector by
      // erasing the first element.
      declaration_node.feature_name = std::move(tokens.front());
      tokens.erase(tokens.begin());
      declaration_node.allowlist = std::move(tokens);
      root.declarations.push_back(declaration_node);
    }
  }

  return root;
}

PermissionsPolicyParser::Node ParsingContext::ParsePermissionsPolicyToIR(
    const String& policy) {
  if (policy.length() > MAX_LENGTH_PARSE) {
    logger_.Error("Permissions policy declaration exceeds size limit(" +
                  String::Number(policy.length()) + ">" +
                  String::Number(MAX_LENGTH_PARSE) + ")");
    return {};
  }

  auto root = net::structured_headers::ParseDictionary(policy.Utf8());
  if (!root) {
    logger_.Error(
        "Parse of permissions policy failed because of errors reported by "
        "structured header parser.");
    return {};
  }

  PermissionsPolicyParser::Node ir_root{
      OriginWithPossibleWildcards::NodeType::kHeader};
  for (const auto& feature_entry : root.value()) {
    const auto& key = feature_entry.first;
    const char* feature_name = key.c_str();
    const auto& value = feature_entry.second;

    if (!value.params.empty()) {
      logger_.Warn(
          String::Format("Feature %s's parameters are ignored.", feature_name));
    }

    Vector<String> allowlist;
    for (const auto& parameterized_item : value.member) {
      if (!parameterized_item.params.empty()) {
        logger_.Warn(String::Format("Feature %s's parameters are ignored.",
                                    feature_name));
      }

      String allowlist_item;
      if (parameterized_item.item.is_token()) {
        // All special keyword appears as token, i.e. self, src and *.
        const std::string& token_value = parameterized_item.item.GetString();
        if (token_value != "*" && token_value != "self") {
          logger_.Warn(String::Format(
              "Invalid allowlist item(%s) for feature %s. Allowlist item "
              "must be *, self or quoted url.",
              token_value.c_str(), feature_name));
          continue;
        }

        if (token_value == "*") {
          allowlist_item = "*";
        } else {
          allowlist_item = String::Format("'%s'", token_value.c_str());
        }
      } else if (parameterized_item.item.is_string()) {
        allowlist_item = parameterized_item.item.GetString().c_str();
      } else {
        logger_.Warn(
            String::Format("Invalid allowlist item for feature %s. Allowlist "
                           "item must be *, self, or quoted url.",
                           feature_name));
        continue;
      }
      if (!allowlist_item.empty())
        allowlist.push_back(allowlist_item);
    }

    if (allowlist.empty())
      allowlist.push_back("'none'");

    ir_root.declarations.push_back(
        PermissionsPolicyParser::Declaration{feature_name, allowlist});
  }

  return ir_root;
}

}  // namespace

ParsedPermissionsPolicy PermissionsPolicyParser::ParseHeader(
    const String& feature_policy_header,
    const String& permissions_policy_header,
    scoped_refptr<const SecurityOrigin> origin,
    PolicyParserMessageBuffer& feature_policy_logger,
    PolicyParserMessageBuffer& permissions_policy_logger,
    ExecutionContext* execution_context) {
  ParsedPermissionsPolicy permissions_policy =
      ParsingContext(permissions_policy_logger, origin, nullptr,
                     GetDefaultFeatureNameMap(), execution_context)
          .ParsePermissionsPolicy(permissions_policy_header);
  ParsedPermissionsPolicy feature_policy =
      ParsingContext(feature_policy_logger, origin, nullptr,
                     GetDefaultFeatureNameMap(), execution_context)
          .ParseFeaturePolicy(feature_policy_header);

  FeatureObserver observer;
  for (const auto& policy_declaration : permissions_policy) {
    bool feature_observed =
        observer.FeatureObserved(policy_declaration.feature);
    DCHECK(!feature_observed);
  }

  std::vector<std::string> overlap_features;

  for (const auto& policy_declaration : feature_policy) {
    if (!observer.FeatureObserved(policy_declaration.feature)) {
      permissions_policy.push_back(policy_declaration);
    } else {
      overlap_features.push_back(
          GetNameForFeature(policy_declaration.feature).Ascii().c_str());
    }
  }

  if (!overlap_features.empty()) {
    std::ostringstream features_stream;
    std::copy(overlap_features.begin(), overlap_features.end() - 1,
              std::ostream_iterator<std::string>(features_stream, ", "));
    features_stream << overlap_features.back();

    feature_policy_logger.Warn(String::Format(
        "Some features are specified in both Feature-Policy and "
        "Permissions-Policy header: %s. Values defined in Permissions-Policy "
        "header will be used.",
        features_stream.str().c_str()));
  }
  return permissions_policy;
}

ParsedPermissionsPolicy PermissionsPolicyParser::ParseAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    PolicyParserMessageBuffer& logger,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, self_origin, src_origin,
                        GetDefaultFeatureNameMap(), execution_context)
      .ParseFeaturePolicy(policy);
}

ParsedPermissionsPolicy PermissionsPolicyParser::ParsePolicyFromNode(
    PermissionsPolicyParser::Node& policy,
    scoped_refptr<const SecurityOrigin> origin,
    PolicyParserMessageBuffer& logger,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, origin, /*src_origin=*/nullptr,
                        GetDefaultFeatureNameMap(), execution_context)
      .ParsePolicyFromNode(policy);
}

ParsedPermissionsPolicy PermissionsPolicyParser::ParseFeaturePolicyForTest(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    PolicyParserMessageBuffer& logger,
    const FeatureNameMap& feature_names,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, self_origin, src_origin, feature_names,
                        execution_context)
      .ParseFeaturePolicy(policy);
}

ParsedPermissionsPolicy PermissionsPolicyParser::ParsePermissionsPolicyForTest(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    PolicyParserMessageBuffer& logger,
    const FeatureNameMap& feature_names,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, self_origin, src_origin, feature_names,
                        execution_context)
      .ParsePermissionsPolicy(policy);
}

bool IsFeatureDeclared(mojom::blink::PermissionsPolicyFeature feature,
                       const ParsedPermissionsPolicy& policy) {
  return base::Contains(policy, feature,
                        &ParsedPermissionsPolicyDeclaration::feature);
}

bool RemoveFeatureIfPresent(mojom::blink::PermissionsPolicyFeature feature,
                            ParsedPermissionsPolicy& policy) {
  auto new_end = std::remove_if(policy.begin(), policy.end(),
                                [feature](const auto& declaration) {
                                  return declaration.feature == feature;
                                });
  if (new_end == policy.end())
    return false;
  policy.erase(new_end, policy.end());
  return true;
}

bool DisallowFeatureIfNotPresent(mojom::blink::PermissionsPolicyFeature feature,
                                 ParsedPermissionsPolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedPermissionsPolicyDeclaration allowlist(feature);
  policy.push_back(allowlist);
  return true;
}

bool AllowFeatureEverywhereIfNotPresent(
    mojom::blink::PermissionsPolicyFeature feature,
    ParsedPermissionsPolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedPermissionsPolicyDeclaration allowlist(feature);
  allowlist.matches_all_origins = true;
  allowlist.matches_opaque_src = true;
  policy.push_back(allowlist);
  return true;
}

void DisallowFeature(mojom::blink::PermissionsPolicyFeature feature,
                     ParsedPermissionsPolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  DisallowFeatureIfNotPresent(feature, policy);
}

bool IsFeatureForMeasurementOnly(
    mojom::blink::PermissionsPolicyFeature feature) {
  return feature == mojom::blink::PermissionsPolicyFeature::kWebShare;
}

void AllowFeatureEverywhere(mojom::blink::PermissionsPolicyFeature feature,
                            ParsedPermissionsPolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  AllowFeatureEverywhereIfNotPresent(feature, policy);
}

const Vector<String> GetAvailableFeatures(ExecutionContext* execution_context) {
  Vector<String> available_features;
  for (const auto& feature : GetDefaultFeatureNameMap()) {
    if (!DisabledByOriginTrial(feature.key, execution_context) &&
        !IsFeatureForMeasurementOnly(feature.value)) {
      available_features.push_back(feature.key);
    }
  }
  return available_features;
}

const String& GetNameForFeature(
    mojom::blink::PermissionsPolicyFeature feature) {
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (entry.value == feature)
      return entry.key;
  }
  return g_empty_string;
}

}  // namespace blink
