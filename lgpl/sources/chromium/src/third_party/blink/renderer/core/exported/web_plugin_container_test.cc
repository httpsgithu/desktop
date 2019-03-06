/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_plugin_container.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/exported/fake_web_plugin.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using blink::test::RunPendingTasks;

namespace blink {

class WebPluginContainerTest : public testing::Test {
 public:
  WebPluginContainerTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void CalculateGeometry(WebPluginContainerImpl* plugin_container_impl,
                         IntRect& window_rect,
                         IntRect& clip_rect,
                         IntRect& unobscured_rect) {
    plugin_container_impl->CalculateGeometry(window_rect, clip_rect,
                                             unobscured_rect);
  }

  void RegisterMockedURL(
      const std::string& file_name,
      const std::string& mime_type = std::string("text/html")) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name), WebString::FromUTF8(mime_type));
  }

 protected:
  ScopedFakePluginRegistry fake_plugins_;
  std::string base_url_;
};

namespace {

#if defined(OS_MACOSX)
const WebInputEvent::Modifiers kEditingModifier = WebInputEvent::kMetaKey;
#else
const WebInputEvent::Modifiers kEditingModifier = WebInputEvent::kControlKey;
#endif

template <typename T>
class CustomPluginWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  WebPlugin* CreatePlugin(const WebPluginParams& params) override {
    return new T(params);
  }
};

class TestPluginWebFrameClient;

// Subclass of FakeWebPlugin that has a selection of 'x' as plain text and 'y'
// as markup text.
class TestPlugin : public FakeWebPlugin {
 public:
  TestPlugin(const WebPluginParams& params,
             TestPluginWebFrameClient* test_client)
      : FakeWebPlugin(params), test_client_(test_client) {}

  bool HasSelection() const override { return true; }
  WebString SelectionAsText() const override { return WebString("x"); }
  WebString SelectionAsMarkup() const override { return WebString("y"); }
  bool SupportsPaginatedPrint() override { return true; }
  int PrintBegin(const WebPrintParams& print_params) override { return 1; }
  void PrintPage(int page_number, cc::PaintCanvas*) override;

 private:
  ~TestPlugin() override = default;

  TestPluginWebFrameClient* const test_client_;
};

// Subclass of FakeWebPlugin used for testing edit commands, so HasSelection()
// and CanEditText() return true by default.
class TestPluginWithEditableText : public FakeWebPlugin {
 public:
  static TestPluginWithEditableText* FromContainer(WebElement* element) {
    WebPlugin* plugin =
        ToWebPluginContainerImpl(element->PluginContainer())->Plugin();
    return static_cast<TestPluginWithEditableText*>(plugin);
  }

  explicit TestPluginWithEditableText(const WebPluginParams& params)
      : FakeWebPlugin(params), cut_called_(false), paste_called_(false) {}

  bool HasSelection() const override { return true; }
  bool CanEditText() const override { return true; }
  bool ExecuteEditCommand(const WebString& name) override {
    return ExecuteEditCommand(name, WebString());
  }
  bool ExecuteEditCommand(const WebString& name,
                          const WebString& value) override {
    if (name == "Cut") {
      cut_called_ = true;
      return true;
    }
    if (name == "Paste" || name == "PasteAndMatchStyle") {
      paste_called_ = true;
      return true;
    }
    return false;
  }

  bool IsCutCalled() const { return cut_called_; }
  bool IsPasteCalled() const { return paste_called_; }
  void ResetEditCommandState() {
    cut_called_ = false;
    paste_called_ = false;
  }

 private:
  ~TestPluginWithEditableText() override = default;

  bool cut_called_;
  bool paste_called_;
};

class TestPluginWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
  WebLocalFrame* CreateChildFrame(WebLocalFrame* parent,
                                  WebTreeScopeType scope,
                                  const WebString& name,
                                  const WebString& fallback_name,
                                  WebSandboxFlags sandbox_flags,
                                  const ParsedFeaturePolicy& container_policy,
                                  const WebFrameOwnerProperties&) override {
    return CreateLocalChild(*parent, scope,
                            std::make_unique<TestPluginWebFrameClient>());
  }

  WebPlugin* CreatePlugin(const WebPluginParams& params) override {
    if (params.mime_type == "application/x-webkit-test-webplugin" ||
        params.mime_type == "application/pdf") {
      if (has_editable_text_)
        return new TestPluginWithEditableText(params);

      return new TestPlugin(params, this);
    }
    return WebLocalFrameClient::CreatePlugin(params);
  }

 public:
  void OnPrintPage() { printed_page_ = true; }
  bool PrintedAtLeastOnePage() const { return printed_page_; }
  void SetHasEditableText(bool has_editable_text) {
    has_editable_text_ = has_editable_text;
  }

 private:
  bool printed_page_ = false;
  bool has_editable_text_ = false;
};

void TestPlugin::PrintPage(int page_number, cc::PaintCanvas* canvas) {
  DCHECK(test_client_);
  test_client_->OnPrintPage();
}

void EnablePlugins(WebView* web_view, const WebSize& size) {
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->Resize(size);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();
}

WebPluginContainer* GetWebPluginContainer(WebViewImpl* web_view,
                                          const WebString& id) {
  WebElement element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(id);
  return element.PluginContainer();
}

String ReadClipboard() {
  // Run all tasks in a message loop to allow asynchronous clipboard writing
  // to happen before reading from it synchronously.
  test::RunPendingTasks();
  return SystemClipboard::GetInstance().ReadPlainText();
}

void ClearClipboardBuffer() {
  SystemClipboard::GetInstance().WritePlainText(String(""));
  EXPECT_EQ(String(""), ReadClipboard());
}

void CreateAndHandleKeyboardEvent(WebElement* plugin_container_one_element,
                                  WebInputEvent::Modifiers modifier_key,
                                  int key_code) {
  WebKeyboardEvent web_keyboard_event(
      WebInputEvent::kRawKeyDown, modifier_key,
      WebInputEvent::GetStaticTimeStampForTests());
  web_keyboard_event.windows_key_code = key_code;
  KeyboardEvent* key_event = KeyboardEvent::Create(web_keyboard_event, nullptr);
  ToWebPluginContainerImpl(plugin_container_one_element->PluginContainer())
      ->HandleEvent(*key_event);
}

void ExecuteContextMenuCommand(WebViewImpl* web_view,
                               const WebString& command_name) {
  auto event = FrameTestHelpers::CreateMouseEvent(WebMouseEvent::kMouseDown,
                                                  WebMouseEvent::Button::kRight,
                                                  WebPoint(30, 30), 0);
  event.click_count = 1;

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  EXPECT_TRUE(
      web_view->MainFrame()->ToWebLocalFrame()->ExecuteCommand(command_name));
}

}  // namespace

TEST_F(WebPluginContainerTest, WindowToLocalPointTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebPluginContainer* plugin_container_one =
      GetWebPluginContainer(web_view, WebString::FromUTF8("translated-plugin"));
  DCHECK(plugin_container_one);
  WebPoint point1 =
      plugin_container_one->RootFrameToLocalPoint(WebPoint(10, 10));
  ASSERT_EQ(0, point1.x);
  ASSERT_EQ(0, point1.y);
  WebPoint point2 =
      plugin_container_one->RootFrameToLocalPoint(WebPoint(100, 100));
  ASSERT_EQ(90, point2.x);
  ASSERT_EQ(90, point2.y);

  WebPluginContainer* plugin_container_two =
      GetWebPluginContainer(web_view, WebString::FromUTF8("rotated-plugin"));
  DCHECK(plugin_container_two);
  WebPoint point3 =
      plugin_container_two->RootFrameToLocalPoint(WebPoint(0, 10));
  ASSERT_EQ(10, point3.x);
  ASSERT_EQ(0, point3.y);
  WebPoint point4 =
      plugin_container_two->RootFrameToLocalPoint(WebPoint(-10, 10));
  ASSERT_EQ(10, point4.x);
  ASSERT_EQ(10, point4.y);
}

TEST_F(WebPluginContainerTest, PluginDocumentPluginIsFocused) {
  RegisterMockedURL("test.pdf", "application/pdf");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "test.pdf", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->UpdateAllLifecyclePhases();

  WebDocument document = web_view->MainFrameImpl()->GetDocument();
  EXPECT_TRUE(document.IsPluginDocument());
  WebPluginContainer* plugin_container =
      GetWebPluginContainer(web_view, "plugin");
  EXPECT_EQ(document.FocusedElement(), plugin_container->GetElement());
}

