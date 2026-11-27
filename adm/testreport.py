#! /usr/bin/env python3
#
# testreport.py
#
#    Runs one of the Sedona test suites and turns its console output
#    into test results:
#
#      <out>/<name>.log   the complete console output
#      <out>/<name>.xml   JUnit XML with one test case per test method
#
#    When run by GitHub Actions it also writes a summary of the run to
#    the job summary and an annotation for every test that failed.
#
#    The exit code is zero only if the suite ran to the end and every
#    test passed.  The exit code of the suite itself is not enough for
#    this: the svm returns its number of assert failures, which wraps
#    to zero at 256, and a suite which runs no tests at all also
#    "passes".
#
#    Usage:
#      testreport.py --suite sedonac -- bin/sedonac.sh -test
#      testreport.py --suite svm -- bin/svm scode/x86-test.scode -test
#

from __future__ import print_function
import argparse
import glob
import os
import re
import select
import signal
import subprocess
import sys
import time
from xml.sax.saxutils import escape, quoteattr

try:
  import pty
except ImportError:          # Windows
  pty = None

PASSED = "passed"
FAILED = "failed"   # a verify or assert of the test failed
ERROR  = "error"    # the test never reported a result (crash, hang)

# "[12 verifies]" ends a test which ran to completion, in both suites
verifiesRe = re.compile(r"\[(\d+) verifies\]\s*$")

# characters which are not allowed in XML 1.0
badXmlRe = re.compile(u"[^\u0009\u000a\u000d -퟿-�]")


class TestCase:
  def __init__(self, cls, name, start):
    self.cls      = cls
    self.name     = name
    self.start    = start
    self.duration = 0.0
    self.status   = None
    self.verifies = 0
    self.message  = ""
    self.output   = []

  def qname(self):
    return self.cls + "." + self.name


#
# Suite
#   Base class of the parsers.  The output of the suite is fed in line
#   by line, each with the time it was read at, since neither suite
#   reports how long a test took.
#
class Suite:
  def __init__(self):
    self.tests    = []
    self.cur      = None     # test which is running
    self.finished = False    # has the suite printed its own summary
    self.problems = []       # failures which don't belong to a test
    self.lastEnd  = None     # when the previous test ended

  def open(self, cls, name, now):
    self.close(now)
    # Tests run back to back, so a test starts when the previous one
    # ended.  The time its name shows up is no use: the svm prints the
    # name without a newline and it only arrives with the next one.
    self.cur = TestCase(cls, name, self.lastEnd or now)
    self.tests.append(self.cur)

  # Close the running test.  A test which gets here without a result
  # never printed its verifies, so it did not run to completion.
  def close(self, now, why="the next test started before it reported a result"):
    t = self.cur
    if t is None: return
    if t.status is None:
      t.status  = ERROR
      t.message = why
    t.duration = now - t.start
    self.lastEnd = now
    self.cur = None

  def passed(self, verifies, now):
    t = self.cur
    t.verifies = verifies
    if t.status is None: t.status = PASSED
    self.close(now)

  def count(self, status):
    return len([t for t in self.tests if t.status == status])

  def verifies(self):
    return sum([t.verifies for t in self.tests])

  def ok(self):
    return len(self.problems) == 0 and \
           len(self.tests) == self.count(PASSED)


#
# SedonacSuite
#   Parses the output of "sedonac -test":
#
#     -- sedonac.test.ParserTest.testFoo [12 verifies]
#     -- sedonac.test.ParserTest.testBar
#     FAILED
#     sedonac.test.TestException: ...
#       at ...
#     ALL TESTS PASSED 2727 total verifies [143632ms]
#
class SedonacSuite(Suite):
  startRe = re.compile(r"^-- (sedonac\.test\.\w+)\.(\w+)(.*)$")
  endRe   = re.compile(r"^(ALL TESTS PASSED|SOME TESTS FAILED) ")

  def line(self, line, now):
    m = self.startRe.match(line)
    if m:
      self.open(m.group(1), m.group(2), now)
      line = m.group(3)
    elif self.endRe.match(line):
      self.close(now)
      self.finished = True
      return

    t = self.cur
    if t is None: return

    if t.status == FAILED:
      # the stack trace follows up to the next test
      if not t.message and line.strip(): t.message = line.strip()
      t.output.append(line)
      return

    if line.strip() == "FAILED":
      t.status = FAILED
      return

    m = verifiesRe.search(line)
    if m:
      self.passed(int(m.group(1)), now)
    elif line.strip():
      t.output.append(line)

  def sourceOf(self, t, home):
    return None, None


