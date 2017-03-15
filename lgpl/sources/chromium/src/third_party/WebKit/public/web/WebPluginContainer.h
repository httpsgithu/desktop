/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#ifndef WebPluginContainer_h
#define WebPluginContainer_h

#include "../platform/WebCommon.h"
#include <v8.h>

struct NPObject;

namespace blink {

class WebDocument;
class WebElement;
class WebPlugin;
class WebString;
class WebURL;
class WebURLRequest;
class WebLayer;
class WebDOMMessageEvent;
struct WebPoint;
struct WebRect;

class WebPluginContainer {
 public:
  enum TouchEventRequestType {
    TouchEventRequestTypeNone,
    TouchEventRequestTypeRaw,
    TouchEventRequestTypeSynthesizedMouse,
  };

  // Returns the element containing this plugin.
  virtual WebElement element() = 0;

  // Returns the owning document for the plugin.
  virtual WebDocument document() = 0;

  // Synchronously dispatches the progress event.
  virtual void dispatchProgressEvent(const WebString& type,
                                     bool lengthComputable,
                                     unsigned long long loaded,
                                     unsigned long long total,
                                     const WebString& url) = 0;

  // Enqueue's a task to dispatch the event.
  // TODO(esprehn): Why are progress events sync and message events async!?
  virtual void enqueueMessageEvent(const WebDOMMessageEvent&) = 0;

  virtual void invalidate() = 0;
  virtual void invalidateRect(const WebRect&) = 0;
  virtual void scrollRect(const WebRect&) = 0;

  // Schedules an animation of the WebView that contains the plugin, as well as
  // the plugin.
  virtual void scheduleAnimation() = 0;

  // Causes the container to report its current geometry via
  // WebPlugin::updateGeometry.
  virtual void reportGeometry() = 0;

  // Returns the scriptable object associated with the DOM element
  // containing the plugin as a native v8 object.
  virtual v8::Local<v8::Object> v8ObjectForElement() = 0;

  // Executes a "javascript:" URL on behalf of the plugin in the context
  // of the frame containing the plugin.  Returns the result of script
  // execution, if any.
  virtual WebString executeScriptURL(const WebURL&, bool popupsAllowed) = 0;

  // Loads an URL in the specified frame (or the frame containing this
  // plugin if target is empty).  If notifyNeeded is true, then upon
  // completion, WebPlugin::didFinishLoadingFrameRequest is called if the
  // load was successful or WebPlugin::didFailLoadingFrameRequest is
  // called if the load failed.  The given notifyData is passed along to
  // the callback.
  virtual void loadFrameRequest(const WebURLRequest&,
                                const WebString& target) = 0;

  // Determines whether the given rectangle in this plugin is above all other
  // content. The rectangle is in the plugin's coordinate system.
  virtual bool isRectTopmost(const WebRect&) = 0;

  // Notifies when the plugin changes the kind of touch-events it accepts.
  virtual void requestTouchEventType(TouchEventRequestType) = 0;

  // Notifies when the plugin starts/stops accepting wheel events. Without
  // calling the function with true, the container might not always able to
  // receive wheel events in some cases (such as when threaded compositing
  // is in use but a scroll bar is not in use).
  virtual void setWantsWheelEvents(bool) = 0;

  // Converts root frame's coordinates to plugin's local coordinates.
  virtual WebPoint rootFrameToLocalPoint(const WebPoint&) = 0;

  // Converts plugin's local coordinate to root frame's coordinates.
  virtual WebPoint localToRootFramePoint(const WebPoint&) = 0;

  // Returns the plugin this container owns. This plugin will be
  // automatically destroyed when the container is destroyed.
  virtual WebPlugin* plugin() = 0;

  // Sets the plugin owned by this container. If the container already owned
  // a different plugin before this call, that old plugin is now unowned.
  // The caller is then responsible for destroying the old plugin.
  virtual void setPlugin(WebPlugin*) = 0;

  virtual float altDeviceScaleFactor() = 0;
  virtual float deviceScaleFactor() = 0;
  virtual float pageScaleFactor() = 0;
  virtual float pageZoomFactor() = 0;

  // Sets the layer representing the plugin for compositing. The
  // WebPluginContainer does *not* take ownership.
  virtual void setWebLayer(WebLayer*) = 0;

  virtual void requestFullscreen() = 0;
  virtual bool isFullscreenElement() const = 0;
  virtual void cancelFullscreen() = 0;

 protected:
  ~WebPluginContainer() {}
};

}  // namespace blink

#endif
