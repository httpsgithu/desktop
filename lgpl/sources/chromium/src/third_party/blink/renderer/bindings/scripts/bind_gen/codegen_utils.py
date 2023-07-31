# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

from . import name_style
from . import style_format
from .blink_v8_bridge import blink_class_name
from .blink_v8_bridge import blink_type_info
from .code_node import CodeNode
from .code_node import EmptyNode
from .code_node import LiteralNode
from .code_node import SequenceNode
from .code_node import render_code_node
from .codegen_accumulator import CodeGenAccumulator
from .path_manager import PathManager


def make_copyright_header():
    return LiteralNode("""\
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DO NOT EDIT: This file is auto-generated by
// //third_party/blink/renderer/bindings/scripts/generate_bindings.py
//
// Use the GN flag `blink_enable_generated_code_formatting=true` to enable
// formatting of the generated files.\
""")


def make_forward_declarations(accumulator):
    assert isinstance(accumulator, CodeGenAccumulator)

    class ForwardDeclarations(object):
        def __init__(self, accumulator):
            self._accumulator = accumulator

        def __str__(self):
            return "\n".join([
                "class {};".format(class_name)
                for class_name in sorted(self._accumulator.class_decls)
            ] + [
                "struct {};".format(struct_name)
                for struct_name in sorted(self._accumulator.struct_decls)
            ])

    return LiteralNode(ForwardDeclarations(accumulator))


def make_header_include_directives(accumulator):
    assert isinstance(accumulator, CodeGenAccumulator)

    class HeaderIncludeDirectives(object):
        def __init__(self, accumulator):
            self._accumulator = accumulator

        def __str__(self):
            lines = []

            if self._accumulator.stdcpp_include_headers:
                lines.extend([
                    "#include <{}>".format(header) for header in sorted(
                        self._accumulator.stdcpp_include_headers)
                ])
                lines.append("")

            lines.extend([
                "#include \"{}\"".format(header)
                for header in sorted(self._accumulator.include_headers)
            ])

            return "\n".join(lines)

    return LiteralNode(HeaderIncludeDirectives(accumulator))


def collect_forward_decls_and_include_headers(idl_types):
    assert isinstance(idl_types, (list, tuple))
    assert all(isinstance(idl_type, web_idl.IdlType) for idl_type in idl_types)

    header_forward_decls = set()
    header_include_headers = set()
    source_forward_decls = set()
    source_include_headers = set()

    def collect(idl_type):
        if idl_type.is_any or idl_type.is_object:
            header_include_headers.add(
                "third_party/blink/renderer/bindings/core/v8/script_value.h")
        elif idl_type.is_boolean or idl_type.is_numeric:
            pass
        elif idl_type.is_bigint:
            header_include_headers.add(
                "third_party/blink/renderer/platform/bindings/bigint.h")
        elif idl_type.is_data_view:
            header_include_headers.update([
                "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h",
                "third_party/blink/renderer/core/typed_arrays/dom_data_view.h",
                "third_party/blink/renderer/platform/heap/member.h",
            ])
        elif idl_type.is_buffer_source_type:
            header_include_headers.update([
                "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h",
                "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h",
                "third_party/blink/renderer/platform/heap/member.h",
            ])
        elif idl_type.is_nullable:
            if not blink_type_info(idl_type.inner_type).has_null_value:
                header_include_headers.add("third_party/abseil-cpp/absl/types/optional.h")
        elif idl_type.is_promise:
            header_include_headers.add(
                "third_party/blink/renderer/bindings/core/v8/script_promise.h")
        elif (idl_type.is_sequence or idl_type.is_frozen_array
              or idl_type.is_record or idl_type.is_variadic):
            header_include_headers.add(
                "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
            )
        elif idl_type.is_string:
            header_include_headers.add(
                "third_party/blink/renderer/platform/wtf/text/wtf_string.h")
        elif idl_type.is_typedef:
            pass
        elif idl_type.is_void:
            pass
        elif idl_type.type_definition_object:
            type_def_obj = idl_type.type_definition_object
            if type_def_obj.is_enumeration:
                header_include_headers.add(
                    PathManager(type_def_obj).api_path(ext="h"))
            elif type_def_obj.is_interface:
                header_forward_decls.add(blink_class_name(type_def_obj))
                header_include_headers.add(
                    "third_party/blink/renderer/platform/heap/member.h")
                source_include_headers.add(
                    PathManager(type_def_obj).blink_path(ext="h"))
            else:
                header_forward_decls.add(blink_class_name(type_def_obj))
                header_include_headers.add(
                    "third_party/blink/renderer/platform/heap/member.h")
                source_include_headers.add(
                    PathManager(type_def_obj).api_path(ext="h"))
        elif idl_type.union_definition_object:
            union_def_obj = idl_type.union_definition_object
            header_forward_decls.add(blink_class_name(union_def_obj))
            header_include_headers.add(
                "third_party/blink/renderer/platform/heap/member.h")
            source_include_headers.add(
                PathManager(union_def_obj).api_path(ext="h"))
        else:
            assert False, "Unknown type: {}".format(idl_type.syntactic_form)

    for idl_type in idl_types:
        idl_type.apply_to_all_composing_elements(collect)

    return (header_forward_decls, header_include_headers, source_forward_decls,
            source_include_headers)


def component_export(component, for_testing):
    assert isinstance(component, web_idl.Component)
    assert isinstance(for_testing, bool)

    if for_testing:
        return ""
    return name_style.macro(component, "EXPORT")


def component_export_header(component, for_testing):
    assert isinstance(component, web_idl.Component)
    assert isinstance(for_testing, bool)

    if for_testing:
        return None
    if component == "core":
        return "third_party/blink/renderer/core/core_export.h"
    elif component == "modules":
        return "third_party/blink/renderer/modules/modules_export.h"
    elif component == "extensions_chromeos":
        return "third_party/blink/renderer/extensions/chromeos/extensions_chromeos_export.h"
    else:
        assert False


def enclose_with_header_guard(code_node, header_guard):
    assert isinstance(code_node, CodeNode)
    assert isinstance(header_guard, str)

    return SequenceNode([
        LiteralNode("#ifndef {}".format(header_guard)),
        LiteralNode("#define {}".format(header_guard)),
        EmptyNode(),
        code_node,
        EmptyNode(),
        LiteralNode("#endif  // {}".format(header_guard)),
    ])


def write_code_node_to_file(code_node, filepath):
    """Renders |code_node| and then write the result to |filepath|."""
    assert isinstance(code_node, CodeNode)
    assert isinstance(filepath, str)

    rendered_text = render_code_node(code_node)

    format_result = style_format.auto_format(rendered_text, filename=filepath)
    if not format_result.did_succeed:
        raise RuntimeError("Style-formatting failed: filename = {filename}\n"
                           "---- stderr ----\n"
                           "{stderr}:".format(
                               filename=format_result.filename,
                               stderr=format_result.error_message))

    web_idl.file_io.write_to_file_if_changed(
        filepath, format_result.contents.encode('utf-8'))