TEST_F(WebPluginContainerTest, IFramePluginDocumentNotFocused) {
  RegisterMockedURL("test.pdf", "application/pdf");
  RegisterMockedURL("iframe_pdf.html", "text/html");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "iframe_pdf.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->UpdateAllLifecyclePhases();

  WebDocument document = web_view->MainFrameImpl()->GetDocument();
  WebLocalFrame* iframe =
      web_view->MainFrame()->FirstChild()->ToWebLocalFrame();
  EXPECT_TRUE(iframe->GetDocument().IsPluginDocument());
  WebPluginContainer* plugin_container =
      iframe->GetDocument().GetElementById("plugin").PluginContainer();
  EXPECT_NE(document.FocusedElement(), plugin_container->GetElement());
  EXPECT_NE(iframe->GetDocument().FocusedElement(),
            plugin_container->GetElement());
}

TEST_F(WebPluginContainerTest, PrintOnePage) {
  RegisterMockedURL("test.pdf", "application/pdf");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "test.pdf", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();
  WebLocalFrame* frame = web_view->MainFrameImpl();

  WebPrintParams print_params;
  print_params.print_content_area.width = 500;
  print_params.print_content_area.height = 500;

  frame->PrintBegin(print_params);
  PaintRecorder recorder;
  frame->PrintPage(0, recorder.beginRecording(IntRect()));
  frame->PrintEnd();
  DCHECK(plugin_web_frame_client.PrintedAtLeastOnePage());
}

TEST_F(WebPluginContainerTest, PrintAllPages) {
  RegisterMockedURL("test.pdf", "application/pdf");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "test.pdf", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();
  WebLocalFrame* frame = web_view->MainFrameImpl();

  WebPrintParams print_params;
  print_params.print_content_area.width = 500;
  print_params.print_content_area.height = 500;

  frame->PrintBegin(print_params);
  PaintRecorder recorder;
  frame->PrintPagesForTesting(recorder.beginRecording(IntRect()), WebSize());
  frame->PrintEnd();
  DCHECK(plugin_web_frame_client.PrintedAtLeastOnePage());
}

TEST_F(WebPluginContainerTest, LocalToWindowPointTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebPluginContainer* plugin_container_one =
      GetWebPluginContainer(web_view, WebString::FromUTF8("translated-plugin"));
  DCHECK(plugin_container_one);
  WebPoint point1 = plugin_container_one->LocalToRootFramePoint(WebPoint(0, 0));
  ASSERT_EQ(10, point1.x);
  ASSERT_EQ(10, point1.y);
  WebPoint point2 =
      plugin_container_one->LocalToRootFramePoint(WebPoint(90, 90));
  ASSERT_EQ(100, point2.x);
  ASSERT_EQ(100, point2.y);

  WebPluginContainer* plugin_container_two =
      GetWebPluginContainer(web_view, WebString::FromUTF8("rotated-plugin"));
  DCHECK(plugin_container_two);
  WebPoint point3 =
      plugin_container_two->LocalToRootFramePoint(WebPoint(10, 0));
  ASSERT_EQ(0, point3.x);
  ASSERT_EQ(10, point3.y);
  WebPoint point4 =
      plugin_container_two->LocalToRootFramePoint(WebPoint(10, 10));
  ASSERT_EQ(-10, point4.x);
  ASSERT_EQ(10, point4.y);
}

// Verifies executing the command 'Copy' results in copying to the clipboard.
TEST_F(WebPluginContainerTest, Copy) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  web_view->MainFrameImpl()
      ->GetDocument()
      .Unwrap<Document>()
      ->body()
      ->getElementById("translated-plugin")
      ->focus();
  EXPECT_TRUE(web_view->MainFrame()->ToWebLocalFrame()->ExecuteCommand("Copy"));
  EXPECT_EQ(String("x"), ReadClipboard());
  ClearClipboardBuffer();
}

