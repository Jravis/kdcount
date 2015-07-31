import sys
import os

from numpy.testing import Tester
sys.path.insert(0, os.path.abspath('.'))

from sys import argv

tester = Tester()
r = tester.test(extra_argv=['-w', 'tests'] + argv[1:])
if not r:
    raise Exception("Tests failed")
