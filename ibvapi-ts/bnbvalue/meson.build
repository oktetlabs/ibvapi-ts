# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2012-2021 OKTET Labs Ltd.

tests = [
    'bad_cq_pool_size',
    'bad_qp_type',
    'circle_wr',
    'create_cq_bad_comp_vect',
    'incorrect_destroy_seq',
]

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

tests_info_xml = custom_target(package_dir.underscorify() + 'tests-info-xml',
                               install: true, install_dir: package_dir,
                               input: package_tests_c,
                               output: 'tests-info.xml', capture: true,
                               command: [ te_tests_info_sh,
                               meson.current_source_dir() ])

install_data([ 'package.xml', 'package.dox' ],
             install_dir: package_dir)