TEST_F(WebPluginContainerTest, CopyFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  // Make sure the right-click + command works in common scenario.
  ExecuteContextMenuCommand(web_view, "Copy");
  EXPECT_EQ(String("x"), ReadClipboard());
  ClearClipboardBuffer();

  auto event = FrameTestHelpers::CreateMouseEvent(WebMouseEvent::kMouseDown,
                                                  WebMouseEvent::Button::kRight,
                                                  WebPoint(30, 30), 0);
  event.click_count = 1;

  // Now, let's try a more complex scenario:
  // 1) open the context menu. This will focus the plugin.
  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  // 2) document blurs the plugin, because it can.
  web_view->ClearFocusedElement();
  // 3) Copy should still operate on the context node, even though the focus had
  //    shifted.
  EXPECT_TRUE(web_view->MainFrameImpl()->ExecuteCommand("Copy"));
  EXPECT_EQ(String("x"), ReadClipboard());
  ClearClipboardBuffer();
}

// Verifies |Ctrl-C| and |Ctrl-Insert| keyboard events, results in copying to
// the clipboard.
TEST_F(WebPluginContainerTest, CopyInsertKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_C);
  EXPECT_EQ(String("x"), ReadClipboard());
  ClearClipboardBuffer();

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_INSERT);
  EXPECT_EQ(String("x"), ReadClipboard());
  ClearClipboardBuffer();
}

// Verifies |Ctrl-X| and |Shift-Delete| keyboard events, results in the "Cut"
// command being invoked.
TEST_F(WebPluginContainerTest, CutDeleteKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Cut".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_X);

  // Check that "Cut" command is invoked.
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsCutCalled());

  // Reset Cut status for next time.
  test_plugin->ResetEditCommandState();

  modifier_key = static_cast<WebInputEvent::Modifiers>(
      WebInputEvent::kShiftKey | WebInputEvent::kNumLockOn |
      WebInputEvent::kIsLeft);

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_DELETE);

  // Check that "Cut" command is invoked.
  EXPECT_TRUE(test_plugin->IsCutCalled());
}

// Verifies |Ctrl-V| and |Shift-Insert| keyboard events, results in the "Paste"
// command being invoked.
TEST_F(WebPluginContainerTest, PasteInsertKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Paste".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_V);

  // Check that "Paste" command is invoked.
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());

  // Reset Paste status for next time.
  test_plugin->ResetEditCommandState();

  modifier_key = static_cast<WebInputEvent::Modifiers>(
      WebInputEvent::kShiftKey | WebInputEvent::kNumLockOn |
      WebInputEvent::kIsLeft);

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_INSERT);

  // Check that "Paste" command is invoked.
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

// Verifies |Ctrl-Shift-V| keyboard event results in the "PasteAndMatchStyle"
// command being invoked.
TEST_F(WebPluginContainerTest, PasteAndMatchStyleKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "PasteAndMatchStyle".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kShiftKey | WebInputEvent::kNumLockOn |
      WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_V);

  // Check that "PasteAndMatchStyle" command is invoked.
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

TEST_F(WebPluginContainerTest, CutFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Cut".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  ExecuteContextMenuCommand(web_view, "Cut");
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsCutCalled());
}

TEST_F(WebPluginContainerTest, PasteFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Paste".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  ExecuteContextMenuCommand(web_view, "Paste");
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

TEST_F(WebPluginContainerTest, PasteAndMatchStyleFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Paste".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  ExecuteContextMenuCommand(web_view, "PasteAndMatchStyle");
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

// A class to facilitate testing that events are correctly received by plugins.
class EventTestPlugin : public FakeWebPlugin {
 public:
  explicit EventTestPlugin(const WebPluginParams& params)
      : FakeWebPlugin(params),
        last_event_type_(WebInputEvent::kUndefined),
        last_event_modifiers_(WebInputEvent::kNoModifiers) {}

  WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent& coalesced_event,
      WebCursorInfo&) override {
    const WebInputEvent& event = coalesced_event.Event();
    coalesced_event_count_ = coalesced_event.CoalescedEventSize();
    last_event_type_ = event.GetType();
    last_event_modifiers_ = event.GetModifiers();
    if (WebInputEvent::IsMouseEventType(event.GetType()) ||
        event.GetType() == WebInputEvent::kMouseWheel) {
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);
      last_event_location_ = IntPoint(mouse_event.PositionInWidget().x,
                                      mouse_event.PositionInWidget().y);
    } else if (WebInputEvent::IsTouchEventType(event.GetType())) {
      const WebTouchEvent& touch_event =
          static_cast<const WebTouchEvent&>(event);
      if (touch_event.touches_length == 1) {
        last_event_location_ =
            IntPoint(touch_event.touches[0].PositionInWidget().x,
                     touch_event.touches[0].PositionInWidget().y);
      } else {
        last_event_location_ = IntPoint();
      }
    }

    return WebInputEventResult::kHandledSystem;
  }
  WebInputEvent::Type GetLastInputEventType() { return last_event_type_; }

  IntPoint GetLastEventLocation() { return last_event_location_; }

  int GetLastEventModifiers() { return last_event_modifiers_; }

  void ClearLastEventType() { last_event_type_ = WebInputEvent::kUndefined; }

  size_t GetCoalescedEventCount() { return coalesced_event_count_; }

 private:
  ~EventTestPlugin() override = default;

  size_t coalesced_event_count_;
  WebInputEvent::Type last_event_type_;
  IntPoint last_event_location_;
  int last_event_modifiers_;
};