#
# SvmSuite
#   Parses the output of "svm <scode> -test":
#
#     -- svm test sys::StrTest.test [41 verifies]
#     -- svm test sys::BufTest.test
#     ASSERT FAILURE: sys::BufTest.test [Line 30]
#      [12 verifies]
#     Assert Successes = 66694
#     Assert Failures  = 0
#
class SvmSuite(Suite):
  # the svm prints nothing between the name of a test and its output,
  # so a failed assert can follow the name directly
  startRe  = re.compile(r"^-- svm test (\w+)::(\w+)\.(\w+?)(?=ASSERT FAILURE:|\W|$)(.*)$")
  assertRe = re.compile(r"ASSERT FAILURE: (.*) \[Line (\d+)\]")
  endRe    = re.compile(r"^Assert Failures\s*=\s*(\d+)")

  def __init__(self):
    Suite.__init__(self)
    self.totalFailures = None

  def line(self, line, now):
    m = self.startRe.match(line)
    if m:
      self.open(m.group(1) + "::" + m.group(2), m.group(3), now)
      line = m.group(4)

    m = self.endRe.match(line)
    if m:
      self.close(now)
      self.finished = True
      self.totalFailures = int(m.group(1))
      return

    t = self.cur
    m = self.assertRe.search(line)
    if m:
      if t is None:
        self.problems.append("Assert failure outside of a test: " + line.strip())
      else:
        t.status = FAILED
        if not t.message: t.message = line.strip()
        t.output.append(line)
      return

    if t is None: return

    m = verifiesRe.search(line)
    if m:
      self.passed(int(m.group(1)), now)
    elif line.strip():
      t.output.append(line)

  # Source file and line of the first failed assert, for annotations
  def sourceOf(self, t, home):
    m = self.assertRe.search("\n".join(t.output))
    if not m: return None, None
    q = re.match(r"(\w+)::(\w+)", m.group(1))
    if not q: return None, None
    pattern = os.path.join(home, "src", "kits", q.group(1), "**", q.group(2) + ".sedona")
    files = glob.glob(pattern, recursive=True)
    if len(files) != 1: return None, None
    return os.path.relpath(files[0], home), int(m.group(2))


#
# Run the command and feed its output to the suite, line by line.
# Returns the exit code, or a string describing why it was killed.
#
def run(cmd, suite, log, timeout):
  # Use a pseudo terminal if there is one: the svm then flushes its
  # output after every line, so that the time of a line is the time it
  # was printed and nothing is lost if the svm crashes.
  if pty is not None:
    master, slave = pty.openpty()
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=slave,
                            stderr=slave, start_new_session=True)
    os.close(slave)
    fd = master
  else:
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    fd = proc.stdout.fileno()

  deadline = None
  if timeout: deadline = time.time() + timeout
  killed = None
  buf = b""

  def feed(raw):
    text = raw.decode("utf-8", "replace").rstrip("\r\n")
    print(text)
    sys.stdout.flush()
    log.write(text + "\n")
    now = time.time()
    if suite.lastEnd is None: suite.lastEnd = now
    suite.line(text, now)

  while True:
    if deadline is not None and pty is not None:
      left = deadline - time.time()
      if left <= 0:
        killed = "timed out after %d seconds" % timeout
        try:
          os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
          proc.kill()
        break
      ready = select.select([fd], [], [], min(left, 5.0))[0]
      if not ready: continue
    try:
      chunk = os.read(fd, 4096)
    except OSError:          # Linux reports the end of a pty as an error
      chunk = b""
    if not chunk: break
    buf += chunk
    lines = buf.split(b"\n")
    buf = lines.pop()
    for raw in lines: feed(raw)

  if buf: feed(buf)
  code = proc.wait()
  if pty is not None: os.close(fd)

  if killed: return killed
  if code < 0:
    try:
      return "was killed by " + signal.Signals(-code).name
    except ValueError:
      return "was killed by signal %d" % -code
  return code


def clean(s):
  return badXmlRe.sub("?", s)


def writeJUnit(path, name, suite, elapsed):
  out = open(path, "w", encoding="utf-8")
  out.write('<?xml version="1.0" encoding="UTF-8"?>\n')
  out.write('<testsuite name=%s tests="%d" failures="%d" errors="%d" time="%.3f">\n' %
    (quoteattr(name), len(suite.tests) + len(suite.problems), suite.count(FAILED),
     suite.count(ERROR) + len(suite.problems), elapsed))
  for t in suite.tests:
    out.write('  <testcase classname=%s name=%s time="%.3f"' %
      (quoteattr(t.cls), quoteattr(t.name), t.duration))
    if t.status == PASSED:
      out.write('/>\n')
      continue
    tag = "failure" if t.status == FAILED else "error"
    out.write('>\n    <%s message=%s>%s</%s>\n  </testcase>\n' %
      (tag, quoteattr(clean(t.message)), escape(clean("\n".join(t.output))), tag))
  for i, p in enumerate(suite.problems):
    out.write('  <testcase classname=%s name="suite%d" time="0">\n' % (quoteattr(name), i))
    out.write('    <error message=%s/>\n  </testcase>\n' % quoteattr(clean(p)))
  out.write('</testsuite>\n')
  out.close()


def headline(suite, elapsed):
  s = "%d tests, %d passed, %d failed, %d errors, %d verifies, %.1f s" % \
    (len(suite.tests), suite.count(PASSED), suite.count(FAILED),
     suite.count(ERROR), suite.verifies(), elapsed)
  return s


