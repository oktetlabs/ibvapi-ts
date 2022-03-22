# SPDX-License-Identifier: Apache-2.0
# -*- coding: utf-8 -*-

project = u'Infiniband Verbs API Test Suite'
copyright = u'2012-2022 OKTET Labs Ltd.'
author = u'Yurij Plotnikov'
version = u'1.0'
release = u'release'

# Specify the path to Doxyrest extensions for Sphinx:

import sys
import os
doxyrest_path = os.getenv('DOXYREST_PREFIX')
sys.path.insert(1, os.path.abspath(doxyrest_path + '/share/doxyrest/sphinx'))

source_suffix = '.rst'
master_doc = 'index'
language = "c"

exclude_patterns = [
    'generated/rst/index.rst',
]

import sphinx_rtd_theme
extensions = [
    "sphinx_rtd_theme"
]
html_theme = 'sphinx_rtd_theme'

html_theme_options = {
    'collapse_navigation': False,
}

extensions += ['doxyrest', 'cpplexer']