TEST_F(WebPluginContainerTest, GestureLongPressReachesPlugin) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebGestureEvent event(WebInputEvent::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        kWebGestureDeviceTouchscreen);

  // First, send an event that doesn't hit the plugin to verify that the
  // plugin doesn't receive it.
  event.SetPositionInWidget(WebFloatPoint(0, 0));

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kUndefined, test_plugin->GetLastInputEventType());

  // Next, send an event that does hit the plugin, and verify it does receive
  // it.
  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(
      WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2));

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kGestureLongPress,
            test_plugin->GetLastInputEventType());
}

TEST_F(WebPluginContainerTest, MouseEventButtons) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event = FrameTestHelpers::CreateMouseEvent(
      WebMouseEvent::kMouseMove, WebMouseEvent::Button::kNoButton,
      WebPoint(30, 30),
      WebInputEvent::kMiddleButtonDown | WebInputEvent::kShiftKey);

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kMouseMove, test_plugin->GetLastInputEventType());
  EXPECT_EQ(WebInputEvent::kMiddleButtonDown | WebInputEvent::kShiftKey,
            test_plugin->GetLastEventModifiers());
}

TEST_F(WebPluginContainerTest, MouseWheelEventTranslated) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kMouseWheel, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 2, test_plugin->GetLastEventLocation().Y());
}

TEST_F(WebPluginContainerTest, TouchEventScrolled) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));
  web_view->SmoothScroll(0, 200, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  WebPointerEvent event(
      WebInputEvent::kPointerDown,
      WebPointerProperties(
          1, WebPointerProperties::PointerType::kTouch,
          WebPointerProperties::Button::kLeft,
          WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2),
          WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2)),
      1.0f, 1.0f);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  web_view->DispatchBufferedTouchEvents();
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kTouchStart, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 2, test_plugin->GetLastEventLocation().Y());
}