def writeSummary(path, name, suite, elapsed):
  out = open(path, "a", encoding="utf-8")
  icon = ":white_check_mark:" if suite.ok() else ":x:"
  out.write("### %s %s\n\n" % (icon, name))
  out.write(headline(suite, elapsed) + "\n\n")

  for p in suite.problems:
    out.write("- :x: %s\n" % p)
  if suite.problems: out.write("\n")

  bad = [t for t in suite.tests if t.status != PASSED]
  for t in bad:
    out.write("<details><summary>:x: <code>%s</code> %s: %s</summary>\n\n" %
      (escape(t.qname()), t.status, escape(t.message)))
    out.write("```\n%s\n```\n\n</details>\n\n" % "\n".join(t.output[-200:]).replace("```", "'''"))

  # per class totals, folded away since they only matter when digging
  classes = []
  byClass = {}
  for t in suite.tests:
    if t.cls not in byClass:
      byClass[t.cls] = [0, 0, 0, 0.0]
      classes.append(t.cls)
    c = byClass[t.cls]
    c[0] += 1
    if t.status != PASSED: c[1] += 1
    c[2] += t.verifies
    c[3] += t.duration
  if classes:
    out.write("<details><summary>Results by test class</summary>\n\n")
    out.write("| Class | Tests | Not passed | Verifies | Time |\n")
    out.write("|---|---:|---:|---:|---:|\n")
    for cls in classes:
      c = byClass[cls]
      out.write("| `%s` | %d | %d | %d | %.1f s |\n" % (cls, c[0], c[1], c[2], c[3]))
    out.write("\n</details>\n\n")
  out.close()


# Value of a GitHub workflow command
def ghEscape(s, prop=False):
  s = s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
  if prop: s = s.replace(":", "%3A").replace(",", "%2C")
  return s


def writeAnnotations(name, suite, home):
  for p in suite.problems:
    print("::error title=%s::%s" % (ghEscape(name, True), ghEscape(p)))
  for t in suite.tests:
    if t.status == PASSED: continue
    props = "title=" + ghEscape("%s: %s %s" % (name, t.qname(), t.status), True)
    f, line = suite.sourceOf(t, home)
    if f: props += ",file=%s,line=%d" % (ghEscape(f, True), line)
    print("::error %s::%s" % (props, ghEscape(t.message or t.status)))


def main():
  parser = argparse.ArgumentParser(description="Run a Sedona test suite and report its results")
  parser.add_argument("--suite", required=True, choices=["sedonac", "svm"],
                      help="Which output format to expect")
  parser.add_argument("--name", default=None,
                      help="Name of the results, also used for the file names (default is the suite)")
  parser.add_argument("--out", default=None,
                      help="Directory for the results (default is build/test-results)")
  parser.add_argument("--timeout", type=int, default=0,
                      help="Kill the suite after this many seconds (default is no timeout)")
  parser.add_argument("cmd", nargs=argparse.REMAINDER,
                      help="Command which runs the suite, after --")
  options = parser.parse_args()

  cmd = options.cmd
  if cmd and cmd[0] == "--": cmd = cmd[1:]
  if not cmd: parser.error("no command given")

  home = os.environ.get("sedona_home")
  if not home:
    home = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  name = options.name or options.suite
  outDir = options.out or os.path.join(home, "build", "test-results")
  if not os.path.isdir(outDir): os.makedirs(outDir)
  base = os.path.join(outDir, re.sub(r"[^\w.-]+", "-", name))

  suite = SedonacSuite() if options.suite == "sedonac" else SvmSuite()

  t1 = time.time()
  with open(base + ".log", "w", encoding="utf-8") as log:
    result = run(cmd, suite, log, options.timeout)
  t2 = time.time()
  elapsed = t2 - t1

  # everything which makes the run a failure beyond the tests themselves
  if isinstance(result, str):
    suite.close(t2, "the suite " + result)
    suite.problems.append("The suite " + result)
  else:
    suite.close(t2, "the suite ended before the test reported a result")
    if result != 0 and suite.ok():
      suite.problems.append("The suite exited with code %d" % result)
  if not suite.finished:
    suite.problems.append("The suite did not run to the end")
  if len(suite.tests) == 0:
    suite.problems.append("No tests were run")
  if options.suite == "svm" and suite.totalFailures:
    inTests = sum([len(SvmSuite.assertRe.findall("\n".join(t.output))) for t in suite.tests])
    if suite.totalFailures != inTests and suite.ok():
      suite.problems.append("The svm counted %d assert failures" % suite.totalFailures)

  writeJUnit(base + ".xml", name, suite, elapsed)
  if os.environ.get("GITHUB_STEP_SUMMARY"):
    writeSummary(os.environ["GITHUB_STEP_SUMMARY"], name, suite, elapsed)
  if os.environ.get("GITHUB_ACTIONS"):
    writeAnnotations(name, suite, home)

  print("")
  print("%s: %s" % (name, headline(suite, elapsed)))
  for p in suite.problems:
    print("  " + p)
  for t in suite.tests:
    if t.status != PASSED:
      print("  %s %s: %s" % (t.status.upper(), t.qname(), t.message))
  print("  results: " + base + ".xml")
  return 0 if suite.ok() else 1


if __name__ == '__main__':
  sys.exit(main())
