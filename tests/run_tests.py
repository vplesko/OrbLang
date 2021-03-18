import difflib
import glob
import os
import platform
import re
import subprocess
import sys

ORBC_EXE = sys.argv[1]

TEST_POS_DIR = 'positive'
TEST_NEG_DIR = 'negative'
TEST_BIN_DIR = 'bin'


def run_positive_test(case):
    print('Positive test: ' + case)

    src_file = TEST_POS_DIR + '/' + case + '.orb'
    exe_file = TEST_BIN_DIR + '/' + case
    if platform.system() == 'Windows':
        exe_file += '.exe'
    cmp_file = TEST_POS_DIR + '/' + case + '.txt'

    result = subprocess.run([ORBC_EXE, src_file, '-o', exe_file], stderr=subprocess.DEVNULL)
    if result.returncode != 0:
        return False

    result = subprocess.run(exe_file, stdout=subprocess.PIPE)
    exe_out = result.stdout.decode('utf-8').splitlines()

    with open(cmp_file, 'r') as file:
        cmp_out = file.read().splitlines()

    success = True
    for line in difflib.context_diff(exe_out, cmp_out, fromfile=exe_file, tofile=cmp_file):
        print(line)
        success = False

    return success


def run_negative_test(case):
    print('Negative test: ' + case)

    src_file = TEST_NEG_DIR + '/' + case + '.orb'
    exe_file = TEST_BIN_DIR + '/' + case
    if platform.system() == 'Windows':
        exe_file += '.exe'

    result = subprocess.run([ORBC_EXE, src_file, '-o', exe_file], stderr=subprocess.DEVNULL)
    return result.returncode > 0 and result.returncode < 100


def run_all_tests(dir, test_func):
    re_pattern = re.compile(r'(test.*)\.orb', re.IGNORECASE)
    test_src_files = glob.glob(dir + '/test*.orb')
    test_cases = [re.search(re_pattern, src).group(1)
                  for src in test_src_files]

    for case in test_cases:
        if case == None:
            continue

        success = test_func(case)
        if not success:
            return False

    return True


if __name__ == "__main__":
    if not os.path.exists(TEST_BIN_DIR):
        os.mkdir(TEST_BIN_DIR)

    if not run_all_tests(TEST_POS_DIR, run_positive_test) \
        or not run_all_tests(TEST_NEG_DIR, run_negative_test):
        print('Test failed!')
    else:
        print('Tests ran successfully.')