TEST_F(WebPluginContainerTest, TouchEventScrolledWithCoalescedTouches) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));
  web_view->SmoothScroll(0, 200, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRawLowLatency);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  {
    WebRect rect = plugin_container_one_element.BoundsInViewport();
    WebPointerEvent event(
        WebInputEvent::kPointerDown,
        WebPointerProperties(
            1, WebPointerProperties::PointerType::kTouch,
            WebPointerProperties::Button::kLeft,
            WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2),
            WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2)),
        1.0f, 1.0f);

    WebCoalescedInputEvent coalesced_event(event);

    web_view->HandleInputEvent(coalesced_event);
    web_view->DispatchBufferedTouchEvents();
    RunPendingTasks();

    EXPECT_EQ(static_cast<const size_t>(1),
              test_plugin->GetCoalescedEventCount());
    EXPECT_EQ(WebInputEvent::kTouchStart, test_plugin->GetLastInputEventType());
    EXPECT_EQ(rect.width / 2, test_plugin->GetLastEventLocation().X());
    EXPECT_EQ(rect.height / 2, test_plugin->GetLastEventLocation().Y());
  }

  {
    WebRect rect = plugin_container_one_element.BoundsInViewport();
    WebPointerEvent event1(
        WebInputEvent::kPointerMove,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(rect.x + rect.width / 2 + 1,
                                           rect.y + rect.height / 2 + 1),
                             WebFloatPoint(rect.x + rect.width / 2 + 1,
                                           rect.y + rect.height / 2 + 1)),
        1.0f, 1.0f);

    WebCoalescedInputEvent coalesced_event(event1);

    WebPointerEvent event2(
        WebInputEvent::kPointerMove,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(rect.x + rect.width / 2 + 2,
                                           rect.y + rect.height / 2 + 2),
                             WebFloatPoint(rect.x + rect.width / 2 + 2,
                                           rect.y + rect.height / 2 + 2)),
        1.0f, 1.0f);
    WebPointerEvent event3(
        WebInputEvent::kPointerMove,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(rect.x + rect.width / 2 + 3,
                                           rect.y + rect.height / 2 + 3),
                             WebFloatPoint(rect.x + rect.width / 2 + 3,
                                           rect.y + rect.height / 2 + 3)),
        1.0f, 1.0f);

    coalesced_event.AddCoalescedEvent(event2);
    coalesced_event.AddCoalescedEvent(event3);

    web_view->HandleInputEvent(coalesced_event);
    web_view->DispatchBufferedTouchEvents();
    RunPendingTasks();

    EXPECT_EQ(static_cast<const size_t>(3),
              test_plugin->GetCoalescedEventCount());
    EXPECT_EQ(WebInputEvent::kTouchMove, test_plugin->GetLastInputEventType());
    EXPECT_EQ(rect.width / 2 + 1, test_plugin->GetLastEventLocation().X());
    EXPECT_EQ(rect.height / 2 + 1, test_plugin->GetLastEventLocation().Y());
  }
}

TEST_F(WebPluginContainerTest, MouseWheelEventScrolled) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));
  web_view->SmoothScroll(0, 200, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kMouseWheel, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 2, test_plugin->GetLastEventLocation().Y());
}

TEST_F(WebPluginContainerTest, MouseEventScrolled) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));
  web_view->SmoothScroll(0, 200, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event(WebInputEvent::kMouseMove, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::kMouseMove, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 2, test_plugin->GetLastEventLocation().Y());
}

TEST_F(WebPluginContainerTest, MouseEventZoomed) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->Resize(WebSize(300, 300));
  web_view->SetPageScaleFactor(2);
  web_view->SmoothScroll(0, 300, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event(WebInputEvent::kMouseMove, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::kMouseMove, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 4, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 4, test_plugin->GetLastEventLocation().Y());
}

TEST_F(WebPluginContainerTest, MouseWheelEventZoomed) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->Resize(WebSize(300, 300));
  web_view->SetPageScaleFactor(2);
  web_view->SmoothScroll(0, 300, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  event.SetPositionInWidget(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  RunPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::kMouseWheel, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 4, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 4, test_plugin->GetLastEventLocation().Y());
}

TEST_F(WebPluginContainerTest, TouchEventZoomed) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->Resize(WebSize(300, 300));
  web_view->SetPageScaleFactor(2);
  web_view->SmoothScroll(0, 300, 0);
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebRect rect = plugin_container_one_element.BoundsInViewport();
  WebPointerEvent event(
      WebInputEvent::kPointerDown,
      WebPointerProperties(
          1, WebPointerProperties::PointerType::kTouch,
          WebPointerProperties::Button::kLeft,
          WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2),
          WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2)),
      1.0f, 1.0f);

  web_view->HandleInputEvent(WebCoalescedInputEvent(event));
  web_view->DispatchBufferedTouchEvents();
  RunPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::kTouchStart, test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width / 4, test_plugin->GetLastEventLocation().X());
  EXPECT_EQ(rect.height / 4, test_plugin->GetLastEventLocation().Y());
}

// Verify that isRectTopmost returns false when the document is detached.
TEST_F(WebPluginContainerTest, IsRectTopmostTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebPluginContainerImpl* plugin_container_impl =
      ToWebPluginContainerImpl(GetWebPluginContainer(
          web_view, WebString::FromUTF8("translated-plugin")));
  plugin_container_impl->SetFrameRect(IntRect(0, 0, 300, 300));

  WebRect rect = plugin_container_impl->GetElement().BoundsInViewport();
  EXPECT_TRUE(plugin_container_impl->IsRectTopmost(rect));

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();

  EXPECT_FALSE(plugin_container_impl->IsRectTopmost(rect));
}

