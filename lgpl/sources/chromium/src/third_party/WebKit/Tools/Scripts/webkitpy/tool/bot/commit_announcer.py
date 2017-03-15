# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
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

import logging
import re
import threading
import time

from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.system.executive import ScriptError
from webkitpy.thirdparty.irc.ircbot import SingleServerIRCBot

_log = logging.getLogger(__name__)

SERVER = "irc.freenode.net"
PORT = 6667
CHANNEL = "#blink"
NICKNAME = "commit-bot"

PULL_TIMEOUT_SECONDS = 60 * 5
UPDATE_WAIT_SECONDS = 10
RETRY_ATTEMPTS = 8


class CommitAnnouncer(SingleServerIRCBot):
    _commit_detail_format = "%H\n%ae\n%s\n%b"  # commit-sha1, author email, subject, body

    def __init__(self, tool, announce_path, irc_password):
        SingleServerIRCBot.__init__(self, [(SERVER, PORT, irc_password)], NICKNAME, NICKNAME)
        self.announce_path = announce_path
        self.git = Git(cwd=tool.scm().checkout_root, filesystem=tool.filesystem, executive=tool.executive)
        self.commands = {
            'help': self.help,
            'ping': self.ping,
            'quit': self.stop,
        }
        self.last_commit = None

    def start(self):
        if not self._update():
            return
        self.last_commit = self.git.latest_git_commit()
        SingleServerIRCBot.start(self)

    def post_new_commits(self):
        if not self.connection.is_connected():
            return
        if not self._update(force_clean=True):
            self.stop("Failed to update repository!")
            return
        new_commits = self.git.git_commits_since(self.last_commit)
        if not new_commits:
            return
        self.last_commit = new_commits[-1]
        for commit in new_commits:
            if not self._should_announce_commit(commit):
                continue
            commit_detail = self._commit_detail(commit)
            if commit_detail:
                _log.info('%s Posting commit %s', self._time(), commit)
                _log.info('%s Posted message: %s', self._time(), repr(commit_detail))
                self._post(commit_detail)
            else:
                _log.error('Malformed commit log for %s', commit)

    # Bot commands.

    def help(self):
        self._post('Commands available: %s' % ' '.join(self.commands.keys()))

    def ping(self):
        self._post('Pong.')

    def stop(self, message=""):
        self.connection.execute_delayed(0, lambda: self.die(message))

    # IRC event handlers. Methods' arguments are determined by superclass
    # and some arguments maybe unused - pylint: disable=unused-argument

    def on_nicknameinuse(self, connection, event):
        connection.nick('%s_' % connection.get_nickname())

    def on_welcome(self, connection, event):
        connection.join(CHANNEL)

    def on_pubmsg(self, connection, event):
        message = event.arguments()[0]
        command = self._message_command(message)
        if command:
            command()

    def _update(self, force_clean=False):
        if not self.git.is_cleanly_tracking_remote_master():
            if not force_clean:
                confirm = raw_input('This repository has local changes, continue? (uncommitted changes will be lost) y/n: ')
                if not confirm.lower() == 'y':
                    return False
            try:
                self.git.ensure_cleanly_tracking_remote_master()
            except ScriptError as e:
                _log.error('Failed to clean repository: %s', e)
                return False

        attempts = 1
        while attempts <= RETRY_ATTEMPTS:
            if attempts > 1:
                # User may have sent a keyboard interrupt during the wait.
                if not self.connection.is_connected():
                    return False
                wait = int(UPDATE_WAIT_SECONDS) << (attempts - 1)
                if wait < 120:
                    _log.info('Waiting %s seconds', wait)
                else:
                    _log.info('Waiting %s minutes', wait / 60)
                time.sleep(wait)
                _log.info('Pull attempt %s out of %s', attempts, RETRY_ATTEMPTS)
            try:
                self.git.pull(timeout_seconds=PULL_TIMEOUT_SECONDS)
                return True
            except ScriptError as e:
                _log.error('Error pulling from server: %s', e)
                _log.error('Output: %s', e.output)
            attempts += 1
        _log.error('Exceeded pull attempts')
        _log.error('Aborting at time: %s', self._time())
        return False

    def _time(self):
        return time.strftime('[%x %X %Z]', time.localtime())

    def _message_command(self, message):
        prefix = '%s:' % self.connection.get_nickname()
        if message.startswith(prefix):
            command_name = message[len(prefix):].strip()
            if command_name in self.commands:
                return self.commands[command_name]
        return None

    def _should_announce_commit(self, commit):
        return any(path.startswith(self.announce_path) for path in self.git.affected_files(commit))

    def _commit_detail(self, commit):
        return self._format_commit_detail(self.git.git_commit_detail(commit, self._commit_detail_format))

    def _format_commit_detail(self, commit_detail):
        if commit_detail.count('\n') < self._commit_detail_format.count('\n'):
            return ''

        commit, email, subject, body = commit_detail.split('\n', 3)
        commit_position_re = r'^Cr-Commit-Position: refs/heads/master@\{#(?P<commit_position>\d+)\}'
        commit_position = None
        red_flag_strings = ['NOTRY=true', 'TBR=']
        red_flags = []

        for line in body.split('\n'):
            match = re.search(commit_position_re, line)
            if match:
                commit_position = match.group('commit_position')

            for red_flag_string in red_flag_strings:
                if line.lower().startswith(red_flag_string.lower()):
                    red_flags.append(line.strip())

        url = 'https://crrev.com/%s' % (commit_position if commit_position else commit[:8])
        red_flag_message = '\x037%s\x03' % (' '.join(red_flags)) if red_flags else ''

        return ('%s %s committed "%s" %s' % (url, email, subject, red_flag_message)).strip()

    def _post(self, message):
        self.connection.execute_delayed(0, lambda: self.connection.privmsg(CHANNEL, self._sanitize_string(message)))

    def _sanitize_string(self, message):
        return message.encode('ascii', 'backslashreplace')


class CommitAnnouncerThread(threading.Thread):

    def __init__(self, tool, announce_path, irc_password):
        threading.Thread.__init__(self)
        self.bot = CommitAnnouncer(tool, announce_path, irc_password)

    def run(self):
        self.bot.start()

    def stop(self):
        self.bot.stop()
        self.join()
