# Copyright (C) 2023 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from blinkpy.web_tests.port import linux


class ChromePort(linux.LinuxPort):
    """ChromePort is essentially same to LinuxPort except that it
    defines an additional directory for Chrome specific baselines
    and a test expectation tag for Chrome.
    """
    port_name = 'chrome'

    SUPPORTED_VERSIONS = ('chrome', )
    FALLBACK_PATHS = {}
    FALLBACK_PATHS['chrome'] = (
        ['linux-chrome'] + linux.LinuxPort.latest_platform_fallback_path())

    def configuration_specifier_macros(self):
        return {self.port_name: list(self.SUPPORTED_VERSIONS)}

    def default_expectations_files(self):
        """Returns a list of paths to expectations files that apply by default.

        There are other "test expectations" files that may be applied if
        the --additional-expectations flag is passed; those aren't included
        here.
        """
        return filter(None, [
            self.path_to_generic_test_expectations_file(),
            self._filesystem.join(self.web_tests_dir(), 'NeverFixTests'),
            self._filesystem.join(self.web_tests_dir(),
                                  'ChromeTestExpectations'),
            self._filesystem.join(self.web_tests_dir(),
                                  'StaleTestExpectations'),
            self._filesystem.join(self.web_tests_dir(), 'SlowTests')
        ])