// Verify that IsRectTopmost works with odd and even dimensions.
TEST_F(WebPluginContainerTest, IsRectTopmostTestWithOddAndEvenDimensions) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebPluginContainerImpl* even_plugin_container_impl =
      ToWebPluginContainerImpl(GetWebPluginContainer(
          web_view, WebString::FromUTF8("translated-plugin")));
  even_plugin_container_impl->SetFrameRect(IntRect(0, 0, 300, 300));
  auto even_rect = even_plugin_container_impl->GetElement().BoundsInViewport();
  EXPECT_TRUE(even_plugin_container_impl->IsRectTopmost(even_rect));

  WebPluginContainerImpl* odd_plugin_container_impl =
      ToWebPluginContainerImpl(GetWebPluginContainer(
          web_view, WebString::FromUTF8("odd-dimensions-plugin")));
  odd_plugin_container_impl->SetFrameRect(IntRect(0, 0, 300, 300));
  auto odd_rect = odd_plugin_container_impl->GetElement().BoundsInViewport();
  EXPECT_TRUE(odd_plugin_container_impl->IsRectTopmost(odd_rect));
}

TEST_F(WebPluginContainerTest, ClippedRectsForIframedElement) {
  RegisterMockedURL("plugin_container.html");
  RegisterMockedURL("plugin_containing_page.html");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_containing_page.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_element = web_view->MainFrame()
                                  ->FirstChild()
                                  ->ToWebLocalFrame()
                                  ->GetDocument()
                                  .GetElementById("translated-plugin");
  WebPluginContainerImpl* plugin_container_impl =
      ToWebPluginContainerImpl(plugin_element.PluginContainer());

  DCHECK(plugin_container_impl);

  IntRect window_rect, clip_rect, unobscured_rect;
  CalculateGeometry(plugin_container_impl, window_rect, clip_rect,
                    unobscured_rect);
  EXPECT_EQ(IntRect(20, 220, 40, 40), window_rect);
  EXPECT_EQ(IntRect(0, 0, 40, 40), clip_rect);
  EXPECT_EQ(IntRect(0, 0, 40, 40), unobscured_rect);

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();
}

TEST_F(WebPluginContainerTest, ClippedRectsForSubpixelPositionedPlugin) {
  RegisterMockedURL("plugin_container.html");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          "subpixel-positioned-plugin");
  WebPluginContainerImpl* plugin_container_impl =
      ToWebPluginContainerImpl(plugin_element.PluginContainer());

  DCHECK(plugin_container_impl);

  IntRect window_rect, clip_rect, unobscured_rect;
  CalculateGeometry(plugin_container_impl, window_rect, clip_rect,
                    unobscured_rect);
  EXPECT_EQ(IntRect(0, 0, 40, 40), window_rect);
  EXPECT_EQ(IntRect(0, 0, 40, 40), clip_rect);
  EXPECT_EQ(IntRect(0, 0, 40, 40), unobscured_rect);

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();
}

TEST_F(WebPluginContainerTest, TopmostAfterDetachTest) {
  static WebRect topmost_rect(10, 10, 40, 40);

  // Plugin that checks isRectTopmost in destroy().
  class TopmostPlugin : public FakeWebPlugin {
   public:
    explicit TopmostPlugin(const WebPluginParams& params)
        : FakeWebPlugin(params) {}

    bool IsRectTopmost() { return Container()->IsRectTopmost(topmost_rect); }

    void Destroy() override {
      // In destroy, IsRectTopmost is no longer valid.
      EXPECT_FALSE(Container()->IsRectTopmost(topmost_rect));
      FakeWebPlugin::Destroy();
    }

   private:
    ~TopmostPlugin() override = default;
  };

  RegisterMockedURL("plugin_container.html");
  // The client must outlive WebViewHelper.
  CustomPluginWebFrameClient<TopmostPlugin> plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebPluginContainerImpl* plugin_container_impl =
      ToWebPluginContainerImpl(GetWebPluginContainer(
          web_view, WebString::FromUTF8("translated-plugin")));
  plugin_container_impl->SetFrameRect(IntRect(0, 0, 300, 300));

  EXPECT_TRUE(plugin_container_impl->IsRectTopmost(topmost_rect));

  TopmostPlugin* test_plugin =
      static_cast<TopmostPlugin*>(plugin_container_impl->Plugin());
  EXPECT_TRUE(test_plugin->IsRectTopmost());

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();

  EXPECT_FALSE(plugin_container_impl->IsRectTopmost(topmost_rect));
}

