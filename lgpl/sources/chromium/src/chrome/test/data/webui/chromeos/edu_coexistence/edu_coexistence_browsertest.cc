// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/chromeos/lacros_only_mocha_browser_test.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "content/public/test/browser_test.h"

class EduCoexistenceMochaTest : public WebUIMochaBrowserTest {
 protected:
  EduCoexistenceMochaTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunEduCoexistenceTest(const std::string& test_path) {
    RunTest(base::StrCat({"chromeos/edu_coexistence/", test_path}),
            "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(EduCoexistenceMochaTest, App) {
  RunEduCoexistenceTest("edu_coexistence_app_test.js");
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceMochaTest, Controller) {
  RunEduCoexistenceTest("edu_coexistence_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceMochaTest, Ui) {
  RunEduCoexistenceTest("edu_coexistence_ui_test.js");
}

class EduCoexistenceWithArcRestrictionsMochaTest
    : public LacrosOnlyMochaBrowserTest {
 protected:
  EduCoexistenceWithArcRestrictionsMochaTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
    scoped_feature_list_.InitWithFeatures(
        /*enabled=*/
        {
            ash::standalone_browser::features::kLacrosOnly,
            ash::standalone_browser::features::kLacrosProfileMigrationForceOff,
        },
        /*disabled=*/{});
  }

  void RunEduCoexistenceTest(const std::string& test_path) {
    RunTest(base::StrCat({"chromeos/edu_coexistence/", test_path}),
            "mocha.run()");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(EduCoexistenceWithArcRestrictionsMochaTest, App) {
  RunEduCoexistenceTest("edu_coexistence_app_with_arc_picker_test.js");
}