namespace {

class CompositedPlugin : public FakeWebPlugin {
 public:
  explicit CompositedPlugin(const WebPluginParams& params)
      : FakeWebPlugin(params), layer_(cc::Layer::Create()) {}

  cc::Layer* GetCcLayer() const { return layer_.get(); }

  // WebPlugin

  bool Initialize(WebPluginContainer* container) override {
    if (!FakeWebPlugin::Initialize(container))
      return false;
    container->SetCcLayer(layer_.get(), false);
    return true;
  }

  void Destroy() override {
    Container()->SetCcLayer(nullptr, false);
    FakeWebPlugin::Destroy();
  }

 private:
  ~CompositedPlugin() override = default;

  scoped_refptr<cc::Layer> layer_;
};

}  // namespace

TEST_F(WebPluginContainerTest, CompositedPluginSPv2) {
  ScopedSlimmingPaintV2ForTest enable_s_pv2(true);
  RegisterMockedURL("plugin.html");
  // Must outlive |web_view_helper|
  CustomPluginWebFrameClient<CompositedPlugin> web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin.html", &web_frame_client);
  EnablePlugins(web_view, WebSize(800, 600));

  WebPluginContainerImpl* container = static_cast<WebPluginContainerImpl*>(
      GetWebPluginContainer(web_view, WebString::FromUTF8("plugin")));
  ASSERT_TRUE(container);
  Element* element = static_cast<Element*>(container->GetElement());
  const auto* plugin =
      static_cast<const CompositedPlugin*>(container->Plugin());

  std::unique_ptr<PaintController> paint_controller = PaintController::Create();
  paint_controller->UpdateCurrentPaintChunkProperties(
      base::nullopt, PropertyTreeState::Root());
  GraphicsContext graphics_context(*paint_controller);
  container->Paint(graphics_context, kGlobalPaintNormalPhase,
                   CullRect(IntRect(10, 10, 400, 300)));
  paint_controller->CommitNewDisplayItems();

  const auto& display_items =
      paint_controller->GetPaintArtifact().GetDisplayItemList();
  ASSERT_EQ(1u, display_items.size());
  EXPECT_EQ(element->GetLayoutObject(), &display_items[0].Client());
  ASSERT_EQ(DisplayItem::kForeignLayerPlugin, display_items[0].GetType());
  const auto& foreign_layer_display_item =
      static_cast<const ForeignLayerDisplayItem&>(display_items[0]);
  EXPECT_EQ(plugin->GetCcLayer(), foreign_layer_display_item.GetLayer());
}

TEST_F(WebPluginContainerTest, NeedsWheelEvents) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|
  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, WebSize(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  plugin_container_one_element.PluginContainer()->SetWantsWheelEvents(true);

  RunPendingTasks();
  EXPECT_TRUE(web_view->MainFrameImpl()
                  ->GetFrame()
                  ->GetEventHandlerRegistry()
                  .HasEventHandlers(EventHandlerRegistry::kWheelEventBlocking));
}

TEST_F(WebPluginContainerTest, IFramePluginDocumentDisplayNone) {
  RegisterMockedURL("test.pdf", "application/pdf");
  RegisterMockedURL("iframe_pdf_display_none.html", "text/html");

  TestPluginWebFrameClient plugin_web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "iframe_pdf_display_none.html", &plugin_web_frame_client);
  web_view->UpdateAllLifecyclePhases();

  WebFrame* web_iframe = web_view->MainFrame()->FirstChild();
  LocalFrame* iframe = ToLocalFrame(WebFrame::ToCoreFrame(*web_iframe));
  EXPECT_TRUE(iframe->GetWebPluginContainer());
}

}  // namespace blink
